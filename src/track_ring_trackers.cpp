#include "track_ring.hpp"
#include <algorithm>

namespace rr {

int TrackRing::create_tracker(TrackerRef slice, int32_t position, uint8_t kind) {
    // 00450d30: the allocator's four slots are shared by both lists.
    const auto free = std::find_if(trackers_.begin(), trackers_.end(), [](const Tracker& t) { return !t.active; });
    if (free == trackers_.end()) return -1;
    const size_t index = size_t(free - trackers_.begin());
    const auto& location = shoulders_[slice.slot].location[slice.half];
    *free = {true, kind, int32_t(location.second * 256) | (position & 255), slice};
    tracker_lists_[kind].insert(tracker_lists_[kind].begin(), index); // 00438a50
    return int(index);
}

void TrackRing::remove_tracker(size_t index) {
    auto& t = trackers_[index];
    auto& list = tracker_lists_[t.kind];
    list.erase(std::remove(list.begin(), list.end(), index), list.end());
    t.active = false;
}

void TrackRing::step_trackers(uint8_t kind, int32_t delta) {
    // 00450f90 saves the next link before the callback can insert/remove.
    const auto walk = tracker_lists_[kind];
    if (walk.empty()) return;
    auto& refs = mutable_tracker_refs();
    for (const size_t index : walk) {
        auto& t = trackers_[index];
        if (!t.active || t.kind != kind) continue;
        const int32_t before = t.position;
        t.position += delta;
        if ((before >> 8) == (t.position >> 8)) continue;
        const TrackerRef old = t.slice;
        const auto old_parent = shoulders_[old.slot];
        const uint32_t old_node = old_parent.location[old.half].first;
        refs[old_node][kind].reset();
        const uint8_t event = slice_events_[old.slot];
        const bool forward = delta > 0;
        // 00450dfc / 00450ec8: a secondary arm ends at these boundaries.
        if (old.half == 1 && event == (forward ? 4 : 2)) {
            remove_tracker(index);
            continue;
        }
        TrackerRef next{uint8_t((int(old.slot) + (forward ? 1 : -1)) & 127), old.half};
        const auto& destination = shoulders_[next.slot];
        if (destination.count == 1) next.half = 0;
        if (!(destination.motion_valid & (1u << next.half)) ||
            (forward && (old_parent.closed & (1u << old.half)))) next = old;
        t.slice = next;
        const auto& parent = shoulders_[next.slot];
        const uint32_t node = parent.location[next.half].first;
        if (node != old_node)
            t.position = forward ? 0 : int32_t((course_->nodes.at(node).length - 1) * 256);
        if (next == old) {
            // 00450e30 / 00450f02: the leading boundary retains its end
            // pointer; the trailing one clears both node references.
            if (kind == (forward ? 1 : 0))
                refs[old_node][kind] = old;
            else
                refs[old_node] = {};
            remove_tracker(index);
            continue;
        }
        refs[node][kind] = next;
        // Parent +1c grows from one sub-slice to two at a split window.
        // 00450e60 / 00450f2c clone the other physical arm, not a route.
        if (old_parent.count >= parent.count) continue;
        const TrackerRef other{next.slot, uint8_t(next.half ^ 1)};
        const int created = create_tracker(other, t.position, kind);
        if (created < 0) continue;
        const uint32_t other_node = shoulders_[other.slot].location[other.half].first;
        refs[other_node][kind] = other;
        // The entering leading boundary also seeds the missing trailing
        // reference. Its retained local coordinate is a real lower bound.
        if (kind == (forward ? 1 : 0) && event != (forward ? 3 : 1))
            refs[other_node][kind ^ 1] = other;
    }
}

void TrackRing::setup_trackers() {
    trackers_ = {};
    tracker_lists_ = {};
    tracker_refs_ = std::make_shared<TrackerRefs>();
    const TrackerRef start{uint8_t((uint32_t(back_) >> 8) & 127), 0};
    create_tracker(start, back_, 1);
    create_tracker(start, back_, 0);
    const uint32_t node = shoulders_[start.slot].location[0].first;
    (*tracker_refs_)[node] = {start, start};
    for (int i = 0; i < 91; ++i) step_trackers(1, 256);
    for (int i = 0; i < 14; ++i) step_trackers(0, 256);
}

std::array<std::optional<TrackRing::TrackerPoint>, 2> TrackRing::tracker_window(uint32_t node) const {
    std::array<std::optional<TrackerPoint>, 2> result;
    const auto found = tracker_refs_->find(node);
    if (found == tracker_refs_->end()) return result;
    for (size_t i = 0; i < 2; ++i) {
        if (!found->second[i]) continue;
        const auto ref = *found->second[i];
        const auto& parent = shoulders_[ref.slot];
        result[i] = TrackerPoint{parent.position, int32_t(parent.location[ref.half].second * 256)};
    }
    return result;
}

std::vector<int64_t> TrackRing::tracker_state() const {
    std::vector<int64_t> words;
    for (const auto& t : trackers_) {
        words.push_back(t.active);
        if (!t.active) continue;
        words.insert(words.end(), {t.kind, t.position, t.slice.slot, t.slice.half});
    }
    for (const auto& list : tracker_lists_) {
        words.push_back(int64_t(list.size()));
        for (const auto index : list) words.push_back(int64_t(index));
    }
    words.push_back(int64_t(tracker_refs_->size()));
    for (const auto& [node, refs] : *tracker_refs_) {
        words.push_back(node);
        for (const auto& ref : refs) {
            words.push_back(ref.has_value());
            if (!ref) continue;
            const auto& parent = shoulders_[ref->slot];
            words.insert(words.end(), {ref->slot, ref->half, parent.position,
                                      int64_t(parent.location[ref->half].second) * 256});
        }
    }
    return words;
}

} // namespace rr
