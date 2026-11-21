#pragma once
#include "race.hpp"
#include <cstdint>
#include <vector>

namespace rr {
// Plan 21 Task 2: interpolated frame state. Positions/heights/angles blend
// between ticks; animation frames and sprite views come from the newest tick.
struct ActorPose {
    uint32_t id = 0;
    uint32_t position_q8 = 0;
    int32_t lateral_q8 = 0, height_q8 = 0, heading = 0, roll = 0;
    int route = 0;
    bool visible = false;
    int32_t vertical_q8 = 0; // body height above physical ground; shadow ground remains separate
};
struct CliffFallPose {
    uint32_t tick = 0;
    int32_t ground = 0;
    int64_t world_q8 = 0, velocity_q8 = 0;
    bool initialized = false, active = false;
};
struct ViewState {
    uint32_t camera_position_q8 = 0; // Race::camera_position()
    Camera camera;                   // x, height
    int32_t camera_origin = 0;        // physical slice offset used to make camera.x relative
    int camera_route = 0;
    std::vector<ActorPose> actors; // riders, crash riders, cars, cops, stable id order
    uint32_t tick = 0;
    std::vector<CliffFallPose> cliff_falls; // presentation history, two entries per rider
};
class TrafficSystem;
// Presentation motion fills distant AI scheduler gaps and bridges fatal cliff resets.
// False retains the raw original pose for oracle/component comparisons.
void capture_view(ViewState&, const Race&, const TrafficSystem*, bool presentation_motion = false);
void blend_view(ViewState& out, const ViewState& a, const ViewState& b, uint32_t alpha_q16,
                uint32_t teleport_q8 = 8 << 8);
// Correction-only smoothing: affects remote human poses, never the local
// rider, camera, AI, simulation state or the input timeline.
class RemoteSmoothing {
  public:
    void correction(const ViewState& drawn, uint64_t now_ms) {
        before_ = drawn;
        began_ = now_ms;
        active_ = true;
    }
    void apply(ViewState&, int humans, uint64_t now_ms);
    void reset() { active_ = false; }

  private:
    ViewState before_;
    uint64_t began_ = 0;
    bool active_ = false;
};
} // namespace rr
