#include "test_assets.hpp"
#include "actor_nodes.hpp"
#include <iostream>
#include <algorithm>
#include <cstdlib>
int main() {
    try {
        rr::Assets assets(rr::test::assets_root());
        if (!assets.exists("DATA/GLOBAL.RSC")) {
            std::cout << "Imported actor nodes corpus absent\n";
            return 0;
        }
        const auto exe = assets.read("ROADRASH.EXE");
        rr::ActorNodes nodes(assets, exe, true);
        const auto depths = rr::read_depth_table(exe);
        size_t populated = 0, empty = 0, pixelless = 0;
        for (size_t bank = 0; bank < 4; ++bank)
            for (size_t f = 0; f < nodes.library().graphics[bank].frames.size(); ++f) {
                const auto* texture = nodes.texture(bank, uint32_t(f), 0);
                if (texture) {
                    const auto& graphic = nodes.library().graphics[bank].frames[f];
                    if (texture->mips.empty()) {
                        // 0x004436a0 null pixel pointer (e.g. empty SHADOW.DAT
                        // frames): the node is still emitted, pixels contribute
                        // nothing. Bounds check does not apply; stability does.
                        ++pixelless;
                    } else {
                        ++populated;
                        if (texture->bounds[2] < graphic.width || texture->bounds[3] < graphic.height)
                            throw std::runtime_error("Actor page smaller than RRAN logical dimensions");
                    }
                    if (nodes.texture(bank, uint32_t(f), 0) != texture)
                        throw std::runtime_error("Actor texture ownership unstable");
                } else
                    ++empty;
            }
        const auto* stable = nodes.texture(0, 3, 0);
        for (int colour = 1; colour < 15; ++colour) nodes.texture(0, 3, colour);
        if (stable != nodes.texture(0, 3, 0))
            throw std::runtime_error("Remap insertion invalidated actor texture");
        // Working original 00446484 tests own heading, then clamps the
        // selected roll. Camera-relative yaw must not flatten a nearby rider.
        const auto& descriptor = nodes.library().playback[0].at(2)->at(0);
        for (const auto& input : {std::array<int32_t, 3>{0, 0x200000, 0x100000},
                                  std::array<int32_t, 3>{0x200000, -0x200000, -0x100000},
                                  std::array<int32_t, 3>{0x200000, 0, 0},
                                  std::array<int32_t, 3>{0x200000, 0, 0x20000}}) {
            rr::ActorOrientation orientation;
            orientation.depth = 512;
            orientation.heading = input[0];
            orientation.road_heading = input[1];
            orientation.roll = input[2];
            rr::ActorProjection projection;
            projection.depth = 512;
            projection.ground = 0;
            const auto angles = rr::actor_angles(orientation, 0);
            int32_t roll = rr::select_actor_view(descriptor, nodes.library().views[0], angles.yaw, angles.roll).roll;
            if (std::abs(int64_t(orientation.heading)) > 0xe0000)
                roll = std::clamp(roll, -0x50000, 0x50000);
            const auto actual = nodes.rider(descriptor, 0, 0, orientation, projection, {0, 150}, depths);
            const auto expected = nodes.rider(descriptor, 0, 0, orientation, projection, {0, 150}, depths, 0, &roll);
            if (actual.size() != expected.size() || actual.empty())
                throw std::runtime_error("Rider lean nodes missing");
            for (size_t i = 0; i < actual.size(); ++i)
                for (size_t k = 0; k < 4; ++k)
                    if (actual[i].view_points[k].x != expected[i].view_points[k].x ||
                        actual[i].view_points[k].y != expected[i].view_points[k].y)
                        throw std::runtime_error("Rider roll clamp differs from original heading rule");
        }
        std::cout << "four original steering/roll clamp cases match rendered body nodes\n";
        size_t drawn = 0;
        for (const auto& sequence : nodes.library().playback[0])
            if (sequence)
                for (const auto& descriptor : *sequence) {
                    rr::ActorOrientation orientation;
                    orientation.depth = 512;
                    rr::ActorProjection projection;
                    projection.depth = 512;
                    drawn += nodes.rider(descriptor, 0, 0, orientation, projection, {}, depths).size();
                }
        if (!drawn || !pixelless || !populated)
            throw std::runtime_error("Imported actor corpus not exercised");
        std::cout << populated << " owned actor pages, " << empty << " missing frames, " << pixelless
                  << " pixel-less frames, " << drawn << " descriptor nodes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
