#ifndef RR_DRIVE_PROGRESS_H
#define RR_DRIVE_PROGRESS_H
#include <stdint.h>

typedef struct RRDriveProgress {
    uint32_t last_tick, progress_tick, stalls;
    int32_t anchor;
    int initialized;
} RRDriveProgress;

/* Five seconds of simulation time, not presentation frames or wall time.
 * A sample (256 position units) of movement restarts the progress window.
 * Small collision jitter does not. Pauses do not advance this clock. */
static int rr_drive_stalled(RRDriveProgress* state, uint32_t tick, int32_t position, int enabled) {
    if (!enabled) {
        state->initialized = 0;
        return 0;
    }
    if (!state->initialized || tick < state->last_tick) {
        state->initialized = 1;
        state->anchor = position;
        state->progress_tick = state->last_tick = tick;
        return 0;
    }
    state->last_tick = tick;
    const int64_t distance = (int64_t)position - state->anchor;
    if (distance >= 256 || distance <= -256) {
        state->anchor = position;
        state->progress_tick = tick;
        return 0;
    }
    if (tick - state->progress_tick < 300) return 0;
    state->anchor = position;
    state->progress_tick = tick;
    ++state->stalls;
    return 1;
}
#endif
