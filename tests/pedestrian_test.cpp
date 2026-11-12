#include <cstdlib>
#include <algorithm>
#include "pedestrians.hpp"
#include "traffic.hpp"
#include "track_ring.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <map>

// Pedestrians (plan 24 Task 4) on the imported CITY course: DZHR actor
// records, spawning through the ring, crossing, and both hit outcomes of
// 0x00440760. Skips without the disc corpus.
namespace {
struct Fixture {
    rr::Assets assets{
        (std::getenv("RR_ASSETS") ? std::getenv("RR_ASSETS") : RR_SOURCE_DIR "/../cdrom/ROADRASH")};
    std::vector<uint8_t> exe;
    rr::PhysicsTables physics;
    std::vector<rr::BikeSpec> specs;
    rr::DepthTable depths;
    rr::Course course;
    std::unique_ptr<rr::CourseScene> scene;
    std::unique_ptr<rr::VehicleLibrary> vehicles;
    explicit Fixture(const std::string& name) {
        exe = assets.read("ROADRASH.EXE");
        physics = rr::read_physics_tables(exe);
        depths = rr::read_depth_table(exe);
        specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        course = rr::read_course(assets.read("DATA/COURSES/" + name + ".CRS"));
        scene = std::make_unique<rr::CourseScene>(assets, exe, course, name, 1, 0);
        vehicles = std::make_unique<rr::VehicleLibrary>(assets.read("DATA/CARS/" + name + ".CAR"), exe);
    }
    std::unique_ptr<rr::PedestrianSystem> pedestrians() const {
        return std::make_unique<rr::PedestrianSystem>(
            [this](const rr::RouteSample& s) -> const std::vector<rr::DzhrActor>& {
                return scene->dzhr_actors(s.section, s.local);
            },
            [this](const rr::RouteSample& s, uint8_t selector) {
                return scene->pedestrian_look(s.section, s.local, selector);
            },
            exe, [this](const std::vector<rr::RouteSample>& route, size_t s, uint8_t selector,
                           const rr::PedestrianLook& look) {
                return scene->pedestrian_graphics_valid(route, s, selector, look);
            }, [this](const std::vector<rr::RouteSample>& route, size_t s, uint8_t selector) {
                return scene->pedestrian_look(route, s, selector);
            });
    }
    std::unique_ptr<rr::Race> race(int opponents) const {
        auto r = std::make_unique<rr::Race>(
            rr::make_route(course, false), rr::make_route(course, true), physics, specs.at(0), 90,
            rr::original_opponents(exe, specs, 0, opponents), 0, 0, rr::read_personalities(exe),
            exe); // personality speed tables (replan resets from them)
        r->set_terrain_source([this](const rr::RouteSample& s) -> const rr::TerrainSlice& {
            return scene->terrain(s.section, s.local);
        });
        return r;
    }
};
void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

void family_buffer_tail() {
    Fixture hiway("HIWAY");
    // At forward cursor127 slot3 changes180->182. 182 writes112744
    // bytes; selector3's cached RRAN starts146528, in the untouched tail.
    const auto* tail = hiway.scene->pedestrian_look(0, 60, 195);
    const auto* overwritten = hiway.scene->pedestrian_look(0, 60, 194);
    check(tail && overwritten, "Hiway initial pedestrian graphics missing");
    check(tail->graphics_offset == 146528 && overwritten->graphics_offset == 112172,
          "Hiway RRAN offsets differ from the measured family-buffer layout");
    // Query local122 (+33 view slices-28 bias) reaches event127.
    check(hiway.scene->pedestrian_graphics_valid(0, 122, 195, *tail),
          "a shorter family replacement must preserve the untouched RRAN tail");
    check(!hiway.scene->pedestrian_graphics_valid(0, 122, 194, *overwritten),
          "replacement bytes must invalidate an overwritten RRAN header");
    const auto route = rr::make_route(hiway.course, false);
    const auto* inherited = hiway.scene->pedestrian_look(route, 902, 194);
    check(inherited && inherited == hiway.scene->pedestrian_look(0, 894, 194),
          "a section without a slot3 load must retain the incoming family");
    check(hiway.scene->pedestrian_graphics_valid(route, 902, 194, *inherited),
          "inherited family graphics must remain valid across a section boundary");
}

void retained_road_slice() {
    Fixture hiway("HIWAY");
    auto race = hiway.race(0);
    race->set_live_schedule(true);
    race->set_ticks(642);
    race->riders()[0].half_mirror.position = 25644;
    race->riders()[0].frozen = true;
    rr::TrackRing ring(hiway.course, 0);
    race->set_track_ring(&ring);
    auto pedestrians = hiway.pedestrians();
    race->set_pedestrians(pedestrians.get());
    auto& p = pedestrians->pool()[0];
    pedestrians->apply_template(p.body.bike);
    p.active = true;
    p.side = 1;
    p.last_tick = 598;
    p.next_tick = 643;
    p.anchor = 12800;
    p.body.sample = p.body.previous_sample = 50;
    p.body.bike.i32(0x1c) = 12911;
    p.body.bike.i32(0x28) = -81920;
    p.body.bike.u8(0xc0) |= 0x10;
    race->countdown_step();
    check(p.body.bike.i32(0x1c) == 12930 && p.body.bike.i32(0x128) == 4187616,
          "last pedestrian move must read curvature from the reused physical road slot");
    check(p.pending_remove, "the last move still precedes deferred window removal");
}

void initial_pedestrian_overlap(const Fixture& f) {
    auto race = f.race(0);
    race->set_live_schedule(true);
    race->set_demo_hold(true);
    auto& rider = race->riders()[0];
    rider.sched_next = rider.half_sched_next = 10000;
    static const std::vector<rr::DzhrActor> empty;
    rr::PedestrianSystem system(
        [](const rr::RouteSample&) -> const std::vector<rr::DzhrActor>& { return empty; },
        [](const rr::RouteSample&, uint8_t) -> const rr::PedestrianLook* { return nullptr; }, f.exe);
    race->set_pedestrians(&system);
    for (int i = 0; i < 2; ++i) {
        auto& p = system.pool()[size_t(i)];
        system.apply_template(p.body.bike);
        p.active = true;
        p.order = uint32_t(i);
        p.type = i ? 4 : 0;
        p.side = 3;
        p.next_tick = 1;
        p.last_tick = -60;
        p.body.bike.u8(0xc0) |= 0x10;
        p.body.bike.i32(0x1c) = p.anchor = i ? 13312 : 13307;
        p.body.sample = p.body.previous_sample = size_t(p.anchor >> 8);
        p.body.bike.i32(0x28) = p.anchor_lateral = i ? 81920 : 89600;
    }
    const auto rng = race->rng_state();
    race->countdown_step();
    // Original Hiway tick1: two already-overlapping pedestrians separate
    // by (4096+4096-7680+2048)/2+3 =1283 each, before green.
    check(system.pool()[0].body.bike.i32(0x28) == 90883 && system.pool()[1].body.bike.i32(0x28) == 80637,
          "initial pedestrian overlap must separate during pre-green contacts");
    check(system.pool()[0].anchor_lateral == 89600 && system.pool()[1].anchor_lateral == 81920,
          "contact separation must preserve the allocation anchors");
    check(race->rng_state() == rng, "unloaded pedestrian separation must consume no random draws");
}

void recycled_pedestrian(const Fixture& f) {
    auto race = f.race(0);
    auto system = f.pedestrians();
    race->set_pedestrians(system.get());
    auto& p = system->pool()[0];
    p.body.bike.i32(0x108) = 73;
    p.body.bike.i32(0x10c) = 160;
    p.body.bike.i32(0x104) = 999;
    p.body.bike.i32(0xec) = 999;
    p.body.bike.i32(0x128) = 999;
    p.body.bike.i32(0x18) = 48;
    p.lane = -2;
    rr::Course course;
    course.root = 20;
    rr::CourseSection section;
    section.samples.resize(300, rr::PathSample{0, 0, 500, 500, 512, 512});
    section.scenery_events.push_back({0x80000000u | (50u << 24) | (15u << 20) | (194u << 8), 0});
    course.sections.push_back(section);
    course.nodes.emplace(20u, rr::CourseNode{20, 0, 40, 0, 0, 300});
    course.nodes.emplace(40u, rr::CourseNode{40, 4});
    rr::TrackRing ring(course, 0);
    race->set_track_ring(&ring);
    check(p.active && system->spawned() == 1, "synthetic ring must reuse the first pedestrian slot");
    check(p.body.bike.i32(0x108) == 73 && p.body.bike.i32(0x10c) == 160 && p.lane == -2,
          "recycled pedestrians retain previous velocities and crossing lane until movement");
    check(p.body.bike.i32(0x104) == 0 && p.body.bike.i32(0xec) == 0 && p.body.bike.i32(0x128) == 0 &&
              p.body.bike.i32(0x18) == 0,
          "allocation still resets current velocity, speed, heading and elapsed time");
    auto& later = system->pool()[1];
    later.active = true;
    later.body.bike.i32(0x10c) = -77;
    system->recycle(p);
    system->recycle(later);
    rr::TrackRing next_ring(course, 0);
    race->set_track_ring(&next_ring);
    check(later.active && !p.active && later.body.bike.i32(0x10c) == -77,
          "allocation reuses the most recently returned slot and its retained motion");
}

void pedestrian_statics(const Fixture& f) {
    for (bool marking : {false, true})
        for (bool overlaps : {false, true}) {
            auto race = f.race(0);
            race->set_live_schedule(true);
            race->riders()[0].sched_next = race->riders()[0].half_sched_next = 1000;
            rr::PedestrianLook look;
            look.bank.resize(6);
            rr::ActorAnimationFrame held;
            held.duration = 127;
            held.control = 1;
            look.bank[1] = look.bank[2] = rr::ActorSequence{held};
            static const std::vector<rr::DzhrActor> empty;
            rr::PedestrianSystem system(
                [](const rr::RouteSample&) -> const std::vector<rr::DzhrActor>& { return empty; },
                [&](const rr::RouteSample&, uint8_t) { return &look; }, f.exe);
            race->set_pedestrians(&system);
            auto& p = system.pool()[0];
            system.apply_template(p.body.bike);
            p.active = true;
            p.look = &look;
            p.type = 4;
            p.next_tick = 1000;
            p.body.sample = p.body.previous_sample = 55;
            p.body.bike.i32(0x1c) = p.anchor = 55 * 256 + 128;
            p.body.bike.i32(0x28) = p.anchor_lateral = overlaps ? 0 : 100000;
            rr::AnimationFrame frame;
            frame.boxes = {{-30000, -10000, 30000, 10000}};
            race->set_static_frames([&](uint32_t, uint32_t, uint8_t, bool) { return &frame; });
            rr::RingStatics slice;
            if (marking) {
                slice.billboards = 1;
                slice.billboard[0] = {10, 128, 0};
            } else {
                slice.borr_count = 1;
                slice.borr_selector = 1;
            }
            const auto& s = race->route()[55];
            race->ring_statics(55, s.section, s.local, slice);
            race->countdown_step(); // prepare collision boxes for the next step
            p.last_tick = 1;
            p.next_tick = 2;
            race->countdown_step();
            check(p.state == (overlaps ? 3 : 0),
                  "pedestrian callback must receive overlapping scenery and marking contacts only");
        }
}

void roadside_pool(const Fixture& f) {
    auto race = f.race(0);
    race->set_live_schedule(true);
    rr::TrackRing ring(f.course, 0);
    race->set_track_ring(&ring);
    auto& rider = race->riders()[0];
    rider.sched_next = rider.half_sched_next = 10000;
    const int32_t sample = ((ring.front_q8() - 0x5000) >> 8) + 5;
    const auto route = rr::make_route(f.course, false);
    const auto& s = route[size_t(sample)];
    rr::RingStatics slots;
    slots.borr_count = 3;
    slots.borr[0] = s.offset - s.path.left;
    slots.borr[1] = s.offset;
    slots.borr[2] = s.offset + s.path.right;
    race->ring_statics(sample, s.section, s.local, slots);
    auto count = [&] {
        return std::count_if(race->roadside_nodes().begin(), race->roadside_nodes().end(),
                              [](const auto& n) { return n.active; });
    };
    check(count() == 1, "only strict interior scenery slots create scheduler nodes");
    check(race->roadside_nodes()[0].position == sample * 256 + 0x80,
          "in-road scenery node uses the original mid-slice position");
    for (int i = 0; i < 25; ++i) race->ring_statics(sample, s.section, s.local, slots);
    check(count() == 20, "the original scenery scheduler pool has twenty slots");
    const auto rng = race->rng_state();
    race->countdown_step();
    check(count() == 20 && race->roadside_nodes()[0].next_tick == 21,
          "null scenery callbacks preserve their twenty-tick scheduling interval");
    for (int i = 0; i < 128; ++i) ring.advance(256, [] { return 0u; }, {});
    while (race->ticks() < 21) race->countdown_step();
    check(count() == 20 && race->roadside_nodes()[0].pending_remove,
          "expired scenery nodes remain linked through their last scheduled tick");
    race->countdown_step();
    check(count() == 0, "marked scenery nodes unlink on the following tick");
    check(race->rng_state() == rng, "null scenery scheduler nodes must consume no random draws");
}

// Graphics residency is independent of the cached animation-table words.
void graphics_replacement(const Fixture& f) {
    auto race = f.race(0);
    rr::TrackRing ring(f.course, 0);
    race->set_track_ring(&ring);
    rr::PedestrianLook old_look, new_look;
    old_look.bank.resize(6);
    new_look.bank.resize(6);
    rr::ActorAnimationFrame held;
    held.duration = 127;
    held.control = 1;
    old_look.bank[0] = rr::ActorSequence{held};
    new_look.bank[5] = rr::ActorSequence{held};
    bool replaced = false;
    static const std::vector<rr::DzhrActor> empty;
    rr::PedestrianSystem system(
        [](const rr::RouteSample&) -> const std::vector<rr::DzhrActor>& { return empty; },
        [&](const rr::RouteSample&, uint8_t) { return replaced ? &new_look : &old_look; }, f.exe);
    race->set_pedestrians(&system);
    auto& p = system.pool()[0];
    p.active = true;
    p.look = &old_look;
    p.body.sample = size_t((ring.front_q8() - 0x5000) >> 8) + 40;
    p.body.previous_sample = p.body.sample;
    p.body.bike.i32(0x1c) = p.anchor = int32_t(p.body.sample * 256);
    system.apply_template(p.body.bike);
    p.body.bike.u8(0xc0) |= 0x10;
    p.next_tick = 1;
    race->countdown_step();
    check(p.loaded() && p.has(0), "resident graphics start loaded");
    replaced = true;
    p.next_tick = 2;
    race->countdown_step();
    check(!p.loaded() && p.look == &old_look && p.has(0),
          "eviction clears graphics validity while retaining the old bank words");
    check(!p.pre_draw_resources[0] && p.pre_draw_resources[1],
          "pre-draw checkpoint records eviction before any lazy reload");
    p.body.sample = size_t((ring.front_q8() - 0x5000) >> 8) + 5;
    p.body.previous_sample = p.body.sample;
    p.body.bike.i32(0x1c) = p.anchor = int32_t(p.body.sample * 256);
    p.next_tick = 1000;
    race->countdown_step();
    check(p.loaded() && p.look == &new_look && !p.has(0) && p.has(5),
          "drawing an evicted actor reloads the replacement family");
    check(!p.pre_draw_resources[0], "lazy reload does not rewrite the pre-draw checkpoint");
}

// The scheduler reads the ring camera before the player driver updates it.
// camera_position() already follows a moved bike's +0x138, so using that
// derived camera here gives an interval one larger at this boundary.
void scheduler_camera(const Fixture& f) {
    auto race = f.race(0);
    rr::TrackRing ring(f.course, 0);
    race->set_track_ring(&ring);
    auto pedestrians = f.pedestrians();
    race->set_pedestrians(pedestrians.get());
    const int32_t ring_camera = ring.front_q8() - 0x5000;
    auto& p = pedestrians->pool()[0];
    p.active = true;
    p.type = 10;
    p.next_tick = 0;
    p.last_tick = -1;
    auto& b = p.body.bike;
    pedestrians->apply_template(b);
    b.i32(0x1c) = ring_camera - 10 * 256 - 1;
    p.anchor = b.i32(0x1c);
    p.body.sample = p.body.previous_sample = size_t(b.i32(0x1c) >> 8);
    b.i32(0x28) = race->route()[p.body.sample].offset * 256;
    // Give the actor an artificial vertical impulse: type 0x20 clears it
    // after sampling the terrain, rather than taking the bike bounce path.
    b.i32(0x20) = 123;
    b.i32(0xf4) = 300;
    race->riders()[0].bike.i32(0x138) += 256;
    race->countdown_step();
    check(b.i32(0x14) == 11 && p.next_tick == 12, "pedestrian cadence must use pre-driver ring camera");
    check(b.i32(0x20) == 0 && b.i32(0xf4) == 0 && b.i32(0x160) == 74,
          "pedestrian vertical branch must stay grounded");
    // Marked nodes remain observable for the teardown tick, but no longer
    // qualify for contact, and release their slot at the next prepass.
    b.i32(0x1c) = ring.back_q8() - 256;
    p.anchor = b.i32(0x1c);
    p.next_tick = 2;
    p.body.sample = p.body.previous_sample = size_t(b.i32(0x1c) >> 8);
    race->countdown_step();
    check(p.active && p.pending_remove && (b.u8(0xc0) & 4) == 0, "pedestrian teardown must defer unlinking");
    race->countdown_step();
    check(!p.active && !p.pending_remove, "pedestrian prepass must release marked slot");
    // Nulled-callback solo replay keeps the previously receipted lifetime:
    // the disabled frozen node is absent from the same step's node list.
    p.active = true;
    p.last_tick = 3;
    p.next_tick = 4;
    race->set_freeze_actors(true);
    race->countdown_step();
    check(!p.active && !p.pending_remove, "frozen replay removal must stay immediate");
}

struct Summary {
    int spawned = 0, max_active = 0, crossers = 0, knocked = 0;
    int64_t travel = 0;
    uint32_t rng = 0;
};
Summary natural_race(const Fixture& f, const char* frame_path) {
    auto race = f.race(7);
    auto pedestrians = f.pedestrians();
    rr::TrafficSystem traffic(*f.vehicles, f.exe);
    traffic.configure(6);
    race->set_traffic(&traffic);
    race->set_pedestrians(pedestrians.get());
    race->set_demo_hold(true, 95, 0, 0, 2, {7, 3000, 3000, 3000}); // the player drives as the AI does
    Summary out;
    std::map<uint16_t, int32_t> start_lateral;
    std::map<uint16_t, int32_t> furthest;
    bool saved = false;
    // Pre-green countdown: the AI thinks (replan, first targets) before the
    // race starts, as in the original (the solo captures hold 59 ticks).
    for (int tick = 0; tick < 59; ++tick) race->countdown_step();
    for (int tick = 0; tick < 12000 && !race->finished(); ++tick) {
        race->step(0);
        if (std::getenv("RR_PED_DEBUG") && tick % 600 == 0) {
            const auto& b = race->riders()[0].brain;
            std::cerr << "tick " << tick << " pos " << race->riders()[0].bike.i32(0x1c) << " speed "
                      << race->riders()[0].bike.i32(0xec) << " state " << b.state << " tspd "
                      << b.target_speed << " tgt " << b.target_rider << " latched " << b.overtake_latched
                      << "\n";
        }
        int active = 0;
        for (const auto& p : pedestrians->pool()) {
            if (!p.active) continue;
            ++active;
            const int32_t lateral = p.body.bike.i32(0x28);
            if (!start_lateral.count(p.serial)) start_lateral[p.serial] = lateral;
            furthest[p.serial] = std::max(furthest[p.serial], std::abs(lateral - start_lateral[p.serial]));
            if (p.state == 2 && p.look) ++out.knocked;
            const int32_t ahead = p.body.bike.i32(0x1c) - int32_t(race->camera_position());
            if (frame_path && !saved && p.state == 1 && p.look && ahead > 6 * 256 && ahead < 14 * 256) {
                saved = true;
                rr::Framebuffer fb(640, 480);
                f.scene->draw_horizon(fb, race->horizon_scroll());
                const auto& route = race->route();
                const auto camera = race->camera();
                const auto slices = rr::project_road(route, race->camera_position(), camera, f.depths);
                auto actors = [&](const rr::RoadSlice& near, const rr::RoadSlice& far, int road,
                                  int32_t ceiling, bool back) {
                    std::vector<rr::DrawNode> nodes;
                    if (road != near.own) return nodes;
                    for (const auto& q : pedestrians->pool()) {
                        const uint32_t pos = uint32_t(q.body.bike.i32(0x1c));
                        if (!q.active || (pos >> 8) != near.sample || ((pos & 255) >= 128) != back) continue;
                        if (auto node = rr::pedestrian_node(q, near, far, road, route.at(near.sample), camera,
                                                            f.depths, ceiling))
                            nodes.push_back(*node);
                    }
                    return nodes;
                };
                rr::render_nodes(fb, f.scene->draw_nodes(route, slices, camera, f.depths, actors),
                                 rr::view_mode(true), 16);
                std::vector<uint8_t> rgba(640 * 480 * 4);
                fb.to_rgba(rr::Palette::from_raw(f.assets.read("DATA/PALETTE.RAW")), rgba.data(), 640 * 4);
                SDL_Surface* surface =
                    SDL_CreateSurfaceFrom(640, 480, SDL_PIXELFORMAT_RGBA32, rgba.data(), 640 * 4);
                if (!surface || !SDL_SaveBMP(surface, frame_path)) throw std::runtime_error(SDL_GetError());
                SDL_DestroySurface(surface);
            }
        }
        check(active <= int(rr::PedestrianSystem::pool_size), "pedestrian pool exceeds 15");
        out.max_active = std::max(out.max_active, active);
    }
    out.spawned = pedestrians->spawned();
    for (const auto& [serial, distance] : furthest)
        if (distance > 250 * 256) {
            ++out.crossers;
            out.travel += distance;
        }
    out.rng = race->rng_state();
    return out;
}

// A pedestrian staged just ahead of the player in its lane, kept still.
rr::Pedestrian& stage(const Fixture& f, rr::Race& race, rr::PedestrianSystem& system) {
    const auto& route = race.route();
    const rr::PedestrianLook* look = nullptr;
    for (size_t s = 0; s < route.size() && !look; ++s)
        for (const auto& record : f.scene->dzhr_actors(route[s].section, route[s].local))
            if ((record.word0 & 15) < 0xb) {
                look = f.scene->pedestrian_look(route[s].section, route[s].local, uint8_t(record.word0 >> 8));
                if (look) break;
            }
    check(look != nullptr, "no CITY pedestrian look");
    auto& p = system.pool()[0];
    p = rr::Pedestrian{};
    p.active = true;
    p.look = look;
    p.type = 0;
    p.state = 0;
    system.apply_template(p.body.bike);
    auto& rider = race.riders()[0];
    auto& q = p.body.bike;
    q.u8(0xc0) |= 0x10;
    q.i32(0x1c) = rider.bike.i32(0x1c) + 0x30;
    q.i32(0x28) = rider.bike.i32(0x28);
    for (int k = 0; k < 5; ++k) q.i32(0x30 + 4 * k) = q.i32(0x1c + 4 * k);
    p.body.route = rider.route;
    p.body.sample = p.body.previous_sample = size_t(q.i32(0x1c)) >> 8;
    p.anchor = q.i32(0x1c);
    p.next_tick = 1 << 30; // no think: it stands where it was put
    rr::set_animation(p.animation, look->bank, 1, 0);
    return p;
}
void staged_hit(const Fixture& f, int32_t speed, int expected_state, bool bump) {
    auto race = f.race(0);
    auto system = f.pedestrians();
    race->set_pedestrians(system.get());
    // Fill the ring first so staged slot 0 is not reused by a natural spawn.
    race->step(0);
    for (auto& p : system->pool()) p.active = false;
    auto& rider = race->riders()[0];
    auto& p = stage(f, *race, *system);
    rider.bike.i32(0xec) = speed;
    rider.bike.i32(0x108) = speed;
    const int32_t height = rider.bike.i32(0x20);
    bool hit = false;
    int32_t bumped = 0;
    for (int t = 0; t < 120 && !hit; ++t) {
        race->step(0);
        if (p.state != 0) {
            hit = true;
            bumped = rider.bike.i32(0x20) - height;
        }
    }
    check(hit, "staged pedestrian was never hit");
    if (p.state != expected_state)
        std::cerr << "speed " << speed << " state " << p.state << " anim " << int(p.animation.animation)
                  << " rider speed now " << rider.bike.i32(0xec) << " bump " << bumped << "\n";
    check(p.state == expected_state, "pedestrian hit state");
    if (expected_state == 2) check(p.animation.animation == 3, "knocked pedestrian plays animation 3");
    check(rider.bike.i32(0x2f4) == 0, "a pedestrian never knocks the rider off");
    (void)bump;
    (void)bumped;
}
} // namespace

