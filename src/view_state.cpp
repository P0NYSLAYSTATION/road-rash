#include "view_state.hpp"
#include "traffic.hpp"
#include <algorithm>

namespace rr {
namespace {
// Q16 blend of int32 fields; Q8 positions wrap-aware on uint32.
int32_t blend_i32(int32_t a, int32_t b, uint32_t alpha_q16) {
    return int32_t(int64_t(a) + ((int64_t(b) - int64_t(a)) * alpha_q16 >> 16));
}
// Shortest signed modular blend for wrapped quarter/factor (position-like uint32).
uint32_t blend_u32_wrap(uint32_t a, uint32_t b, uint32_t alpha_q16) {
    const uint32_t wrapped = b - a;
    const int64_t d = wrapped <= INT32_MAX ? int64_t(wrapped) : int64_t(wrapped) - (int64_t(1) << 32);
    return uint32_t(int64_t(a) + (d * alpha_q16 >> 16));
}
// Wrapped blend for heading (period 0x1000000, as in wrap_angle).
int32_t blend_heading(int32_t a, int32_t b, uint32_t alpha_q16) {
    constexpr int32_t period = 0x1000000;
    int64_t d = int64_t(b) - int64_t(a);
    d %= period;
    if (d > period / 2)
        d -= period;
    else if (d < -period / 2)
        d += period;
    int64_t r = (int64_t(a) + (d * alpha_q16 >> 16)) % period;
    if (r > period / 2)
        r -= period;
    else if (r < -period / 2)
        r += period;
    return int32_t(r);
}
// The fatal cliff branch resets ground and relative height together. Bridge
// that discontinuity in the visible pose using the actor's vertical velocity
// and gravity at 60 Hz. Shadows stay on physical ground; this never feeds
// physics, contacts, wreck timing or RNG.
void cliff_pose(ActorPose& pose, CliffFallPose& state, const CliffFallPose& bike_before,
                const Bike& actor, uint32_t tick, bool fatal) {
    const int32_t ground = pose.height_q8;
    const int64_t native_world = int64_t(ground) * 256 + pose.vertical_q8;
    const auto previous = state.initialized ? state : bike_before;
    const bool consecutive = previous.initialized && tick == previous.tick + 1;
    if (!fatal || !pose.visible || tick < state.tick) state.active = false;
    if (!state.active && fatal && pose.visible && consecutive && ground < 0 &&
        int64_t(previous.ground) - ground >= 70) {
        state.active = true;
        state.world_q8 = native_world + (int64_t(previous.ground) - ground) * 256;
        state.velocity_q8 = actor.i32(0xf4);
    } else if (state.active && tick > state.tick) {
        const int64_t dt = tick - state.tick;
        const int64_t gravity = -std::max<int32_t>(1, actor.i32(0x1b4));
        state.world_q8 += state.velocity_q8 * dt + gravity * dt * (dt - 1) / 2;
        state.velocity_q8 += gravity * dt;
        if (state.world_q8 <= int64_t(ground) * 256 && state.velocity_q8 <= 0)
            state.active = false;
    }
    if (state.active) pose.vertical_q8 = int32_t(state.world_q8 - int64_t(ground) * 256);
    state.ground = ground;
    state.tick = tick;
    state.initialized = true;
}
} // namespace

void capture_view(ViewState& v, const Race& race, const TrafficSystem* traffic, bool presentation_motion) {
    v.camera_position_q8 = race.camera_position();
    v.camera = race.camera();
    v.camera_origin = race.camera_x() - v.camera.x;
    v.camera_route = 0;
    v.tick = uint32_t(race.ticks());
    v.actors.clear();
    const auto& riders = race.riders();
    if (presentation_motion) v.cliff_falls.resize(riders.size() * 2);
    for (size_t i = 0; i < riders.size(); ++i) {
        const auto& rd = riders[i];
        ActorPose p;
        p.id = uint32_t(i + 1);
        p.position_q8 = uint32_t(rd.crash_active ? rd.bike.i32(0x1c) : rd.half_mirror.position);
        p.lateral_q8 = rd.crash_active ? rd.bike.i32(0x28) : rd.half_mirror.lateral;
        p.height_q8 = rd.bike.i32(0x24);
        p.vertical_q8 = rd.crash_active ? rd.bike.i32(0x20) : rd.half_mirror.height;
        p.heading = rd.bike.i32(0x5c);
        p.roll = rd.crash_active ? rd.bike.i32(0x60) : rd.half_mirror.yaw;
        p.route = rd.crash_active ? rd.route : rd.half_mirror.route;
        p.visible = rd.live && !rd.parked && (rd.crash_active ? rd.bike_slice_present : rd.half_slice_present);
        if(presentation_motion && rd.ai && p.visible && !rd.frozen && !rd.half_frozen && !rd.crash_active && race.live_schedule()) {
            // A distant bike integrates several ticks at once. Adjacent-tick
            // interpolation alone holds its pose, then displays that whole step.
            // Predict only the unsimulated presentation interval; simulation,
            // collision membership and the RNG stream remain untouched.
            const int32_t elapsed=std::clamp(race.ticks()-rd.sched_last,0,7);
            const int64_t position=int64_t(p.position_q8)+int64_t(rd.half_mirror.lat108)*elapsed/256;
            const auto& road=race.route_for(p.route);
            if(position>=0 && size_t(position>>8)<road.size() && size_t(p.position_q8>>8)<road.size() &&
                road[size_t(position>>8)].node==road[size_t(p.position_q8>>8)].node) {
                p.position_q8=uint32_t(position);
                p.lateral_q8=int32_t(int64_t(p.lateral_q8)+int64_t(rd.half_mirror.lat_vel)*elapsed);
            }
        }
        const auto bike_before = presentation_motion ? v.cliff_falls[2 * i] : CliffFallPose{};
        const bool fatal = rd.crash_active && rd.bike.i32(0x2f0) == 0;
        if (presentation_motion) cliff_pose(p, v.cliff_falls[2 * i], bike_before, rd.bike, v.tick, fatal);
        v.actors.push_back(p);
        if (rd.crash_active) {
            ActorPose c;
            c.id = uint32_t(1000 + i);
            c.position_q8 = uint32_t(rd.crash_actor.motion.i32(0x1c));
            c.lateral_q8 = rd.crash_actor.motion.i32(0x28);
            c.height_q8 = rd.crash_actor.motion.i32(0x24);
            c.vertical_q8 = rd.crash_actor.motion.i32(0x20);
            c.heading = rd.crash_actor.motion.i32(0x5c); // walk-back facing (0x0040a600)
            c.roll = rd.crash_actor.motion.i32(0x60);
            c.route = rd.crash_actor.route;
            c.visible = rd.live && !rd.parked && rd.half_slice_present;
            if (presentation_motion)
                cliff_pose(c, v.cliff_falls[2 * i + 1], bike_before, rd.crash_actor.motion, v.tick, fatal);
            v.actors.push_back(c);
        } else if (presentation_motion) v.cliff_falls[2 * i + 1] = {};
    }
    if (traffic) {
        const auto& cars = traffic->cars();
        for (size_t i = 0; i < cars.size(); ++i) {
            if (!cars[i].active) continue;
            ActorPose p;
            p.id = uint32_t(2000 + i);
            p.position_q8 = uint32_t(cars[i].position);
            p.lateral_q8 = cars[i].lateral;
            p.route = 0;
            p.visible = true;
            v.actors.push_back(p);
        }
    }
}

void blend_view(ViewState& out, const ViewState& a, const ViewState& b, uint32_t alpha_q16,
                uint32_t teleport_q8) {
    out.tick = b.tick;
    out.camera_route = b.camera_route;
    out.camera = b.camera;
    out.camera_origin = b.camera_origin;
    if (a.camera_route != b.camera_route) {
        out.camera_position_q8 = b.camera_position_q8;
    } else if (uint32_t((b.camera_position_q8 - a.camera_position_q8) & 0xffffffffu) > teleport_q8 &&
               uint32_t((a.camera_position_q8 - b.camera_position_q8) & 0xffffffffu) > teleport_q8) {
        // Ambiguous large jump either way: teleport, draw at b.
        // (Forward wrap is small in (b-a); backward wrap small in (a-b). A
        // genuine teleport is large in both directions.)
        out.camera_position_q8 = b.camera_position_q8;
    } else {
        out.camera_position_q8 = blend_u32_wrap(a.camera_position_q8, b.camera_position_q8, alpha_q16);
    }
    out.actors.clear();
    out.actors.reserve(b.actors.size());
    for (const auto& pb : b.actors) {
        const ActorPose* pa = nullptr;
        for (const auto& q : a.actors)
            if (q.id == pb.id) {
                pa = &q;
                break;
            }
        if (!pa || !pa->visible || pa->route != pb.route) {
            out.actors.push_back(pb); // new, gone-before, or route-changed: draw at b
            continue;
        }
        const uint32_t fwd = pb.position_q8 - pa->position_q8;
        const uint32_t back = pa->position_q8 - pb.position_q8;
        if (fwd > teleport_q8 && back > teleport_q8) {
            out.actors.push_back(pb); // teleport: draw at b
            continue;
        }
        ActorPose m = pb;
        m.position_q8 = blend_u32_wrap(pa->position_q8, pb.position_q8, alpha_q16);
        m.lateral_q8 = blend_i32(pa->lateral_q8, pb.lateral_q8, alpha_q16);
        m.height_q8 = blend_i32(pa->height_q8, pb.height_q8, alpha_q16);
        m.vertical_q8 = blend_i32(pa->vertical_q8, pb.vertical_q8, alpha_q16);
        m.heading = blend_heading(pa->heading, pb.heading, alpha_q16);
        m.roll = blend_i32(pa->roll, pb.roll, alpha_q16);
        out.actors.push_back(m);
    }
}
void RemoteSmoothing::apply(ViewState& view, int humans, uint64_t now) {
    if (!active_) return;
    const uint64_t elapsed = now >= began_ ? now - began_ : 0;
    if (elapsed >= 100) {
        active_ = false;
        return;
    }
    ViewState blended;
    blend_view(blended, before_, view, uint32_t(elapsed * 65536 / 100));
    for (auto& actor : view.actors) {
        const bool remote = (actor.id >= 2 && actor.id <= uint32_t(humans)) ||
                            (actor.id >= 1001 && actor.id < uint32_t(1000 + humans));
        if (remote)
            for (const auto& smoothed : blended.actors)
                if (smoothed.id == actor.id) {
                    actor = smoothed;
                    break;
                }
    }
}
} // namespace rr
