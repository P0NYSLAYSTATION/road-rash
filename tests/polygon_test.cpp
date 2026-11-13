#include "raster.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
int main() {
    int failures = 0;
    const auto path = rr::golden::raster_fixture();
    if (!std::filesystem::exists(path)) {
        std::cerr << "SKIP missing polygon oracle\n";
        return rr::golden::skip;
    }
    // The moving Napa hillside crosses x=0. Enhanced rendering must repair
    // its concave corner identically after widescreen centering.
    {
        std::array<rr::RasterPoint, 4> a{{{195, 505}, {-176, 378}, {-252, 391}, {134, 489}}};
        auto b = a;
        for (auto& point : b) point.x += 212;
        bool af = false, bf = false;
        const bool accepted = rr::normalize_quad(a, 0, &af, true);
        if (rr::normalize_quad(b, 0, &bf, true) != accepted || af != bf) ++failures;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].x + 212 != b[i].x || a[i].y != b[i].y) {
                std::cerr << "Enhanced polygon repair depends on horizontal origin\n";
                ++failures;
            }
    }
    try {
        const auto golden = rr::golden::load(path);
        if (!rr::golden::original_executable(golden.exe) ||
            golden.frames.empty())
            throw std::runtime_error("Wrong polygon oracle");
        const auto& frame = golden.frames.front();
        for (int c = 0; c < 2048; ++c) {
            const std::string prefix = "polygon." + std::to_string(c) + ".";
            auto field = [&](const std::string& key) {
                const auto it = frame.find(prefix + key);
                if (it == frame.end() || it->second < INT32_MIN || it->second > INT32_MAX)
                    throw std::runtime_error("Missing polygon field: " + prefix + key);
                return int32_t(it->second);
            };
            std::array<rr::RasterPoint, 4> points;
            for (int i = 0; i < 4; ++i)
                points[i] = {field("input." + std::to_string(i * 2)),
                             field("input." + std::to_string(i * 2 + 1))};
            bool flip = c % 2 != 0;
            const bool accepted = rr::normalize_quad(points, uint32_t(field("flags")), &flip);
            auto check = [&](bool match, const std::string& what) {
                if (!match) {
                    if (failures < 12) std::cerr << prefix + what << '\n';
                    ++failures;
                }
            };
            check(accepted == (field("accepted") != 0), "accepted");
            check(flip == (field("flip") != 0), "flip");
            for (int i = 0; i < 4; ++i) {
                check(points[i].x == field("output." + std::to_string(i * 2)), "x" + std::to_string(i));
                check(points[i].y == field("output." + std::to_string(i * 2 + 1)), "y" + std::to_string(i));
            }
        }
        std::cout << "2048 original polygon normalizations compared; " << failures << " failures\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return failures ? 1 : 0;
}
