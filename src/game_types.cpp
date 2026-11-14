// Plan 28 Task 4: Game methods (moved unchanged from game_main.cpp).
#include "game_types.hpp"
#include <cstdlib>
#include "lifecycle.hpp"
#include "platform_files.hpp"
#include "raster_rgb.hpp"
#include "view_state.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rr {
int32_t Game::logical_width() const {
    // Single setting (review 2026-10-13 item 1); match follows the window aspect.
    return 320;
}

Camera Game::blend_camera() const {
    if (!view_ready) return race().camera();
    Camera camera = view_drawn.camera;
    const auto& route = render_route();
    // The longitudinal interpolation may still be in the preceding common or
    // branch slice. Rebase the latest absolute camera into that drawn slice,
    // rather than interpreting a branch-relative x in the common-road frame.
    camera.x += view_drawn.camera_origin - route.at(blend_camera_position() >> 8).offset;
    return camera;
}

const std::vector<RouteSample>& Game::render_route() const {
    const auto& base = race().route();
    if (!instance.ring) return base;
    if (render_base_ != &base || render_route_.size() != base.size()) {
        render_route_ = base;
        render_base_ = &base;
    }
    const size_t first = blend_camera_position() >> 8;
    const uint8_t own = race().camera_half();
    for (size_t i = first > 2 ? first - 2 : 0; i < base.size() && i < first + view_slices + 2; ++i)
        render_route_[i] = instance.ring->render_sample(int32_t(i * 256), own).value_or(base[i]);
    return render_route_;
}

void Game::load_course(const Assets& a, const std::vector<uint8_t>& exe) {
    clear_rgba_mip_cache(); // textures below are rebuilt; pointers may be reused
    exe_ = exe;
    // Plan 26 Task 3: course-scoped assets come from the shared factory.
    // Scene failure keeps the explicit flat-road fallback (no scene, no
    // terrain source); the wiring log records it.
    try {
        assets = load_race_assets(a, exe, config);
    } catch (const std::exception& e) {
        SDL_Log("%s assets unavailable: %s", config.course.c_str(), e.what());
        try {
            RaceConfig fallback = config;
            fallback.optional_scene = true;
            assets = load_race_assets(a, exe, fallback);
        } catch (const std::exception& e2) {
            SDL_Log("%s fallback failed: %s", config.course.c_str(), e2.what());
        }
    }
    upscaled_attached = false;
}

void Game::restart() {
    instance = build_race(assets, exe_, config);
    // 004163f5 / 00416401: the original runs two zero-input driver
    // steps before the starting-grid fade/countdown is visible. These settle
    // rider placement against the loaded ring and publish the chase camera.
    // Deferring them until GO exposed the placement correction as a camera snap.
    race().step(0);
    race().step(0);
    countdown = countdown_ticks;
    reset_view_history();
    render_base_ = nullptr;
    render_route_.clear();
    const RaceWiring w = instance.race->wiring();
    SDL_Log("race wiring: terrain=%d traffic=%d pedestrians=%d ring=%d crash_library=%d ride_library=%d "
            "nitro=%d finish=%d personalities=%d",
            int(w.terrain), int(w.traffic), int(w.pedestrians), int(w.ring), int(w.crash_library),
            int(w.ride_library), int(w.nitro), int(w.finish), int(w.personalities));
}

