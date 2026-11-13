#include "test_assets.hpp"
// Plan 32 E: complete canvas output and persistent horizon regression.
#define RR_ALLOC_COUNTER_DEFINE
#include "alloc_counter.hpp"
#include "race_caches.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
static uint64_t hash_bytes(const void* data, size_t size) {
    auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}
int main(int argc, char** argv) {
    const bool benchmark = argc == 2 && std::string(argv[1]) == "--benchmark";
    try {
        rr::Assets assets(rr::test::assets_root());
        if (!assets.exists("DATA/COURSES/NAPA.CRS")) {
            std::cout << "No imported scene corpus\n";
            return 77;
        }
        rr::SurfacePtr surface(SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888));
        check(bool(surface), SDL_GetError());
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
            SDL_CreateSoftwareRenderer(surface.get()), SDL_DestroyRenderer);
        check(bool(renderer), SDL_GetError());
        rr::Game game;
        const auto exe = assets.read("ROADRASH.EXE");
        game.depth = rr::read_depth_table(exe);
        game.palette = rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
        game.config.course = "NAPA";
        game.config.opponents = 7;
        game.config.career = true;
        game.load_course(assets, exe);
        game.restart();
        check(bool(game.assets.scene) && bool(game.assets.hud), "Canvas test needs scene and dashboard");
        for (int tick = 0; tick < 300; ++tick) game.tick(rr::control_accelerate);
        const auto snapshot = game.race().snapshot();
        size_t row = 0;
        for (int scale : {1, 2, 4})
            for (bool wide : {false, true})
                for (bool rgb : {false, true}) {
                    game.widescreen = wide ? rr::Widescreen::mode16x9 : rr::Widescreen::off;
                    const auto slices =
                        rr::project_road(game.race().route(), game.race().camera_position(),
                                         game.race().camera(), game.depth, game.logical_width());
                    game.enhancements.full_colour = rgb;
                    rr::CourseCanvas canvas;
                    const auto cold_start = std::chrono::steady_clock::now();
                    canvas.draw(renderer.get(), game, slices, scale, 1);
                    const double cold_ms = std::chrono::duration<double, std::milli>(
                                               std::chrono::steady_clock::now() - cold_start)
                                               .count();
                    const uint64_t checksum =
                        rgb ? hash_bytes(canvas.rgb.row(0), size_t(canvas.scale_w) * canvas.scale_h * 4)
                            : hash_bytes(canvas.frame.row(0), size_t(canvas.scale_w) * canvas.scale_h);
                    size_t allocations = 0;
                    std::vector<double> timings;
                    timings.reserve(benchmark ? 20 : 1);
                    // Warm the pool/mip cache before measuring complete draws, including
                    // actors, HUD, horizon and upload through the software renderer.
                    canvas.draw(renderer.get(), game, slices, scale, 1);
                    for (int sample = 0; sample < (benchmark ? 20 : 1); ++sample) {
                        const auto start = std::chrono::steady_clock::now();
                        {
                            rr::test::CountAllocs counter;
                            canvas.draw(renderer.get(), game, slices, scale, 1);
                            allocations = counter.count();
                        }
                        timings.push_back(std::chrono::duration<double, std::milli>(
                                              std::chrono::steady_clock::now() - start)
                                              .count());
                    }
                    std::sort(timings.begin(), timings.end());
                    auto percentile = [&](size_t percent) {
                        return timings[(timings.size() * percent + 99) / 100 - 1];
                    };
                    canvas.draw(renderer.get(), game, slices, scale, 4);
                    const uint64_t threaded =
                        rgb ? hash_bytes(canvas.rgb.row(0), size_t(canvas.scale_w) * canvas.scale_h * 4)
                            : hash_bytes(canvas.frame.row(0), size_t(canvas.scale_w) * canvas.scale_h);
                    check(checksum == threaded, "Canvas differs between one and four threads");
                    std::cout << "row=" << row++ << " scale=" << scale << " wide=" << wide << " rgb=" << rgb
                              << " hash=" << checksum << " allocations=" << allocations
                              << " cold_ms=" << cold_ms << " p50=" << percentile(50)
                              << " p95=" << percentile(95) << " p99=" << percentile(99) << '\n';
                    // Compare independently initialized caches with the warmed
                    // canvas at the same measured simulation state. October's
                    // native frame hashes froze old rider/terrain behavior;
                    // original parity is checked by the separate oracle tests.
                    rr::CourseCanvas cold;
                    cold.draw(renderer.get(), game, slices, scale, 1);
                    const auto cold_hash = rgb
                        ? hash_bytes(cold.rgb.row(0), size_t(cold.scale_w) * cold.scale_h * 4)
                        : hash_bytes(cold.frame.row(0), size_t(cold.scale_w) * cold.scale_h);
                    check(checksum == cold_hash, "Retained canvas differs from independent cold caches");
                    // Dashboard overlays the scene; every opaque dashboard sample and
                    // the full Hor+ margins must survive composition.
                    const auto dash = game.assets.hud->compose(
                        game.assets.hud->state(game.race(), game.player_name, game.rider_names),
                        game.assets.physics, rr::DashMode::full, scale);
                    const int xoff = (game.logical_width() - 320) / 2 * 2 * scale;
                    rr::DashLut lut;
                    auto expect = [&](int x, int y, uint32_t c) {
                        if (c == 0x01000000) return;
                        if (rgb)
                            check(canvas.rgb.row(y)[x] == (0xff000000u | (c & 0xffffffu)),
                                  "RGB dashboard/panel pixel missing");
                        else
                            check(canvas.frame.row(y)[x] == lut.index(c, game.palette),
                                  "Indexed dashboard/panel pixel missing");
                    };
                    auto dark = [](uint32_t c) {
                        return (((c >> 16 & 255) / 2) << 16) | (((c >> 8 & 255) / 2) << 8) | ((c & 255) / 2);
                    };
                    for (int y = 0; y < dash.height; ++y) {
                        for (int x = 0; x < dash.width; ++x) expect(x + xoff, y, dash.at(x, y));
                        const auto lc = dash.at(0, y), rc = dash.at(dash.width - 1, y);
                        if (lc != 0x01000000)
                            for (int x = 0; x < xoff; ++x) expect(x, y, dark(lc));
                        if (rc != 0x01000000)
                            for (int x = xoff + dash.width; x < canvas.scale_w; ++x) expect(x, y, dark(rc));
                    }
                    // No-HUD Original rendering still follows the original scene path.
                    auto hud = std::move(game.assets.hud);
                    game.enhancements.full_colour = false;
                    canvas.draw(renderer.get(), game, slices, scale, 1);
                    const uint64_t no_hud =
                        hash_bytes(canvas.frame.row(0), size_t(canvas.scale_w) * canvas.scale_h);
                    // Reconstruct without CourseCanvas caches, uploads or
                    // render workers. Check every byte rather than replacing
                    // the historical native hash with a new native hash.
                    rr::Framebuffer direct(canvas.scale_w, canvas.scale_h);
                    game.assets.scene->draw_horizon(direct, game.race().horizon_scroll(), game.logical_width());
                    auto direct_nodes = game.assets.scene->draw_nodes(
                        game.race().route(), slices, game.race().camera(), game.depth,
                        [&](const rr::RoadSlice& near, const rr::RoadSlice& far, int road,
                            int32_t ceiling, bool back) {
                            return game.rider_nodes(near, far, road, ceiling, back);
                        }, game.logical_width());
                    rr::render_nodes(direct, direct_nodes, rr::view_mode_wide(scale, game.logical_width()), 16);
                    check(no_hud == hash_bytes(direct.row(0), size_t(direct.width()) * direct.height()),
                          "Retained Original scene differs from direct unthreaded rendering");
                    game.assets.hud = std::move(hud);
                    game.enhancements.full_colour = rgb;
                    // Compare retained horizon storage to a fresh reference frame,
                    // including negative and wrapped scroll.
                    rr::Framebuffer fresh(canvas.scale_w, canvas.scale_h);
                    canvas.draw_horizon_rgb(*game.assets.scene, game.palette, 0, game.logical_width());
                    for (int32_t scroll : {0, -721, 719, INT32_MIN, INT32_MAX}) {
                        game.assets.scene->draw_horizon(fresh, scroll, game.logical_width());
                        {
                            rr::test::CountAllocs counter;
                            canvas.draw_horizon_rgb(*game.assets.scene, game.palette, scroll,
                                                    game.logical_width());
                            check(counter.count() == 0, "Horizon allocates after warmup");
                        }
                        for (int y = 0; y < canvas.scale_h; ++y)
                            for (int x = 0; x < canvas.scale_w; ++x) {
                                const auto* c = &game.palette.rgbx[4 * fresh.row(y)[x]];
                                const uint32_t expected =
                                    0xff000000u | (uint32_t(c[0]) << 16) | (uint32_t(c[1]) << 8) | c[2];
                                check(canvas.rgb.row(y)[x] == expected,
                                      "Retained horizon differs from fresh conversion");
                            }
                    }
                }
        const auto after = game.race().snapshot();
        check(after.rng == snapshot.rng && after.tick == snapshot.tick &&
                  after.fields.size() == snapshot.fields.size(),
              "Canvas changed simulation state");
        for (size_t i = 0; i < after.fields.size(); ++i)
            check(after.fields[i].name == snapshot.fields[i].name &&
                      after.fields[i].value == snapshot.fields[i].value,
                  "Canvas changed simulation fields");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
