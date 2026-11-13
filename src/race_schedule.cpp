#include "race.hpp"
#include "pedestrians.hpp"
#include "track_ring.hpp"
#include "traffic.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <cstdio>
#include <cstdlib>

// Live rider scheduler (plan 10 Task 6): 0x00438170(0) walks the object list
// and 0x00437e60 runs each due node. Each function names the routine it
// ports. Riders, pedestrians, cars and intersection nodes share the live
// position-ordered scheduler. Remaining route limitation:
// - the parked virtual walk follows the rider's own route at splits, where
//   the original takes the split node's byte +0x18;
namespace rr {
namespace {
int32_t sar(int32_t x, int n) {
    return (x + ((x >> 31) & ((1 << n) - 1))) >> n;
} // signed division by 2^n
size_t section_start(const std::vector<RouteSample>& road, size_t sample) {
    sample = std::min(sample, road.size() - 1);
    return sample >= road[sample].local ? sample - road[sample].local : 0;
}
int32_t section_length(const std::vector<RouteSample>& road, size_t start) {
    size_t s = start;
    while (s + 1 < road.size() && road[s + 1].section == road[start].section &&
           road[s + 1].local == road[s].local + 1)
        ++s;
    return int32_t(s - start + 1);
}
} // namespace

void Race::set_live_schedule(bool v) {
    live_schedule_ = v;
    if (!v) return;
    // 0x00438930: next = build tick, last = tick - template interval. The
    // player bike and half (0x00463c10) carry interval 1, opponent bikes
    // (0x00463ad8) and halves (0x00463b40) interval 4; dt starts at 0.
    for (size_t i = 0; i < riders_.size(); ++i) init_live_rider(riders_[i], i == 0 ? 1 : 4);
}

void Race::init_live_rider(Rider& r, int32_t interval) {
    r.sched_interval = interval;
    r.sched_next = ticks_;
    r.sched_last = ticks_ - interval;
    r.half_sched_interval = interval;
    r.half_sched_next = ticks_;
    r.half_sched_last = ticks_ - interval;
    r.bike.i32(0x18) = 0;
    r.half_dt = 0;
    r.sched_live = true;
    r.live = true;
    r.parked = false;
    const auto& road = route_of(r);
    r.virtual_section = section_start(road, r.sample);
    r.virtual_local = int32_t(r.sample - r.virtual_section) * 256; // 0x00405990: slice +4
    const Bike& b = r.bike;
    r.half_mirror = {b.i32(0x1c),  b.i32(0x28),  b.i32(0x20), b.i32(0xf4), b.i32(0xec), b.i32(0xf0),
                     b.i32(0x10c), b.i32(0x108), b.i32(0x60), r.sample,    r.route, r.slice_half};
    // 0x00405c80 spawn words (spawn_brain repeats them for built riders).
    r.bike.i32(0x30c) = 600;
    r.bike.i32(0x310) = (((level_ * 5 + 0x2d) * 200) >> 8) * 600;
    r.bike.i32(0x314) = 0;
    r.bike.i32(0x318) = 0;
    // 0x00405c80: half +0x1f8 = 0x00407410, the distance to the finish.
    if (finish_q8_ != 0) r.finish_distance = remaining_finish_distance(r, b.i32(0x1c));
}

void Race::schedule_add(int rider) {
    // 0x00438aa0 for a rider created mid-race: the bike node, then the half
    // node in front of it, each ahead of existing nodes at an equal position.
    if (schedule_.empty()) return; // built lazily from riders_ by schedule_order
    for (uint32_t type : {4u, 8u}) {
        ScheduleNode n{rider, type, 0};
        n.position = n.sorted = node_position(n);
        auto at = std::find_if(schedule_.begin(), schedule_.end(),
                               [&](const ScheduleNode& o) { return o.position <= n.position; });
        schedule_.insert(at, n);
    }
}

std::vector<Race::ScheduleNode> Race::schedule_order() const {
    if (schedule_.empty()) {
        // Insert order (0x00438aa0): a node created at an equal position goes
        // after the existing ones on the ascending +0 chain, so the walk
        // (front to back) meets the later rider first and a half before its
        // bike.
        for (size_t i = 0; i < std::min(riders_.size(), built_riders_); ++i) {
            schedule_.push_back({int(i), 4, 0});
            schedule_.push_back({int(i), 8, 0});
        }
        for (auto& n : schedule_) n.position = n.sorted = node_position(n);
        std::stable_sort(schedule_.begin(), schedule_.end(),
                         [](const ScheduleNode& a, const ScheduleNode& b) {
                             if (a.position != b.position) return a.position > b.position;
                             if (a.rider != b.rider) return a.rider > b.rider;
                             return a.type > b.type;
                         });
    }
    for (auto& n : schedule_) n.position = node_position(n);
    return schedule_;
}

int32_t Race::node_position(const ScheduleNode& n) const {
    const Rider& r = riders_[size_t(n.rider)];
    if (n.type != 8) return r.bike.i32(0x1c);
    // A crashed rider's half node is the body (0x00403d60): the 0x004383e0
    // sort keys it by the body's own +0x1c.
    return r.crash_active ? r.crash_actor.motion.i32(0x1c) : r.half_mirror.position;
}

void Race::sort_schedule() {
    // 0x004383e0: insertion sort of the list by +0x1c; equal positions keep
    // their previous order.
    (void)schedule_order();
    std::stable_sort(schedule_.begin(), schedule_.end(),
                     [](const ScheduleNode& a, const ScheduleNode& b) { return a.position > b.position; });
    for (auto& n : schedule_) n.sorted = n.position;
    car_scan_keys_.clear();
    if (traffic_)
        for (const auto& car : traffic_->cars()) car_scan_keys_.push_back(car.position);
    ped_scan_keys_.clear();
    if (pedestrians_)
        for (const auto& p : pedestrians_->pool()) ped_scan_keys_.push_back(p.body.bike.i32(0x1c));
    // Retain mixed-list tie order even on ticks where no AI scan runs.
    build_scan_list();
}

int32_t live_interval(bool slice, int32_t position, int32_t ring_camera, int32_t speed, int32_t tick,
                      bool player_object) {
    // 0x00437e60 tail for +0xc4 & 0xc (bikes and halves): 30 with no slice,
    // else 1 + (distance ahead or behind the ring camera DAT_004b6e5c in
    // samples)/4, plus 1..4 for slow non-player objects before tick 300,
    // capped at 8.
    if (!slice) return 0x1e;
    int32_t d = sar(position - ring_camera, 8), v = 1;
    if (d < 4) {
        if (d < -2) v = sar(-2 - d, 2) + 1;
    } else {
        v = sar(d + 1, 2) + 1;
    }
    if (tick < 300 && speed < 0x1800 && !player_object) {
        v += speed < 0x1000 ? 2 : 1;
        if (speed < 0x800) v += 2;
    }
    return std::min(v, 8);
}

int32_t parked_pace(int32_t live_distance, int32_t live_ticks, bool ahead_of_player) {
    // 0x00406940: live distance per live tick (Q8), 95% for a rider ahead
    // of the player (DAT_00464304) or 105% behind (DAT_00464308).
    const int32_t pace = (live_distance << 8) / live_ticks;
    return ((ahead_of_player ? 95 : 105) * pace) / 100;
}

void pace_band(std::array<std::optional<int32_t>, 16>& by_place, int player_place) {
    // 0x00407610 body: by_place[k] is the pace +0x314 of the parked rider in
    // place k + 1 (empty if none).
    static constexpr int32_t ahead[3] = {-0x3c, -0x1e, -0xf}, behind[3] = {0x3c, 0x1e, 0xf};
    const int player = player_place - 1;
    int k = 0;
    for (int at = player - 1; at >= 0 && k < 3; --at) {
        if (at > 15 || !by_place[size_t(at)]) continue;
        *by_place[size_t(at)] = std::max(*by_place[size_t(at)] + ahead[k++], 3000);
    }
    k = 0;
    for (int at = player + 1; at <= 15 && k < 3; ++at) {
        if (at < 0 || !by_place[size_t(at)]) continue;
        *by_place[size_t(at)] = std::min(*by_place[size_t(at)] + behind[k++], 30000);
    }
}

int32_t Race::schedule_interval(const Rider& r, bool half) const {
    // Plan 34 C2: LOD distance from the nearest human's ring camera.
    const TrackRing* nearest = nearest_ring(half ? r.half_mirror.position : r.bike.i32(0x1c));
    const int32_t camera = nearest ? nearest->front_q8() - 0x5000 : int32_t(camera_position()) + 0x100;
    return live_interval(half ? r.half_slice_present : r.bike_slice_present,
                         half ? r.half_mirror.position : r.bike.i32(0x1c), camera,
                         half ? r.half_mirror.speed : r.bike.i32(0xec), ticks_, human(r));
}

void Race::schedule_pass(uint32_t controls, bool countdown) {
    (void)controls; // plan 34: each rider's word comes from controls_ via word()
    // 0x00438170(0): every node whose +0x0c is due runs 0x00437e60 with
    // param_3 = (+0x2c != 0) sampled before the call.
    if (pedestrians_ && std::any_of(pedestrians_->pool().begin(), pedestrians_->pool().end(),
                                    [](const Pedestrian& p) { return p.pending_remove; })) {
        //00437df0 follows ascending links before this pass changes sort keys.
        //The LIFO free list retains the last returned node's object image.
        build_scan_list();
        for (const auto& node : scan_)
            if (node.pedestrian >= 0) {
                auto& p = pedestrians_->pool()[size_t(node.pedestrian)];
                if (p.pending_remove) pedestrians_->recycle(p);
            }
    }
    std::vector<ScheduleNode> order = schedule_order();
    const int32_t camera = ring_ ? ring_->front_q8() - 0x5000 : int32_t(previous_camera_position_) + 0x100;
    // 00438170 walks one list. In particular, controller rand() draws
    // must occur between rider thinks at the controller's position.
    struct ActorNode {
        int32_t position;
        uint32_t type;
        size_t index;
    };
    std::vector<ActorNode> actors;
    for (size_t i = 0; i < roadside_nodes_.size(); ++i) {
        auto& n = roadside_nodes_[i];
        if (n.pending_remove) n = {};
        if (n.active && n.next_tick <= ticks_) actors.push_back({n.position, 1, i});
    }
    std::vector<uint32_t> seen_cars;
    car_scan_keys_.clear();
    update_traffic_windows(false);
    const bool traffic_pass = traffic_ && traffic_->begin_schedule(
                                              route_of(riders_[0]), camera + 0x5000,
                                              [this] { return random(); }, uint32_t(ticks_), true);
    if (traffic_pass) {
        const auto& pool = traffic_->cars();
        seen_cars.resize(pool.size());
        for (size_t i = 0; i < pool.size(); ++i) {
            car_scan_keys_.push_back(pool[i].position);
            if (pool[i].active) {
                seen_cars[i] = pool[i].serial;
                actors.push_back({pool[i].position, 0x10, i});
            }
        }
        const auto& controllers = traffic_->controllers();
        for (size_t i = 0; i < controllers.size(); ++i)
            if (controllers[i].active && controllers[i].next_tick <= ticks_)
                actors.push_back({controllers[i].position_q8, 0x400, i});
    }
    //00437df0 has already returned marked pedestrians before dispatch.
    ped_scan_keys_.clear();
    if (pedestrians_) {
        for (size_t i = 0; i < pedestrians_->pool().size(); ++i) {
            const auto& p = pedestrians_->pool()[i];
            ped_scan_keys_.push_back(p.body.bike.i32(0x1c));
            if (p.active && p.next_tick <= ticks_) actors.push_back({p.body.bike.i32(0x1c), 0x20, i});
        }
    }
    auto sort_actors = [&] {
        std::stable_sort(actors.begin(), actors.end(),
                         [](const ActorNode& a, const ActorNode& b) { return a.position > b.position; });
    };
    sort_actors();
    auto run_actors = [&](int32_t position) {
        while (!actors.empty() && actors.front().position >= position) {
            const ActorNode n = actors.front();
            actors.erase(actors.begin());
            if (n.type == 1) {
                auto& solid = roadside_nodes_[n.index];
                solid.last_tick = ticks_;
                solid.next_tick = ticks_ + 20; // null move preserves template interval
                if (outside_windows(solid.position, 0, 0)) {
                    solid.pending_remove = true;
                    solid.last_tick = solid.next_tick = ticks_ + 100;
                }
            } else if (n.type == 0x20) {
                run_ped_node(pedestrians_->pool()[n.index]);
            } else if (n.type == 0x400) {
                traffic_->run_controller(n.index, camera - 0x3000, camera + 0x5000, uint32_t(ticks_),
                                         route_of(riders_[0]), [this] { return random(); });
                // 00438aa0 inserts newly spawned cars in the live list. Only
                // those behind this cursor are visited during the same pass.
                const auto& pool = traffic_->cars();
                for (size_t i = 0; i < pool.size(); ++i)
                    if (pool[i].active && seen_cars[i] != pool[i].serial) {
                        seen_cars[i] = pool[i].serial;
                        car_scan_keys_[i] = pool[i].position;
                        if (pool[i].position < n.position) actors.push_back({pool[i].position, 0x10, i});
                    }
                sort_actors();
            } else {
                prepare_traffic_obstacles();
                const auto& player = riders_[0];
                traffic_->run_car(
                    n.index, route_of(player), camera, camera + 0x5000, player.bike.i32(0x1c),
                    player.bike.i32(0x28), [this] { return random(); }, uint32_t(ticks_),
                    player.bike.i32(0x108));
            }
        }
    };
    for (size_t at = 0; at < order.size(); ++at) {
        const ScheduleNode n = order[at];
        run_actors(n.position);
        Rider& r = riders_[size_t(n.rider)];
        if (n.type == 8) {
            if (r.half_sched_next <= ticks_) run_half_node(r, word(r), countdown);
        } else if (r.sched_next <= ticks_)
            run_bike_node(r, word(r), countdown);
        if (schedule_.size() != order.size()) {
            // The original walks the live ring (0x00438170): a node inserted
            // mid-pass (the periodic cop, 0x00406b80) behind the cursor is met
            // in the same pass. Resume after the current node in the new list.
            order = schedule_;
            bool found = false;
            for (size_t k = 0; k < order.size() && !found; ++k)
                if (order[k].rider == n.rider && order[k].type == n.type) {
                    at = k;
                    found = true;
                }
            if (!found && at > 0) --at; // the current node was unlinked: the next one shifts into its slot
        }
    }
    run_actors(INT32_MIN);
    // 0x00438170, 0x00438385..0x004383a2: human play forces the
    // player's bike and half due next tick, retaining their LOD +0x14.
    // Plan 34 applies the same rule to each human seat.
    if (!demo_hold_)
        for (auto& r : riders_)
            if (human(r)) r.sched_next = r.half_sched_next = ticks_ + 1;
}

void Race::parked_pace_band() {
    // 0x00407610, every 60 ticks: parked riders nearest ahead of the player
    // in place order lose 60/30/15 of pace (DAT_004642e8, floor 3000), the
    // three nearest behind gain 60/30/15 (DAT_004642f8, cap 30000). A parked
    // rider qualifies unless its +0x304 node and the player's lie on the two
    // branches of one split (00454040 node flags40/80 and section+20 group).
    if (pace_band_tick_ > ticks_ || riders_.empty()) return;
    pace_band_tick_ = ticks_ + 0x3c;
    // Plan 34: the band forms around every human in seat order (one human:
    // the original band around the player).
    for (int seat = 0; seat < humans(); ++seat) {
        const Rider& h = riders_[size_t(human_rider(seat))];
        if (seat != 0 && !human(h)) continue;
        std::array<std::optional<int32_t>, 16> by_place{};
        std::array<Rider*, 16> who{};
        const auto& human_road = route_of(h);
        for (auto& r : riders_)
            if (!r.cop && !r.live && r.place > 0 && r.place < 0x11) {
                const auto& road = route_of(r);
                if (finish_course_ && !shared_course_branch(*finish_course_, road[r.virtual_section].node,
                                                            human_road[h.sample].node)) continue;
                by_place[size_t(r.place - 1)] = r.bike.i32(0x314);
                who[size_t(r.place - 1)] = &r;
            }
        pace_band(by_place, h.place);
        for (size_t k = 0; k < 16; ++k)
            if (who[k]) who[k]->bike.i32(0x314) = *by_place[k];
    }
}

void Race::run_half_node(Rider& r, uint32_t controls, bool countdown) {
    // 0x00437e60 on a type-8 node: think 0x00407bd0 (0x00434660 for the
    // player) unless parked, move 0x00406890, interval, next. The window
    // and lateral calls reach 0x00406b40, which only clamps a live half's
    // lateral when called with edx 1.
    r.half_dt = ticks_ - r.half_sched_last;
    r.half_sched_last = ticks_;
    if (!r.parked) think_half(r, controls, countdown);
    // 0x00406890 (the half move): while the bike is crashed the half runs
    // the detached body's own motion (0x0043adc0) at the half node's dt,
    // then the +0x1f8 finish delta and the place refresh - the body words
    // and the ring node position stay live for the scans behind it.
    if (crash_library_ && r.bike.i32(0x2f4) && r.crash_active) crash_body_motion(r, r.half_dt);
    //00406890 latches finish from the half mover, without clamping the
    //bike's preceding movement to the line. Parked riders finish virtually.
    const int32_t finish = known_finish_q8();
    if (r.half_slice_present && finish != 0 && r.half_mirror.position >= finish &&
        (r.half_object.u8(0xc0) & 0x10) == 0) {
        r.half_object.u8(0xc0) |= 0x10;
        r.bike.u8(0xc0) |= 0x10;
        if (!r.finished && !r.cop) {
            r.finished = true;
            r.finish_tick = ticks_;
        }
    }
    r.half_sched_interval = schedule_interval(r, true);
    if (r.live && (r.half_mirror.lateral < -0xc8000 || r.half_mirror.lateral > 0xc8000))
        r.half_mirror.lateral = std::clamp<int32_t>(r.half_mirror.lateral, -0xc8000, 0xc8000);
    // While crashed the half is the crash body, so the clamp lands on the
    // body's own +0x28 and the walk-back starts from the edge (traffic-field
    // slice capture tick 1923: original 819200 - 1002, native kept walking
    // from the unclamped body).
    if (r.live && r.crash_active) {
        Bike& body = r.crash_actor.motion;
        if (body.i32(0x28) < -0xc8000 || body.i32(0x28) > 0xc8000) {
            body.i32(0x28) = std::clamp<int32_t>(body.i32(0x28), -0xc8000, 0xc8000);
            r.half_object.i32(0x28) = body.i32(0x28);
        }
    }
    r.half_sched_next = ticks_ + r.half_sched_interval;
}

void Race::think_half(Rider& r, uint32_t controls, bool countdown) {
    const bool player = human(r);
    //00407bd0 still runs AI transitions and replanning after the finish
    //latch; the bike's bit0x10 makes throttle select braking.
    // The countdown ticks run the whole think (0x00407f30 advances the
    // riding animation from tick 1 on napa-field-l1-20261017).
    (void)countdown;
    if (crash_library_ && r.bike.i32(0x2f4)) {
        // 0x00434660 routes every crashed rider to 0x00407bd0 (and the
        // crashed branch skips the player's input block): the transition
        // latches state 9 (state 8 for crash states 2/3), the state clock
        // and replan accumulator add the half dt, and the stamina regen
        // runs its crash-gated passes. The think tail 0x00407f30 then
        // advances the crash rider animation and the +0x2f4 phase off the
        // body words left by the previous half move; the walk-back and
        // remount states follow it on the same half block.
        if (!r.crash_active) crash(r); // entry applies once, on the first node to run
        // 004346c6..004346e1 starts the result for a crashed, finish-latched
        // player before dispatching crash AI, without a speed threshold.
        if (player && (r.half_object.u8(0xc0) & 0x10) != 0) {
            r.finish_controls_ended = true;
            register_result(r, 0x7f);
        }
        ai_think(r);
        // 0x00434660 wrapper stamina regen (lines 15-24): the player half
        // regenerates +dt whenever crash != 1, on top of the 0x00407bd0
        // passes (fight 2239: crash 2 needs +3/tick, not the +2 the passes
        // alone give). Uncrashed this is fight()'s regen; crash 1 gets none.
        if (player && r.bike.i32(0x2f4) != 1 && r.stamina.current < r.stamina.max)
            r.stamina.current = std::min(r.stamina.max, r.stamina.current + r.bike.i32(0x18));
        crash_recovery(r, player, controls);
        if (player) police_think(r); // a rider on foot can be arrested
        if (r.crash_active)
            crash_phases(r);
        else
            advance_ride_animation(r, player);
        return;
    }
    // 004346a1..004346b6 selects AI after any terminal outcome as
    // well as during attract hold; player input ends before the result delay.
    const bool demo_player = player && (demo_hold_ || r.result_code != 1);
    fight(r, demo_player ? 0 : controls, size_t(&r - riders_.data()));
    rider_think(r, controls, player);
}

void Race::run_bike_node(Rider& r, uint32_t controls, bool countdown) {
    // 0x00437e60 on a type-4 node.
    Bike& b = r.bike;
    const bool slice = r.bike_slice_present;
    b.i32(0x18) = ticks_ - r.sched_last;
    r.sched_last = ticks_;
    // 0x00407a90: scenery-mark animation on the bike mirrors the half's
    // +0x1e9, advances once per four absolute ticks and expires after12.
    // Disassembly words (407acd-407b0b): sample check bike +0x1c>>8 vs half
    // +0x1ec, gap unsigned bike +0x10 minus half +0x1f0. Native keeps the
    // +0x10 image in Rider::sched_last (the raw Bike word +0x10 is never
    // stored; it must not be read), stored to ticks_ two lines above, and
    // r.sample equals bike +0x1c>>8 at every bike think (0 mismatches over
    // the 2400-tick field replay, October 25 probe), so these forms are
    // behavior-identical to the disassembly here.
    if (!r.parked && b.i32(0x2f4) != 1 && b.i32(0x2f4) != 2) {
        auto& half = r.half_object;
        if (half.u8(0x1e9)) {
            const uint32_t elapsed = uint32_t(r.sched_last) - uint32_t(half.i32(0x1f0));
            half.u8(0x1e9) =
                int32_t(r.sample) == half.i32(0x1ec) && elapsed < 12 ? uint8_t((elapsed >> 2) + 1) : 0;
        }
        b.u8(0x81) = half.u8(0x1e9);
    }
    // Think 0x00407a90 (nulled while parked): the player's nitro charge
    // 0x0040b1a0; its animation mirror is part of the native riding block.
    if (!r.parked && human(r) && !countdown) nitro_charge_tick(r);
    // Move 0x004060b0.
    if (r.live)
        move_bike_live(r, controls, countdown);
    else
        move_bike_parked(r);
    // 0x00437e60: a bike left on an end slice of the ring parks (+0xe8, edx 0).
    if (ring_ && r.bike_slice_present) {
        const int32_t s = b.i32(0x1c) >> 8;
        bool outside = true; // every window (plan 34 C2; one ring: the original test)
        for (const TrackRing* w : rings_)
            if (w && !(s < (w->back_q8() >> 8) || s > (w->front_q8() >> 8) - 1)) outside = false;
        //00437e99..00437eb7 also parks a rider that crossed the numeric
        //coordinate of a physical self-linked end slice (including a
        //discarded split arm inside the global ring window).
        if (s != int32_t(r.sample) && ring_->forward_closed(int32_t(r.sample * 256), r.slice_half.value_or(route_of(r)[r.sample].half)))
            outside = true;
        if (outside) park(r);
    }
    r.sched_interval = schedule_interval(r, false);
    // Lateral beyond +-0xc8000: 0x00406940 with edx 1 clamps a live bike.
    if (r.live && (b.i32(0x28) < -0xc8000 || b.i32(0x28) > 0xc8000))
        b.i32(0x28) = std::clamp<int32_t>(b.i32(0x28), -0xc8000, 0xc8000);
    r.sched_next = ticks_ + r.sched_interval;
    // 0x00438108 window test on a run that began with a slice: within 0x1e00
    // of the ring front or 0x800 of the ring back parks the bike.
    if (slice && ring_ && outside_windows(b.i32(0x1c), 0x1e00, 0x800)) park(r);
}

void Race::move_bike_live(Rider& r, uint32_t controls, bool countdown) {
    // 0x004060b0 live branch: physics 0x0043a580, then (uncrashed) the half
    // copy 0x004067d0 and standings, and the +0x30c/+0x310 sums.
    Bike& b = r.bike;
    const bool player = human(r);
    const int32_t before = b.i32(0x1c);
    const auto& road = route_of(r);
    auto physical_node = [&](size_t sample) {
        const auto& logical = road[std::min(sample, road.size() - 1)];
        if (const auto* ring = nearest_ring(b.i32(0x1c)))
            if (const auto physical = ring->retained_sample(int32_t(sample * 256), r.slice_half.value_or(logical.half)))
                return physical->node;
        return logical.node;
    };
    const uint32_t node_before = physical_node(r.sample);
    // The countdown runs the same physics move: with no target speed the
    // bike stays put, but its ground contact word settles (+0x160 37 -> 74
    // at tick 1 on napa-field-l1-20261017).
    (void)countdown;
    {
        if (crash_library_ && b.i32(0x2f4)) {
            // 0x004060b0's +0x300 path: the riderless bike keeps its own
            // animation advance (the crashed branch of the bike think
            // 0x00407a90) and the 0x0043a580 move on the bike node; the
            // crash phases, the body motion and the recovery belong to the
            // half node (think_half / run_half_node).
            if (!r.crash_active) crash(r); // entry applies once, on the first node to run
            crash_bike_animation(r);
            crash_bike_motion(r, player);
            // 0x004060b0 lines 46-67: the crashed bike and its detached half
            // (the crash body, object +0x2fc) drift apart as the body keeps
            // sliding on its own drag. Once they sit more than 0x500 apart
            // along the road, the bike's speed eighths toward the half's: the
            // slower side gains (|half| - |bike|) / 8, or, when the half is
            // slower, the bike sheds its own speed / 8. The following
            // 0x0043c7e0 serializes a network snapshot; its first branch
            // returns when both network-mode bytes are zero (offline).
            const Bike& body = r.crash_actor.motion;
            if (std::abs(b.i32(0x1c) - body.i32(0x1c)) > 0x500) {
                const int32_t bike_speed = b.i32(0xec);
                const int32_t half_speed = body.i32(0xec);
                const int32_t bike_abs = std::abs(bike_speed);
                const int32_t half_abs = std::abs(half_speed);
                if (half_abs < bike_abs)
                    b.i32(0xec) = bike_speed - sar(bike_speed, 3);
                else
                    b.i32(0xec) = bike_speed + sar(half_abs - bike_abs, 3);
            }
        } else {
            rider_move(r, player);
        }
    }
    const int32_t after = b.i32(0x1c);
    if (b.i32(0x2f4) == 0) {
        live_standings(r);
        // Half +0x1f8: moved along the same node, subtract the move;
        // otherwise 0x00407410 recomputes it (finish - position once the
        // finish is known). Contact pushes between moves are not folded in.
        if (physical_node(r.sample) == node_before)
            r.finish_distance += before - after;
        else if (finish_q8_ != 0)
            r.finish_distance = remaining_finish_distance(r, after);
        if (r.place_refresh <= r.half_sched_last) refresh_place(r); // 0x004075f0 -> 0x00407550
    }
    b.i32(0x310) += after - before;
    b.i32(0x30c) += b.i32(0x18);
}

void Race::refresh_place(Rider& r) {
    const bool player = human(r);
    const size_t i = size_t(&r - riders_.data());
    auto mask = [&](const Rider& actor) {
        const auto& half = actor.half_mirror;
        const auto& sample = (*routes_[half.route])[half.sample];
        if (const auto* ring = nearest_ring(half.position))
            if (const auto retained = ring->retained_roadway_mask(int32_t(half.sample * 256), half.slice_half.value_or(sample.half)))
                return *retained;
        return roadway_mask(sample);
    };
    int place = 1;
    for (size_t j = 0; j < riders_.size(); ++j) {
        if (j == i || riders_[j].cop) continue; // the place table 0x004c7fb0 holds no cop
        const Rider& o = riders_[j];
        const bool shared = o.half_slice_present && r.half_slice_present &&
                            (mask(r) & mask(o) & 3) != 0;
        const bool ahead = shared ? o.half_mirror.position > r.half_mirror.position : o.finish_distance < r.finish_distance;
        if (std::getenv("RR_SIM_TRACE_TICK") && ticks_ == std::atoi(std::getenv("RR_SIM_TRACE_TICK")) &&
            std::getenv("RR_SIM_TRACE_RIDER") && int(i) == std::atoi(std::getenv("RR_SIM_TRACE_RIDER")))
            std::fprintf(stderr, "place-compare self %zu other %zu shared %d slices %d/%d positions %d/%d distance %d/%d ahead %d\n",
                         i, j, shared, r.half_slice_present, o.half_slice_present,
                         r.half_mirror.position, o.half_mirror.position, r.finish_distance, o.finish_distance, ahead);
        if (ahead)
            ++place;
    }
    r.place = place;
    r.place_refresh = r.half_sched_last + (player ? 4 : 0x3c);
}

void Race::live_standings(Rider& r) {
    // 0x004067d0: an uncrashed bike copies its motion words and slice to
    // its half.
    const Bike& b = r.bike;
    r.half_mirror = {b.i32(0x1c),  b.i32(0x28),  b.i32(0x20), b.i32(0xf4), b.i32(0xec), b.i32(0xf0),
                     b.i32(0x10c), b.i32(0x108), b.i32(0x60), r.sample,    r.route, r.slice_half};
    r.half_slice_present = r.bike_slice_present;
}

void Race::park(Rider& r) {
    // 0x00406940 with edx 0 on a live opponent (+0x324 != 0).
    if (!r.live || human(r)) return; // humans never park (0x00406940 skips +0x324 == 0)
    if (r.cop) {                     // single player: 0x004073a0 deletes the cop instead of parking it
        // 0x004073a0: siren off (event 0x13), DAT_004642dc = 0 at once, and
        // 0x00437c10 marks the half and bike (0x00437be0: +0xc0 &= ~4,
        // +0xc = +0x10 = tick + 100; then +0xc0 |= 8). The next scheduler
        // prepass (0x00437df0) unlinks them, so the rider is still dumped on
        // its deletion tick (traffic-field box capture tick 4104: bike_c =
        // bike_10 = 4204, gone on 4105).
        siren_event(r, false);
        for (Bike* b : {&r.bike, &r.half_object}) b->u8(0xc0) = uint8_t((b->u8(0xc0) & 0xfb) | 8);
        r.sched_next = r.sched_last = r.half_sched_next = r.half_sched_last = ticks_ + 100;
        r.cop = false;
        cop_removal_pending_ = true;
        return;
    }
    Bike& b = r.bike;
    b.i32(0x318) = 0;
    if (b.i32(0x2f4) != 0) {
        if (b.i32(0x2f4) == 1) {
            const int32_t v = r.half_mirror.speed;
            b.i32(0x318) = sar((v >> 9) + (v >> 12) + (v >> 7), 5) * 0x3c;
        }
        b.i32(0x318) += sar(std::abs(b.i32(0x1c) - r.half_mirror.position), 8) * 0x3c;
        // 0x00404560: crashed-rider cleanup remounts the rider, restores
        // stamina, copies bike words to the half (0x004067d0) and recomputes
        // finish distance (0x00407410).
        if (crash_library_) {
            remount_crash_actor(b, r.crash_bike_animation, r.crash_actor, crash_config_, crash_bank(r),
                                crash_library_->fallback, uint32_t(ticks_));
            r.ride_animation = r.crash_actor.animation;
        } else {
            b.i32(0x2f4) = 0;
        }
        r.stamina.current = r.stamina.max;
        r.crash_active = false;
        live_standings(r);
        if (finish_q8_ != 0) r.finish_distance = remaining_finish_distance(r, r.half_mirror.position);
    }
    // Pace: live distance per live tick (Q8), 95% ahead of the player
    // (DAT_00464304) or 105% behind (DAT_00464308).
    // +0x304/+0x308 follow the bike's slice: 0x00439390 copies the node and
    // local offset (whole samples) at every sample crossing.
    const auto& road = route_of(r);
    r.virtual_section = section_start(road, r.sample);
    r.virtual_local = int32_t(r.sample - r.virtual_section) * 256;
    if (const auto* ring = nearest_ring(b.i32(0x1c)))
        if (const auto physical = ring->slice_location(int32_t(r.sample * 256), road[r.sample].half)) {
            const auto node = std::find_if(road.begin(), road.end(), [&](const RouteSample& s) {
                return s.node == physical->first;
            });
            if (node != road.end()) {
                r.virtual_section = section_start(road, size_t(node - road.begin()));
                r.virtual_local = int32_t(physical->second * 256);
            }
        }
    b.i32(0x314) =
        parked_pace(b.i32(0x310), b.i32(0x30c), nearest_human(b.i32(0x1c)).bike.i32(0x1c) < b.i32(0x1c));
    r.live = false;  // 0x00431960(bike, 0)
    r.parked = true; // +0xdc/+0xe0 of bike and half to +0x31c/+0x320 / +0x200/+0x204
    b.u8(0xc0) &= 0xfc;
    r.bike_slice_present = r.half_slice_present = false;
}

void Race::move_bike_parked(Rider& r) {
    // 0x004060b0 parked branch (+0x300 clear).
    Bike& b = r.bike;
    const auto& road = route_of(r);
    bool section_changed = false;
    if ((b.u8(0xc0) & 0x10) == 0) {
        int32_t step = 0;
        const int32_t dt = b.i32(0x18), pace = b.i32(0x314);
        if (b.i32(0x318) < 1) {
            if (nearest_human(b.i32(0x1c)).bike.i32(0x2f4) == 0)
                step = dt * pace >> 8;
            else if (level_ == 3)
                step = sar(pace * 3, 2) * dt >> 8;
            else if (level_ == 4)
                step = dt * pace >> 8;
            else
                step = (pace / 2) * dt >> 8;
        } else {
            b.i32(0x318) -= dt;
        }
        r.virtual_local += step;
        r.finish_distance -= step;
        b.i32(0x1c) += step;
        r.half_mirror.position = b.i32(0x1c);
        // Virtual finish: the finish section's +0x10 -> +0x2c finish sample.
        if (finish_q8_ != 0) {
            const size_t finish = size_t(finish_q8_ >> 8);
            const int32_t local = finish_course_
                                      ? section_finish_sample(finish_course_->sections.at(
                                                                  road[r.virtual_section].section), level_) * 256
                                      : finish < road.size() && section_start(road, finish) == r.virtual_section
                                            ? int32_t(finish - r.virtual_section) * 256 : 0;
            if (local != 0) {
                if (local != 0 && r.finish_distance < 1) {
                    if ((b.u8(0xc0) & 0x10) == 0) { // 0x00437800
                        b.u8(0xc0) |= 0x10;
                        r.half_object.u8(0xc0) |= 0x10;
                        if (!r.finished && !r.cop) {
                            r.finished = true;
                            r.finish_tick = ticks_;
                        }
                    }
                    section_changed = true;
                    r.virtual_local = local + int32_t(random() % 0x701) + 0x100;
                }
            }
        }
        // Section walk over +0x304/+0x308 (the route's own branch at splits).
        int32_t length = section_length(road, r.virtual_section);
        while (length * 256 <= r.virtual_local) {
            section_changed = true;
            const size_t next = r.virtual_section + size_t(length);
            if (next >= road.size()) { // end node (link types 3/4)
                r.virtual_local = (length - 1) * 256;
                b.i32(0x314) = 0;
                break;
            }
            r.virtual_local -= length * 256;
            r.virtual_section = next;
            length = section_length(road, r.virtual_section);
        }
        if (section_changed && finish_q8_ != 0) r.finish_distance = remaining_finish_distance(r, b.i32(0x1c)); // 0x00407410
    }
    // 004065d4 reads this graph node's actual retained tracker pointers.
    // Plan 34 C1: every human's window, in seat order; the first that
    // reaches the rider wakes it (one ring: the original test).
    for (const TrackRing* w : rings_) {
        if (!w) continue;
        const uint32_t node = road[r.virtual_section].node;
        const auto refs = w->tracker_window(node);
        const bool has0 = refs[0].has_value(), has1 = refs[1].has_value();
        if (!has0 && !has1) continue;
        const int32_t length = section_length(road, r.virtual_section);
        const int32_t lo = has0 ? refs[0]->local : 0;
        const int32_t hi = has1 ? refs[1]->local : length * 256 - 1;
        const int32_t v = r.virtual_local;
        if (std::getenv("RR_SIM_TRACE_TICK") && ticks_ == std::atoi(std::getenv("RR_SIM_TRACE_TICK")))
            std::fprintf(stderr, "wake-window slot %d node %u local %d bounds %d/%d has %d/%d\n",
                         r.slot, node, v, lo, hi, has0, has1);
        if (v < lo || hi < v) continue;
        bool use0 = has0;
        if (has0 && has1) use0 = std::abs(v - lo) <= std::abs(v - hi);
        const int32_t slice = (use0 ? refs[0]->position : refs[1]->position) >> 8;
        const int32_t back = w->back_q8() >> 8, front = w->front_q8() >> 8;
        // 0x00449910: step whole slices from the tracker's slice, clamped to the ring.
        int32_t steps = sar(v - (use0 ? lo : hi), 8);
        if (steps >= 1 && front <= steps + slice) steps = front - slice - 1;
        if (steps < 0 && steps + slice < back) steps = back - slice;
        wake(r, size_t(slice + steps));
        return;
    }
}

void Race::wake(Rider& r, size_t sample) {
    // 0x004060b0 tail: the bike lands mid-slice on the road centre, takes
    // its straight and overtake targets, rides at its pace, and becomes
    // live again with its callbacks restored.
    Bike& b = r.bike;
    const auto& road = route_of(r);
    sample = std::min(sample, road.size() - 1);
    const int32_t pos = int32_t(sample) * 256 + 0x80;
    b.i32(0x1c) = pos;
    b.i32(0x20) = 0;
    b.i32(0x24) = 0;
    b.i32(0x28) = road[sample].offset * 256 + road_base_; // sub-slice +0xcc
    r.sample = r.previous_sample = sample;
    r.slice_half = r.previous_slice_half = route_of(r)[sample].half;
    b.i32(0x134) = b.i32(0x138) = pos;
    r.bike_slice_present = true;
    r.virtual_section = section_start(road, sample);
    r.virtual_local = int32_t(sample - r.virtual_section) * 256;
    ai_straight_target(r);
    b.i32(0x28) = r.brain.target_x;
    ai_overtake(r);
    b.i32(0x28) = r.brain.target_x;
    b.i32(0xf0) = 0;
    b.i32(0xf4) = 0;
    b.i32(0xec) = (b.u8(0xc0) & 0x10) == 0 ? b.i32(0x314) : 0;
    r.parked = false;
    b.u8(0xc0) |= 3;
    r.live = true;                            // 0x00431960(bike, 1)
    if (b.i32(0x2f4) == 0) live_standings(r); // 0x004067d0
    // 0x00407550: refresh the half's place now.
    refresh_place(r);
}
} // namespace rr