std::vector<DrawNode> Game::rider_nodes(const RoadSlice& near, const RoadSlice& far, int road,
                                        int32_t ceiling, bool back) const {
    struct Visible {
        int32_t position;
        size_t index;
        bool car, body = false, pedestrian = false;
    };
    std::vector<Visible> visible;
    // Plan 21 Task 2: sample membership uses live positions (which slice
    // an actor is on); the drawn quad uses the blended pose. With
    // interpolation off the blend is alpha=65536 = live, exactly.
    auto rider_pose = [&](size_t i, bool body) -> const ActorPose* {
        return view_ready ? blend_pose(uint32_t(body ? 1000 + i : i + 1)) : nullptr;
    };
    for (size_t i = 0; i < race().riders().size(); ++i) {
        const auto& rd = race().riders()[i];
        auto include = [&](const Bike& actor, int route, const ActorAnimationState& state, bool body) {
            const auto pos = uint32_t(actor.i32(0x1c));
            if ((pos >> 8) != near.sample || ((pos & 255) >= 128) != back || !state.selected) return;
            const auto half = body ? rd.crash_actor.slice_half : rd.slice_half;
            if (near.count == 2 && half.value_or(uint8_t(route)) != road) return;
            if (const auto* pose = rider_pose(i, body))
                visible.push_back({int32_t(pose->position_q8), i, false, body});
            else
                visible.push_back({actor.i32(0x1c), i, false, body});
        };
        if (rd.crash_active) {
            include(rd.bike, rd.route, rd.crash_bike_animation, false);
            include(rd.crash_actor.motion, rd.crash_actor.route, rd.crash_actor.animation, true);
        } else
            include(rd.bike, rd.route, rd.ride_animation, false);
    }
    if (instance.traffic && road == near.own)
        for (size_t i = 0; i < instance.traffic->cars().size(); ++i) {
            const auto& car = instance.traffic->cars()[i];
            const auto pos = uint32_t(car.position);
            if (car.active && (pos >> 8) == near.sample && ((pos & 255) >= 128) == back) {
                if (view_ready) {
                    if (const auto* pose = blend_pose(uint32_t(2000 + i)))
                        visible.push_back({int32_t(pose->position_q8), i, true});
                    else
                        visible.push_back({car.position, i, true});
                } else
                    visible.push_back({car.position, i, true});
            }
        }
    if (instance.pedestrians && road == near.own)
        for (size_t i = 0; i < instance.pedestrians->pool().size(); ++i) {
            const auto& p = instance.pedestrians->pool()[i];
            const auto pos = uint32_t(p.body.bike.i32(0x1c));
            if (p.active && (pos >> 8) == near.sample && ((pos & 255) >= 128) == back)
                visible.push_back({int32_t(pos), i, false, false, true});
        }
    std::stable_sort(visible.begin(), visible.end(),
                     [](const Visible& a, const Visible& b) { return a.position > b.position; });
    std::vector<DrawNode> output;
    for (const auto& item : visible) {
        const size_t i = item.index;
        if (item.pedestrian) {
            if (auto node = pedestrian_node(instance.pedestrians->pool()[i], near, far, road,
                                            render_route().at(near.sample), blend_camera(), depth, ceiling,
                                            logical_width()))
                output.push_back(*node);
            continue;
        }
        if (item.car) {
            const auto& car = instance.traffic->cars()[i];
            int32_t bpos = 0, blat = 0;
            bool use_blend = false;
            if (view_ready) {
                if (const auto* pose = blend_pose(uint32_t(2000 + i))) {
                    bpos = int32_t(pose->position_q8);
                    blat = pose->lateral_q8;
                    use_blend = true;
                }
            }
            if (auto node = traffic_car_node(instance.traffic->library().vehicles().at(car.vehicle), car,
                                             near, far, road, render_route().at(near.sample), blend_camera(),
                                             depth, int32_t(blend_camera_position()), ceiling,
                                             logical_width(), bpos, blat, use_blend))
                output.push_back(*node);
            continue;
        }
        const auto& rd = race().riders()[i];
        const auto& bike = item.body ? rd.crash_actor.motion : rd.bike;
        auto sample = race().route_for(item.body ? rd.crash_actor.route : rd.route).at(near.sample);
        if (instance.ring) {
            const auto half = item.body ? rd.crash_actor.slice_half : rd.slice_half;
            sample = instance.ring->render_sample(int32_t(near.sample * 256), half.value_or(0)).value_or(sample);
        }
        const auto& a = near.road[road];
        const auto& b = far.road[road];
        const auto camera = blend_camera();
        // Plan 21 Task 2: drawn position/lateral/ground/heading/roll come
        // from the blended pose; sample membership above stays live.
        int32_t pos_q8 = bike.i32(0x1c), lat_q8 = bike.i32(0x28), gnd_q8 = bike.i32(0x24),
                head = bike.i32(0x5c), roll = bike.i32(0x60);
        if (view_ready) {
            if (const auto* pose = rider_pose(i, item.body)) {
                pos_q8 = int32_t(pose->position_q8);
                lat_q8 = pose->lateral_q8;
                gnd_q8 = pose->height_q8;
                head = pose->heading;
                roll = pose->roll;
            }
        }
        ActorOrientation orientation;
        orientation.depth = near.depth;
        orientation.position = pos_q8;
        orientation.road_x = a.road_x + camera.x;
        orientation.lateral_q8 = lat_q8 - sample.offset * 256;
        orientation.road_heading = a.heading;
        orientation.heading = head;
        orientation.roll = roll;
        orientation.rider = !rd.crash_active || item.body;
        ActorProjection projection;
        projection.road_x = orientation.road_x;
        projection.far_x = b.road_x + camera.x;
        projection.road_y = a.world_y;
        projection.far_y = b.world_y;
        projection.lateral = orientation.lateral_q8 / 256;
        projection.ground = gnd_q8;
        projection.depth = near.depth;
        projection.fraction = uint8_t(orientation.position);
        projection.occlusion = ceiling;
        const auto& state = rd.crash_active ? (item.body ? rd.crash_actor.animation : rd.crash_bike_animation)
                                            : rd.ride_animation;
        const size_t bank = rd.cop ? 1 : 0; // 0x00443610: the cop sheets (bank 1) draw with the cop palette
        const auto selected = *state.selected;
        const auto& descriptor =
            assets.actors->library().playback[bank].at(size_t(selected[0]))->at(size_t(selected[1]));
        const auto nodes = assets.actors->rider(descriptor, bank, rd.colour, orientation, projection, camera,
                                                depth, bike.i32(0x20), nullptr, logical_width());
        output.insert(output.end(), nodes.begin(), nodes.end());
    }
    return output;
}

// Race owns the original scheduled animation block for every rider, including
// contact reactions, AI poses and police. Rendering reads that same block.
void Game::tick(uint32_t controls) {
    if (countdown > 0)
        --countdown; // race clock starts after the countdown
    else
        race().step(controls);
    if (sound) {
        for (const auto& e : race().take_sound_events()) sound->post(e);
        if (instance.traffic)
            for (const auto& e : instance.traffic->take_sound_events()) sound->post(e);
        sound->update(race());
    } else {
        update_engine_rpm(race().riders()[0].bike); // 0x00434980 without sound
    }
}
void Game::present_online(const RaceInstance& source, int seat) {
    copy_race_presentation(instance, source, seat);
    render_base_ = nullptr;
}
std::string pref_directory() {
    if (const char* o = SDL_getenv("RR_PREF_OVERRIDE")) return o;
    if (std::string portable = portable_data_directory(); !portable.empty())
        return portable; // plan 33b Task 5
    char* p = SDL_GetPrefPath("RoadRashNative", "RoadRash");
    if (!p) throw std::runtime_error(SDL_GetError());
    std::string s = p;
    SDL_free(p);
    return s;
}
} // namespace rr