int main(int argc, char** argv) {
    try {
        rr::Assets probe(
            (std::getenv("RR_ASSETS") ? std::getenv("RR_ASSETS") : RR_SOURCE_DIR "/../cdrom/ROADRASH"));
        if (!probe.exists("DATA/COURSES/CITY.CRS")) {
            std::cout << "No imported course corpus\n";
            return 0;
        }
        if (!SDL_Init(0)) std::cerr << SDL_GetError() << '\n';
        family_buffer_tail();
        retained_road_slice();
        Fixture city("CITY");
        recycled_pedestrian(city);
        pedestrian_statics(city);
        roadside_pool(city);
        initial_pedestrian_overlap(city);
        graphics_replacement(city);
        scheduler_camera(city);
        // DZHR actor records survive placement (0x00451fc0 gate, level 1).
        size_t pedestrians = 0, cops = 0, looks = 0;
        for (size_t s = 0; s < city.course.sections.size(); ++s)
            for (size_t j = 0; j < city.course.sections[s].samples.size(); ++j)
                for (const auto& record : city.scene->dzhr_actors(uint32_t(s), uint32_t(j))) {
                    if ((record.word0 & 15) < 0xb) {
                        ++pedestrians;
                        looks += city.scene->pedestrian_look(uint32_t(s), uint32_t(j),
                                                             uint8_t(record.word0 >> 8)) != nullptr;
                    } else
                        ++cops;
                }
        std::cout << "CITY level 1: " << pedestrians << " pedestrian and " << cops << " police records, "
                  << looks << " with artwork\n";
        check(pedestrians > 0 && looks > 0, "CITY has no placed pedestrians");
        // Plan 24 Task 5 pedestrian counterpart: every course keeps DZHR
        // actor records through the level-1 gate wherever they occur.
        for (const char* other : {"HIWAY", "MEDLY", "NAPA", "CANYN"}) {
            Fixture f(other);
            size_t op = 0, oc = 0;
            for (size_t s = 0; s < f.course.sections.size(); ++s)
                for (size_t j = 0; j < f.course.sections[s].samples.size(); ++j)
                    for (const auto& record : f.scene->dzhr_actors(uint32_t(s), uint32_t(j)))
                        ((record.word0 & 15) < 0xb) ? ++op : ++oc;
            std::cout << other << " level 1: " << op << " pedestrian and " << oc << " police records\n";
            if (op == 0) throw std::runtime_error(std::string(other) + " has no placed pedestrians");
        }
        const auto a = natural_race(city, argc > 1 ? argv[1] : nullptr);
        std::cout << "natural race: " << a.spawned << " spawned, at most " << a.max_active << " active, "
                  << a.crossers << " crossed more than a lane, " << a.knocked
                  << " pedestrian-ticks knocked down\n";
        check(a.spawned > 0, "no pedestrian spawned");
        check(a.crossers > 0, "no pedestrian crossed the road");
        const auto b = natural_race(city, nullptr);
        check(a.spawned == b.spawned && a.crossers == b.crossers && a.travel == b.travel && a.rng == b.rng,
              "pedestrians are not deterministic");
        // 0x00440760: a fast rider knocks the pedestrian down (state 2,
        // animation 3); a slow one only makes it stagger (state 3).
        staged_hit(city, 12000, 2, true);
        staged_hit(city, 800, 3, false); // 0x00440760: knocked above |speed| 1000
        std::cout << "staged hits: fast -> knocked down, slow -> stagger\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "pedestrian: " << e.what() << '\n';
        return 1;
    }
}
