#include "race.hpp"
#include "pedestrians.hpp"
#include "track_ring.hpp"
#include "traffic.hpp"
#include "terrain_ground.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdio>

// Pedestrians, plan 24 Task 4. Each function names the routine it ports.
// The live scheduler interleaves pedestrians with riders and traffic. The
// track ring walks both roadways at splits, and draw-time category-2 lookup
// follows the shared resource queue. Allocation queries the newly built
// sub-slice before copying its placement words, as 0x00438930 does.
// Standalone fixtures without a track ring retain the fallback slice builder.
namespace rr {
int32_t Race::allocation_ground(const Rider& r, const RouteSample& current) const {
    const auto& b = r.bike;
    const auto& path = current.path;
    const int32_t lateral = b.i32(0x28) - current.offset * 256;
    if (!terrain_source_) {
        const int32_t x = lateral >> 8;
        if (x > path.right)
            return x >= path.right + path.shoulder_right ? 999 : path.height_right;
        if (x < -path.left)
            return x <= -path.left - path.shoulder_left ? 999 : path.height_left;
        return 0;
    }
    const auto& terrain = ground(current);
    const uint8_t fraction = uint8_t(b.i32(0x138));
    if (terrain.type != 4 || fraction == 0)
        return terrain_height(terrain, path, terrain, path, lateral, fraction, 0, current.offset);

    // The current slice has just been written, but its forward physical
    // slot can still hold the previous ring generation (or the initial slot template).
    // Do not substitute the not-yet-built future route geometry.
    const auto& route = route_of(r);
    const auto* ring = nearest_ring(b.i32(0x1c));
    if (ring && !ring->slice_location(int32_t((r.sample + 1) * 256), current.half)) {
        // 004551c0 initializes unbuilt slots: ground edges e0/e4 are
        // -500/+500, shoulders e8/ec -650/+650. They are not zeroed roads.
        PathSample initial{};
        initial.left = initial.right = 500;
        initial.shoulder_left = initial.shoulder_right = 150;
        return terrain_height(terrain, path, TerrainSlice{}, initial, lateral, fraction,
                              -current.offset, current.offset);
    }
    size_t next_sample = std::min(r.sample + 1, route.size() - 1);
    if (ring) {
        const int64_t back = ring->back_q8() >> 8;
        next_sample = size_t(std::clamp<int64_t>(back + ((int64_t(next_sample) - back) & 127),
                                                0, int64_t(route.size() - 1)));
    }
    auto next = route[next_sample];
    if (next.dual && next.event == RoadEvent::split) next = route_half(next, 1);
    return terrain_height(terrain, path, ground(next), slice_path(next, next_sample), lateral, fraction,
                          next.offset - current.offset, current.offset);
}
namespace {
int32_t sar8(int32_t x) {
    return (x + ((x >> 31) & 0xff)) >> 8;
}
int32_t sar4(int32_t x) {
    return (x + ((x >> 31) & 0xf)) >> 4;
}
int32_t div256(int64_t x) {
    return int32_t((x + ((x >> 63) & 0xff)) >> 8);
}
int32_t magnitude(int32_t v) {
    const uint32_t s = uint32_t(v >> 31);
    return int32_t((uint32_t(v) ^ s) - s);
}
constexpr int32_t crossing_heading[4] = {0, 0x400000, -0x800000, -0x400000}; // 0x0046df30 by +0x1ec
constexpr int32_t car_half_length = 64, car_half_height = 8960; // 0x00475700 + kind * 0x18 via 0x004387a0
constexpr int32_t car_mass = 1800;
} // namespace

void Race::build_authority_regions(){
    if(!snapshot_authority_||!pedestrians_)return;
    for(int seat=0;seat<humans();++seat){
        const auto& human=riders_[size_t(human_rider(seat))];const auto& road=route_of(human);
        const int64_t center=int64_t(seat_camera(seat))+0x100;
        const int64_t first=std::max<int64_t>(0,(center-0x3000)>>8);
        const int64_t last=std::min<int64_t>(int64_t(road.size()),((center+0x5000)>>8)+1);
        actor_seat_=seat;
        for(int64_t index=first;index<last;++index){
            const auto& sample=road[size_t(index)];const auto& records=pedestrians_->actors(sample);
            for(size_t i=0;i<records.size();++i)if(pedestrians_->consume(sample.section,sample.local,i))
                spawn_pedestrian(sample,size_t(index),records[i],random());
        }
    }
    actor_seat_=0;load_pedestrian_looks();
}

void Race::build_pedestrian_slices() {
    // 0x004552e0 fills the ring back to front (128 slices); 0x00454120 then
    // builds the old front slice when moving forward and the new back slice
    // when moving backward. Every build walks the slice's DZHR records
    // (0x004520d0), firing each record once.
    if (!pedestrians_) return;
    auto& system = *pedestrians_;
    const auto& road = route_of(riders_[0]);
    const int32_t camera = int32_t(camera_position());
    const int32_t front = (camera + 0x5100) >> 8, back = (camera - 0x2f00) >> 8;
    auto build = [&](int32_t s) {
        if (s < 0 || size_t(s) >= road.size()) return;
        const auto& sample = road[size_t(s)];
        const auto& records = system.actors(sample);
        for (size_t i = 0; i < records.size(); ++i)
            if (system.consume(sample.section, sample.local, i))
                spawn_pedestrian(sample, size_t(s), records[i], random());
    };
    if (!system.filled) {
        system.filled = true;
        system.built_back = system.built_front = front - 128;
    }
    while (system.built_front < front) build(system.built_front++);
    while (system.built_back > back) build(--system.built_back);
    system.built_front = std::min(system.built_front, front);
    system.built_back = std::max(system.built_back, back);
}

void Race::set_track_ring(TrackRing* ring) {
    ring_ = ring;
    if (!ring_) return;
    ring_random_ = [this] { return random(); };
    ring_sink_ = [this](const RingActor& a, uint32_t jitter) { ring_actor(a, jitter); };
    rings_[0] = ring_;
    ring_setup_ = true;
    ring_->setup(int32_t(camera_position()) + 0x100, ring_random_, ring_sink_); // ring camera 0x004b7ee0
    ring_setup_ = false;
}

void Race::add_human_ring(int seat, TrackRing* ring) {
    if (seat < 1 || seat >= humans() || !ring || !ring_) return;
    rings_[size_t(seat)] = ring;
    seat_sink_[size_t(seat)] = [this, seat](const RingActor& a, uint32_t jitter) {
        if (sample_in_other_window(seat, a.ring_sample)) return;
        actor_seat_ = seat; // a DZHR cop spawns ahead of this seat's human
        ring_actor(a, jitter);
        actor_seat_ = 0;
    };
    seat_previous_[size_t(seat)] = seat_camera(seat);
    ring_setup_ = true;
    ring->setup(int32_t(seat_camera(seat)) + 0x100, ring_random_, seat_sink_[size_t(seat)]);
    ring_setup_ = false;
}

bool Race::sample_in_other_window(int seat, int32_t sample) const {
    for (int k = 0; k < 8; ++k) {
        const TrackRing* r = rings_[size_t(k)];
        if (k == seat || !r) continue;
        if (sample >= (r->back_q8() >> 8) && sample < (r->front_q8() >> 8)) return true;
    }
    return false;
}

const TrackRing* Race::nearest_ring(int32_t position) const {
    const TrackRing* best = ring_;
    int64_t best_d = -1;
    for (const TrackRing* r : rings_) {
        if (!r) continue;
        const int64_t d = std::abs(int64_t(r->front_q8() - 0x5000) - position);
        if (best_d < 0 || d < best_d) best_d = d, best = r;
    }
    return best;
}

bool Race::outside_windows(int32_t pos, int32_t front, int32_t back) const {
    for (const TrackRing* r : rings_)
        if (r && !(r->front_q8() - pos < front || pos - r->back_q8() < back)) return false;
    return true;
}

void Race::ring_actor(const RingActor& a, uint32_t jitter) {
    if(snapshot_authority_)return;
    // Single-road ring: the slice's route sample is its ring position.
    if (!pedestrians_) return;
    const auto& road = route_of(riders_[0]);
    if (a.ring_sample < 0 || size_t(a.ring_sample) >= road.size()) return;
    auto sample = road[size_t(a.ring_sample)];
    if (sample.dual && sample.other_section == a.section && sample.other_local == a.local)
        sample = route_half(sample, sample.half ^ 1);
    sample.offset = a.road_offset;
    spawn_pedestrian(sample, size_t(a.ring_sample),
                     DzhrActor{uint32_t(a.ring_sample), a.word0, a.word1}, jitter);
}

void Race::spawn_pedestrian(const RouteSample& sample, size_t s, const DzhrActor& record, uint32_t jitter) {
    // 0x004520d0: lateral (slice +0xcc + signed 12 bits) * 256, jittered by
    // the record's spread; the caller draws the rand() first for every
    // actor record.
    auto& system = *pedestrians_;
    const uint32_t w0 = record.word0, w1 = record.word1;
    const int32_t offset12 = (w1 & 0x800) ? int32_t(w1 | 0xfffff000u) : int32_t(w1 & 0xfff);
    const uint32_t spread = (w1 & 0xff0000u) >> 14;
    const int type = int(w0 & 15);
    const int32_t lateral =
        int32_t(uint32_t((sample.offset + offset12) * 256) - spread * 2 + jitter % (spread * 4 + 1));
    if (type >= 0xb) {
        // 0x004409e0 else-branch: two rand() draws, then the police bike
        // 0x00406b80 at (second draw % 0x81 + 0x200) * 0x10 >> 8 slices
        // ahead of the player, with the first draw as the siren distance.
        // It draws only once the player exists (DAT_004642d8); the ring
        // setup runs before 0x00405600 builds it.
        if (!ring_setup_) {
            random();
            const uint32_t ahead = random() % 0x81;
            spawn_cop(int32_t((ahead + 0x200) * 0x10) >> 8, human_rider(actor_seat_));
        }
        return;
    }
    const int32_t player = ring_setup_ ? 0 : snapshot_authority_
        ? riders_[size_t(human_rider(actor_seat_))].half_mirror.position : player_half_position();
    Pedestrian* slot = nullptr;
    for (auto& p : system.pool())
        if (!p.active && (!slot || p.free_order > slot->free_order)) {
            slot = &p;
        }
    if (!slot) return; // pool full: 0x004387a0 returns null, the record is still consumed
    Pedestrian& p = *slot;
    //00438c10 returns the existing pool storage. 004387a0 clears selected
    //motion fields, but not +108/+10c; 004409e0 leaves crossing lane+1fc.
    const auto previous_image = p.body.bike.m;
    const int8_t previous_lane = p.lane;
    p = Pedestrian{};
    p.body.bike.m = previous_image;
    p.lane = previous_lane;
    p.active = true;
    Bike& b = p.body.bike;
    system.apply_template(b);
    b.u8(0xc0) =
        uint8_t(b.u8(0xc0) | 0x10); // native guard: cross_sample's finish latch never applies to pedestrians
    const int32_t position = int32_t(uint32_t(s) * 256 + (w1 >> 24));
    b.i32(0x1c) = position;
    b.i32(0x20) = 0;
    b.i32(0x24) = 0;
    b.i32(0x28) = lateral;
    b.i32(0x134) = position;
    b.i32(0x138) = position;
    p.body.route = riders_[size_t(snapshot_authority_ ? human_rider(actor_seat_) : 0)].route;
    const int initial_route = p.body.route;
    for (int index = 0; index < 2; ++index) {
        const int road = (initial_route + index) % 2;
        if (s < routes_[road]->size() && (*routes_[road])[s].section == sample.section &&
            (*routes_[road])[s].local == sample.local) {
            p.body.route = road;
            break;
        }
    }
    p.body.sample = p.body.previous_sample = s;
    b.i32(0x24) = allocation_ground(p.body, sample);
    for (int k = 0; k < 5; ++k) b.i32(0x30 + 4 * k) = b.i32(0x1c + 4 * k);
    p.anchor = position; // +0x1c8..+0x1d8 copy of +0x1c..+0x2c
    p.anchor_lateral = lateral;
    p.selector = uint8_t(w0 >> 8);
    p.type = type;
    p.side = lateral < 1 ? 1 : 3;
    p.trigger = player + int32_t(random() % 0x4fb1);
    p.serial = system.next_serial();
    // 0x00440680 loads the category-2 resources (+0x1e0) lazily at the
    // first draw, not here: p.look stays null until Race::load_pedestrian_looks
    // models that frame render. The think's +0x208/+0x21c gate and its
    // rand() draw at 0x00440eff read the unloaded words until then (plan 10
    // napa-field-l5 measured the tick-1 draw this way).
    // Scan-list ties: ring-setup spawns exist before 0x00405600 builds the
    // riders; later spawns come after every rider.
    p.order = (ring_setup_ ? 0u : 0x40000000u) + spawn_order_++;
    // 0x00438930: +0x10 = tick - interval, +0xc = tick; ring-setup spawns
    // see Race::set_pedestrian_setup_tick.
    const int32_t created = ring_setup_ ? setup_tick_ : ticks_;
    p.last_tick = created - b.i32(0x14);
    p.next_tick = created;
    p.look = nullptr; // 0x00440680 loads it at the first draw (load_pedestrian_looks)
    ++system.spawned_;
}

void Race::load_pedestrian_looks() {
    // 0x00440680/0x00440b80: the renderer loads a pedestrian's category-2
    // resources on its first drawn frame, after that tick's sim step. The
    // think gate at 0x00440eff reads the still-unloaded +0x208/+0x21c words
    // until then and passes via +0x1e0 == 0 (plan 10 napa-field-l5 measured
    // the resulting tick-1 rand() draw). 0x00438610 calls a node's draw
    // (+0xdc) only on the slices that frame draws: one behind the ring
    // camera (front - 0x5000) up to camera + 0x1e00, the static-box reach.
    // A pedestrian beyond it stays unloaded and keeps thinking as one
    // (city-parked-randlog-20261102: the parked-camera crosser 10000 Q8
    // ahead walks at the unloaded pace 4). 0x00438610's lateral dispatch
    // bounds are world coordinates, not projected screen x; the static
    // strip's broad bounds also admit these pedestrian nodes.
    if (!pedestrians_) return;
    const int32_t camera =
        std::max<int32_t>(ring_ ? ring_->front_q8() - 0x5000 : int32_t(camera_position()), 0);
    const size_t first = size_t(std::max<int32_t>((camera >> 8) - 1, 0));
    for (auto& p : pedestrians_->pool()) {
        if (!p.active) continue;
        p.pre_draw_resources = {p.loaded(), p.has(0), p.has(5)};
        if (p.loaded()) continue;
        if (p.body.sample < first || int64_t(p.body.sample) * 256 > int64_t(camera) + 0x1e00) continue;
        const auto& road = route_of(riders_[0]);
        // 0x0044acd0 resolves a global family slot, advanced by the front
        // MSRR walker. Query its global front coordinate directly: subtracting
        //and re-adding view_slices inside a section can fire events beyond
        //that section's actual terminal cursor.
        const size_t front =
            ring_ ? size_t(std::max(0, ring_->front_q8() >> 8)) : p.body.sample;
        const auto* current = ring_ && pedestrians_->has_resident_sources()
            ? pedestrians_->resident_look(ring_->resident_family(p.selector >> 6), p.selector)
            : pedestrians_->look(road, std::min(front, road.size() - 1), p.selector);
        if (!current) continue;
        p.look = current;
        p.graphics_evicted = false;
        if (p.look) { // 0x00440b80
            set_animation(p.animation, p.look->bank, 1, uint32_t(ticks_));
            queue_animation(p.animation, p.look->bank, -1);
            advance_animation(p.animation, p.look->bank, uint32_t(ticks_));
        }
    }
}

void Race::run_ped_node(Pedestrian& p) {
    // 0x00437e60 for a +0x20 node: dt = tick - last run; the next interval is
    // 1 within 4 samples of the ring camera (1 - d when more than 2 behind),
    // else (d + 1) / 2. Parked outside the ring window or beyond lateral
    // +-0xc8000.
    const TrackRing* nearest = nearest_ring(p.body.bike.i32(0x1c));
    const int32_t ring = nearest ? nearest->front_q8() - 0x5000 : int32_t(camera_position()) + 0x100;
    const int32_t front = ring + 0x5000, back = ring - 0x3000;
    Bike& b = p.body.bike;
    b.i32(0x18) = ticks_ - p.last_tick;
    p.last_tick = ticks_;
    // Solo oracle freeze (proxy sim_freeze_others runs at the start of every
    // step from green, before the node's first scheduled run): 0x00437e60
    // skips the think when +0xe0 is NULL, so a frozen pedestrian never
    // thinks, moves, or recomputes +0x14. The scheduler metadata (+0x18 dt,
    // +0x10 last, +0xc next) and the window test still run, which is why the
    // frozen nodes stay in place and leave the list on a scheduled run once
    // the ring passes them.
    if (!freeze_actors_) {
        think_pedestrian(p);
        move_pedestrian(p);
        const int32_t d = sar8(b.i32(0x1c) - ring);
        const int32_t interval = d < 4 ? (d < -2 ? 1 - d : 1) : (d + 1) / 2;
        b.i32(0x14) = interval;
    }
    p.next_tick = ticks_ + b.i32(0x14);
    if (b.i32(0x28) < -0xc8000 || b.i32(0x28) > 0xc8000 || front - b.i32(0x1c) < 0 ||
        b.i32(0x1c) - back < 0) {
        // Nulled-callback solo replays keep their measured same-step removal
        // contract; only the live move-slot path defers unlink.
        if (freeze_actors_) {
            pedestrians_->recycle(p);
            return;
        }
        // 0x00437c10 -> 0x00437be0: disabled now, still in the list through
        // this frame; next tick's prepass returns the pool slot.
        p.pending_remove = true;
        b.u8(0xc0) = uint8_t((b.u8(0xc0) & ~4) | 8);
        p.last_tick = p.next_tick = ticks_ + 100;
    }
}

void Race::step_pedestrians() {
    // Legacy (non-live scheduler) captures keep the separate pass; the live
    // scheduler interleaves pedestrian nodes by position in schedule_pass.
    if (!pedestrians_) return;
    for (auto& p : pedestrians_->pool()) {
        // 0x00437df0 removes last tick's marked nodes before scheduling.
        if (p.pending_remove) {
            pedestrians_->recycle(p);
            continue;
        }
        if (!p.active || p.next_tick > ticks_) continue;
        run_ped_node(p);
    }
}

size_t Race::pedestrian_sample(const Pedestrian& p) const {
    const auto& road = route_of(p.body);
    if (const auto* ring = live_schedule_ ? nearest_ring(p.body.bike.i32(0x1c)) : nullptr) {
        const int64_t back = ring->back_q8() >> 8;
        return size_t(std::clamp<int64_t>(back + ((int64_t(p.body.sample) - back) & 127),
                                          0, int64_t(road.size() - 1)));
    }
    return p.body.sample;
}

int Race::pedestrian_lane(const Pedestrian& p) const {
    // 0x0044a020(obj, 0): lanes of 250 world units from the slice centre.
    const auto& s = route_of(p.body)[pedestrian_sample(p)];
    const int32_t d = int32_t(uint32_t(p.body.bike.i32(0x28)) - uint32_t(s.offset * 256));
    return int8_t(int32_t(int64_t(sar8(d)) / 250) - (d < 0 ? 1 : 0));
}

bool Race::pedestrian_road_clear(const Pedestrian& p, int factor, int delta) const {
    // 0x00441500 -> 0x0044a070(obj, lane, lane < 0 ? 0 : 2, 0x14, -1): the
    // nearest bike or car in that lane, ahead for the oncoming (negative)
    // lanes and behind otherwise. Clear when none, or when the gap beats
    // its speed scaled by the crossing factor and cruise DAT_004b7e8c.
    const int8_t lane = int8_t(pedestrian_lane(p) + delta);
    const bool ahead = lane < 0;
    const int32_t pos = p.body.bike.i32(0x1c);
    bool found = false;
    int32_t best_gap = 0, other_pos = 0, other_velocity = 0;
    auto consider = [&](int32_t opos, int32_t olateral, int32_t ooffset, int32_t velocity) {
        if (ahead ? opos <= pos : opos >= pos) return;
        const int32_t d = int32_t(uint32_t(olateral) - uint32_t(ooffset * 256));
        if (int8_t(int32_t(int64_t(sar8(d)) / 250) - (d < 0 ? 1 : 0)) != lane) return;
        const int32_t gap = std::abs(opos - pos);
        if (found && gap >= best_gap) return;
        found = true;
        best_gap = gap;
        other_pos = opos;
        other_velocity = velocity;
    };
    for (const auto& r : riders_) {
        if (!r.bike_slice_present)
            continue; // 0x00449ce0 skips nodes without a slice (+0x2c), e.g. parked bikes
        const auto& sample = route_of(r)[r.sample];
        if (sample.dual && r.route != p.body.route) continue; // 0x00449ce0 roadway-half test
        consider(r.bike.i32(0x1c), r.bike.i32(0x28), sample.offset, r.bike.i32(0x108));
    }
    if (traffic_) {
        const auto& road = route_of(riders_[0]);
        for (const auto& car : traffic_->cars()) {
            if (!car.active || car.position < 0) continue;
            const auto& own=snapshot_authority_?route_for(car.road):road;
            const size_t s = std::min(size_t(car.position) >> 8, own.size() - 1);
            if(snapshot_authority_&&!same_collision_road(p.body.route,p.body.sample,car.road,s))continue;
            consider(car.position, car.lateral, own[s].offset, car.motion.i32(0x108));
        }
    }
    if (!found) return true;
    const uint32_t v = uint32_t(magnitude(other_velocity));
    const int32_t sum = int32_t(uint32_t(int32_t(v & 0xfffff00fu) >> 4) +
                                uint32_t(int32_t(v & 0xfffffe01u) >> 1) + (v & 0xffffff80u) * 2u);
    const int32_t cruise = traffic_ ? traffic_->timers().cruise : 25;
    const int32_t q = sum / (cruise + 1);
    const uint32_t f = uint32_t(factor * 0x191) >> 8;
    const int32_t lhs = int32_t((uint32_t(q) * f) & 0xffffff0fu) >> 4;
    const int32_t rhs =
        int32_t(uint32_t(magnitude(int32_t(uint32_t(pos) - uint32_t(other_pos)))) & 0xfffffff0u);
    return lhs <= rhs;
}

bool Race::pedestrian_arrived(const Pedestrian& p) const {
    // 0x004414a0: past the middle of the far shoulder (+0xe4/+0xec right,
    // +0xe0/+0xe8 left).
    const SliceEdges e = edges(p.body, pedestrian_sample(p));
    const int32_t lateral = p.body.bike.i32(0x28);
    if (p.side == 1) return ((e.right + e.right_shoulder) / 2) * 256 < lateral;
    return lateral < ((e.left + e.left_shoulder) / 2) * 256;
}

void Race::pedestrian_voice(Pedestrian& p, int which) {
    // 0x00436830(0x16, {voice}): the 'MROF' word byte-swapped by 0x00440730;
    // 0x004351f0 plays it as the speech event.
    p.voice_tick = ticks_;
    if (sound_events_.size() < 10)
        sound_events_.push_back({SoundEvent::speech, 0, 0, 0, 0, p.look->voices[size_t(which)],
                                 pedestrians_ ? 3000 + int32_t(&p - pedestrians_->pool().data()) : -1});
}

void Race::think_pedestrian(Pedestrian& p) {
    // 0x00440dd0.
    Bike& b = p.body.bike;
    const int32_t player = snapshot_authority_
        ? nearest_human(p.body.bike.i32(0x1c)).half_mirror.position : player_half_position();
    const uint32_t tick = uint32_t(ticks_);
    auto set = [&](int8_t code) {
        if (p.look) set_animation(p.animation, p.look->bank, code, tick);
    };
    auto queue = [&](int8_t code) {
        if (p.look) queue_animation(p.animation, p.look->bank, code);
    };
    auto rest = [&] {
        p.state = 0;
        set(1);
        queue(-1);
        p.pace = 0;
    }; // LAB_004413ea
    auto resume = [&] {
        p.state = 1;
        p.pace = 4;
        set(0);
        queue(-1);
    };
    // 0x004415b0: on a crosswalk edge slice, the phase of the index-0/2
    // signal for this side of the road, or -1. Phase 2 walks without a road
    // check; other phases hold; no signal checks the road.
    const int signal = traffic_
                           ? traffic_->pedestrian_signal(route_of(p.body)[pedestrian_sample(p)].path.marking_flags,
                                                         b.i32(0x1c), pedestrian_lane(p), snapshot_authority_ ? p.body.route : -1)
                           : -1;
    p.pace = 0;
    if (p.state != 2) {
        p.heading = crossing_heading[p.side & 3];
        int base = -1;
        switch (p.type) {
        case 4:
            p.heading = int32_t(0xff800800u);
            break;
        case 5:
            base = 0xfe;
            break; // 0x7f + 0x7f falling through to case 7
        case 6:
            base = 0x7f;
            break;
        case 7:
            base = 0;
            break;
        case 10:
            if (p.loaded() && p.state != 7) {
                p.state = 7;
                set(6);
            }
            break;
        default:
            break;
        }
        if (base >= 0) {
            const int factor = uint8_t(base + 1);
            switch (p.state) {
            case 0:
                p.lane = int8_t(pedestrian_lane(p));
                if ((p.has(0) || p.has(5) || !p.loaded()) && p.trigger < player && (random() & 1) != 0) {
                    if (signal >= 0 && signal != 2) break; // red phases hold
                    if (signal < 0) {
                        if (!pedestrian_road_clear(p, factor, 0)) break;
                        if (!pedestrian_road_clear(p, factor, p.side == 1 ? 1 : -1)) break;
                    }
                    p.state = 1;
                    p.pace = p.has(0) ? 4 : 8;
                    if (!p.loaded())
                        set(0);
                    else
                        set(p.has(0) ? 0 : 5);
                }
                break;
            case 1:
                if (p.has(0)) {
                    queue(0);
                    p.pace = 4;
                } else if (p.has(5)) {
                    queue(5);
                    p.pace = 8;
                } else if (!p.loaded())
                    p.pace = 4;
                else {
                    queue(1);
                    p.pace = 0;
                }
                if (signal >= 0 || int8_t(pedestrian_lane(p)) == p.lane) {
                    if ((random() & 3) == 0 || pedestrian_road_clear(p, (factor >> 2) + 0x10, 0)) {
                        if (pedestrian_arrived(p)) {
                            p.state = 0;
                            queue(1);
                            p.pace = 0;
                            p.trigger = player + 0xa0;
                            p.side = (p.side + 2) & 3;
                        } else if (signal >= 0 && signal != 2 && p.has(5))
                            queue(5); // red mid-crossing: hurry
                    } else {
                        // Traffic too close mid-crossing: hesitate.
                        p.state = 3;
                        if (p.has(2)) set(2);
                        if (!p.has(4) || (p.has(5) && (random() & 7) == 0)) {
                            if (p.has(5)) queue(5);
                        } else {
                            if (uint32_t(ticks_ - p.voice_tick) > 0xb4 && p.look && p.look->voices[1] >= 0)
                                pedestrian_voice(p, 1);
                            queue(4);
                        }
                    }
                } else {
                    p.lane = int8_t(pedestrian_lane(p));
                    if (!pedestrian_road_clear(p, factor, 0)) rest();
                }
                break;
            case 3: {
                const int8_t current = p.animation.animation;
                if (current == 2) break;
                if (current == 4 || current == 5) {
                    p.state = current;
                    queue(p.has(0) ? 0 : p.has(5) ? 5 : 1);
                } else if (!p.has(0)) {
                    if (p.has(5))
                        queue(5);
                    else
                        rest();
                } else
                    resume();
                break;
            }
            case 4:
                if (p.animation.animation != 4) {
                    if (!p.has(0)) {
                        if (p.has(5))
                            queue(5);
                        else
                            rest();
                    } else
                        resume();
                }
                break;
            case 5:
                p.pace = 8;
                if (pedestrian_road_clear(p, factor, 0)) {
                    if (!p.has(0)) {
                        if (p.has(5))
                            queue(5);
                        else
                            rest();
                    } else
                        resume();
                }
                break;
            default:
                break;
            }
        }
        // 0x004498c0(obj, heading, pace*100, pace*100).
        const int32_t heading = p.heading;
        b.i32(0x128) = heading;
        int32_t speed = b.i32(0x18) * (p.pace * 100) + b.i32(0xec);
        if (p.pace * 100 < speed) speed = p.pace * 100;
        b.i32(0xec) = speed;
        b.i32(0x5c) = heading;
    }
    if (p.loaded()) {
        //The global family slots follow the player's committed front walker,
        //not a pedestrian's retained historical split arm.
        const auto& road = route_of(riders_[0]);
        const size_t front =
            ring_ ? size_t(std::max(0, ring_->front_q8() >> 8)) : p.body.sample;
        if (ring_ && std::getenv("RR_SIM_TRACE_TICK") && ticks_ == std::atoi(std::getenv("RR_SIM_TRACE_TICK"))) {
            const auto state = ring_->diagnostic_state();
            const auto& s = road[std::min(front, road.size() - 1)];
            std::fprintf(stderr, "ped-family-cursor serial %u ped-route %d player-route %d query %zu section %u local %u physical-node %u physical-position %d\n",
                         unsigned(p.serial), p.body.route, riders_[0].route, front, s.section, s.local,
                         state.front.node, state.front.position);
        }
        const bool valid = ring_ && pedestrians_->has_resident_sources()
            ? pedestrians_->resident_graphics_valid(ring_->family_history(p.selector >> 6), *p.look)
            : pedestrians_->graphics_valid(road, std::min(front, road.size() - 1), p.selector, *p.look);
        if (!valid) {
            // 0x0044143b..0x00441474: replaced graphics lose their RRAN
            // header. Only pointers +0/+4/+0xc are cleared; animation
            // code/frame/queue/deadline remain, as do bank-table +8 and
            // availability words. State3 can keep waiting on retained code2.
            p.graphics_evicted = true;
            p.animation.selected.reset();
        } else {
            advance_animation(p.animation, p.look->bank, tick);
        }
    }
}

void Race::move_pedestrian(Pedestrian& p) {
    // 0x00440990: 0x0043adc0 (independent actor motion), then stay within
    // 0x1ff of the spawn position.
    Bike& b = p.body.bike;
    p.body.wall_previous_sample = p.body.sample;
    if (!b.i32(0x160)) {
        b.i32(0x114) = b.i32(0x118) = 0;
        b.u8(0x164) = 0;
    }
    size_t surface_sample = p.body.sample;
    if (const auto* ring = live_schedule_ ? nearest_ring(b.i32(0x1c)) : nullptr) {
        const int64_t back = ring->back_q8() >> 8;
        surface_sample = size_t(std::clamp<int64_t>(back + ((int64_t(surface_sample) - back) & 127),
                                                  0, int64_t(route_of(p.body).size() - 1)));
    }
    const auto surface_edges = edges(p.body, surface_sample);
    const int surface = surface_class(surface_edges, b.i32(0x28));
    if (std::getenv("RR_SIM_TRACE_TICK") && ticks_ == std::atoi(std::getenv("RR_SIM_TRACE_TICK")))
        std::fprintf(stderr, "ped-integrate serial %u dt %d pos %d lat %d speed %d heading %d surface %d type %u flags %u edges %d/%d shoulders %d/%d retained %zu\n",
                     unsigned(p.serial), int32_t(b.i32(0x18)), int32_t(b.i32(0x1c)), int32_t(b.i32(0x28)),
                     int32_t(b.i32(0xec)), int32_t(b.i32(0x128)), surface, unsigned(surface_edges.type),
                     unsigned(surface_edges.flags), surface_edges.left, surface_edges.right,
                     surface_edges.left_shoulder, surface_edges.right_shoulder, surface_sample);
    integrate_speed(b, tables_, surface);
    advance_actor_motion(b, tables_, [&](int32_t distance) { move_along(p.body, distance); });
    //0043adf1 runs the shared wall/shoulder response for pedestrians too,
    //before vertical settling. City6207 clamps the walker to the shoulder.
    walls(p.body);
    vertical(p.body);
    const int32_t gap = int32_t(uint32_t(p.anchor) - uint32_t(b.i32(0x1c)));
    if (magnitude(gap) > 0x1ff) {
        move_along(p.body, gap >= 0 ? gap - 0x1ff : gap + 0x1ff, true);
        b.i32(0xec) = 0;
    }
}

void Race::hit_pedestrian(Pedestrian& p, int32_t hitter_speed, bool hitter_attacking, bool hitter_walking) {
    // 0x00440760, the pedestrian's +0x1a4 handler.
    if (!p.loaded()) return;
    Bike& b = p.body.bike;
    const uint32_t tick = uint32_t(ticks_);
    if (p.state != 2) {
        if (hitter_attacking || (magnitude(hitter_speed) > 1000 && !hitter_walking)) {
            // Knocked down: scream when 0..0x400 ahead of the view camera.
            const int32_t ahead = b.i32(0x1c) - int32_t(camera_position());
            if (ahead >= 0 && ahead < 0x401 && uint32_t(ticks_ - p.voice_tick) > 0xb4 &&
                p.look->voices[0] >= 0)
                pedestrian_voice(p, 0);
            p.state = 2;
            set_animation(p.animation, p.look->bank, 3, tick);
            queue_animation(p.animation, p.look->bank, -1);
            b.i32(0x154) = 0x19;
            b.i32(0x128) = 0;
            b.i32(0x5c) = 0; // 0x004498c0(obj, 0, 20000, 0)
            if (20000 < b.i32(0xec)) b.i32(0xec) = 20000;
        } else {
            // Bumped: stagger.
            p.state = 3;
            if (p.has(2)) set_animation(p.animation, p.look->bank, 2, tick);
            if (!p.has(4) || (p.has(5) && (random() & 7) == 0))
                queue_animation(p.animation, p.look->bank, p.has(5) ? 5 : 1);
            else {
                queue_animation(p.animation, p.look->bank, 4);
                if (uint32_t(ticks_ - p.voice_tick) > 0xb4 && p.look->voices[1] >= 0) pedestrian_voice(p, 1);
            }
        }
    }
    if (p.state != 2) {
        const int32_t pushed = sar4(hitter_speed);
        b.i32(0xec) = pushed < 0 ? 0 : pushed;
        b.i32(0x128) = hitter_speed > 0 ? 0 : 0x7f;
    }
}

void Race::contact_pedestrian(Rider& self, Pedestrian& p) {
    // 0x004049c0 for a short object (+0x17c 0x1e00 < 0x2000): no knock-off
    // decision. Upright pedestrians (state != 2) only nudge the rider below
    // speed 2000 (500 for one already down); faster riders ride up over them.
    Bike& s = self.bike;
    if (s.i32(0x2f4) == 0) self.half_object.u8(0x1e9) = 0; // 00404b2c
    const Bike& q = p.body.bike;
    const int32_t steer = s.i32(0xf0);
    if (human(self)) {
        const int32_t hit = magnitude(s.i32(0x184));
        if (hit < 0x1771) {
            if (hit >= 0x7d1 || magnitude(s.i32(0x188)) >= 0x3e9) {
                sound(SoundEvent((random() & 1) + 3), s);
                sound(SoundEvent(random() & 1), s);
            }
        } else {
            sound(SoundEvent((random() & 1) + 4), s);
            sound(SoundEvent((random() & 1) + 1), s);
        }
    }
    const int32_t elasticity = s.i32(0x180);
    s.i32(0x188) = div256(int64_t(s.i32(0x188)) * elasticity);
    s.i32(0x18c) = div256(int64_t(s.i32(0x18c)) * elasticity);
    s.i32(0x184) = div256(int64_t(s.i32(0x184)) * elasticity);
    const int32_t speed = s.i32(0xec);
    bool nudge;
    if (speed < 300 && s.u8(0x19c) == 0x20) {
        move_along(self, s.i32(0x18) * 2, false);
        nudge = true;
    } else if (p.state != 2)
        nudge = speed < 2000;
    else
        nudge = speed < 500;
    if (nudge) {
        const int32_t step = sar4(s.i32(0x174) + q.i32(0x174)) * s.i32(0x18);
        s.i32(0x28) += q.i32(0x28) < s.i32(0x28) ? step : -step;
    } else {
        s.i32(0x20) += q.i32(0x17c) / 2;
        int32_t t = int32_t(uint32_t(q.i32(0x17c)) * uint32_t(magnitude(s.i32(0x184))) * 2u);
        t += (t >> 31) & 0x3fff;
        s.i32(0xf4) += (((t >> 14) + ((t >> 31) & 0xf)) >> 4) * 4;
    }
    (void)steer; // +0xf0 is unchanged on this branch, so the steer-rate tail is skipped
    if (s.i32(0xf4) > 0x600) s.i32(0xf4) = 0x600;
    s.i32(0x184) = s.i32(0x188) = s.i32(0x18c) = 0;
    s.u8(0x19c) = 0;
    if (s.i32(0xec) > 30000) s.i32(0xec) = 30000;
    if (s.i32(0xec) < -1000) s.i32(0xec) = -1000;
    s.i32(0xf4) = std::clamp(int32_t(s.i32(0xf4)), -10000, 10000);
    if (live_schedule_ && s.i32(0x2f4) == 0 && finish_q8_ != 0)
        self.finish_distance = remaining_finish_distance(self, self.half_mirror.position);
}

void Race::contact_rider_pedestrian(Rider& r, Pedestrian& p, bool rider_outer) {
    // 0x004368f0 pair of a bike (4) and a pedestrian (0x20): separation
    // 0x00436a40 in walk orientation (mass +0x194: pedestrian 100), then the
    // outer node's handler first: the rider's 0x004049c0 path, the
    // pedestrian's 0x00440760.
    const int32_t hitter_speed = r.bike.i32(0xec);
    if (rider_outer)
        separate(r, 4, p.body, 0x20);
    else
        separate(p.body, 0x20, r, 4);
    if (rider_outer) {
        contact_pedestrian(r, p);
        hit_pedestrian(p, hitter_speed, ai_attack_anim(r) != 0, false);
    } else {
        hit_pedestrian(p, hitter_speed, ai_attack_anim(r) != 0, false);
        contact_pedestrian(r, p);
    }
}

void Race::collide_pedestrians() {
    // Pedestrian against free cars (the rider pairs run in contact_pass).
    if (!pedestrians_ || !traffic_) return;
    for (auto& p : pedestrians_->pool()) {
        if (!p.active || p.pending_remove) continue;
        Bike& q = p.body.bike;
        for (auto& car : traffic_->cars_mut()) {
            if (!car.active || !car.collides) continue;
            if(snapshot_authority_&&!same_collision_road(p.body.route,p.body.sample,car.road,size_t(std::max(car.position,0)>>8)))continue;
            const int32_t car_half_width = TrafficSystem::car_half_width(car);
            if (std::abs(car.position - q.i32(0x1c)) > car_half_length + q.i32(0x178)) continue;
            if (std::abs(car.lateral - q.i32(0x28)) > car_half_width + q.i32(0x174)) continue;
            if (std::abs(q.i32(0x20)) > car_half_height + q.i32(0x17c)) continue;
            // Lateral separation only (the car keeps no previous-position copy).
            const int32_t depth =
                (car_half_width + q.i32(0x174) - std::abs(car.lateral - q.i32(0x28))) + 0x800;
            const int32_t back = depth * car_mass / (car_mass + q.i32(0x194)) + 3;
            q.i32(0x28) += car.lateral < q.i32(0x28) ? back : -back;
            hit_pedestrian(p, car.kind == 2 ? -car.speed : car.speed, false, false);
        }
    }
}
void Race::attach_terrain_type() {
    if (traffic_ && terrain_source_) {
        traffic_->set_obstacle_scan([this](const TrafficCar& car, int32_t reach) -> std::optional<TrafficObstacle> {
            build_scan_list();
            int at = -1;
            const int index = int(&car - traffic_->cars().data());
            for (size_t i = 0; i < scan_.size(); ++i)
                if (scan_[i].car == index) { at = int(i); break; }
            const int32_t origin = car.position;
            auto lateral_next = [&](int from) {
                if (from < 0) return -1;
                const auto self = scan_[size_t(from)];
                auto side = [&](bool forward) {
                    int current = from;
                    for (;;) {
                        const int32_t range = 128 + (forward ? self.position - scan_[size_t(current)].position
                                                                           : scan_[size_t(current)].position - self.position);
                        current = scan_next(current, 0x3f, forward, range);
                        if (current < 0) return -1;
                        const int32_t x = scan_[size_t(current)].lateral;
                        if (x != self.lateral && (self.lateral < x) == (car.kind == 1)) return current;
                    }
                };
                const int forward = side(true), backward = side(false);
                if (forward < 0) return backward;
                if (backward < 0) return forward;
                return (scan_[size_t(forward)].lateral > scan_[size_t(backward)].lateral) == (car.kind == 1)
                           ? backward : forward;
            };
            for (;;) {
                at = (car.kind & 1) ? lateral_next(at) : scan_next(at, 0x3f, car.kind == 0, reach);
                if (at < 0) return std::nullopt;
                const auto& o = scan_[size_t(at)];
                if (car.kind & 1) {
                    if (((o.position ^ origin) & ~255) != 0) continue;
                } else {
                    const int32_t delta = int32_t(uint32_t(o.lateral) - uint32_t(o.sample->offset * 256));
                    const int lane = int8_t(int32_t(int64_t(delta / 256) / 250) - (delta < 0 ? 1 : 0));
                    if (lane != car.lane) continue;
                }
                int32_t across = 0;
                if (o.car >= 0) across = traffic_->cars()[size_t(o.car)].motion.i32(0x10c);
                else if (o.pedestrian >= 0) across = pedestrians_->pool()[size_t(o.pedestrian)].body.bike.i32(0x10c);
                else if (o.rider >= 0) {
                    const auto& r = riders_[size_t(o.rider)];
                    across = o.type == 4 ? r.bike.i32(0x10c)
                             : r.crash_active ? r.crash_actor.motion.i32(0x10c) : r.half_mirror.lat_vel;
                }
                return TrafficObstacle{o.position, o.lateral, o.speed, across, o.type};
            }
        });
        traffic_->set_ring_source([this](int32_t position) { return nearest_ring(position); });
        traffic_->set_terrain_type([this](const RouteSample& s) { return int(ground(s).type); });
        traffic_->set_road_motion([this](int32_t position) -> std::optional<std::pair<int32_t, int32_t>> {
            const auto* ring = nearest_ring(position);
            if (!ring) return std::nullopt;
            const auto& route = route_of(riders_[0]);
            const size_t sample = size_t(std::clamp(position >> 8, 0, int32_t(route.size()) - 1));
            return ring->retained_motion(position, route[sample].half);
        });
        traffic_->set_wall_response([this](Bike& body, size_t current, size_t previous) {
            Rider actor;
            actor.bike = body;
            actor.route = riders_[0].route;
            actor.sample = current;
            actor.previous_sample = previous;
            actor.wall_previous_sample = previous;
            if (std::getenv("RR_SIM_TRACE_TICK") && ticks_ == std::atoi(std::getenv("RR_SIM_TRACE_TICK"))) {
                const auto s = sub_slice(actor, current);
                std::fprintf(stderr, "car-wall tick %d pos %d lat %d samples %zu/%zu type %u flags %u edges %d/%d shoulders %d/%d halfwidth %d\n",
                             ticks_, int32_t(body.i32(0x1c)), int32_t(body.i32(0x28)), previous, current,
                             unsigned(s.type), unsigned(s.flags), s.e0, s.e4, s.e8, s.ec, int32_t(body.i32(0x174)));
            }
            walls(actor);
            body = actor.bike;
        });
    }
}
} // namespace rr
