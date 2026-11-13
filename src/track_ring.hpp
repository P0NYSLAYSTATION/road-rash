#pragma once
#include "course.hpp"
#include "terrain.hpp"
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace rr {
// The random-consuming half of the original track ring: the two course
// walkers 0x0049ee30 (front) and 0x0049ea20 (back) that 0x004552e0 builds
// and 0x00454120 steps one slice at a time. Each walker carries the section
// feature iterators that draw from the shared CRT stream:
//   terrain  0x004510c0/0x004516e0 (LIHR init 0x00450fc0, steps 0x00451380;
//            NTMR/DWDR steps 0x00451220), via TerrainIterator;
//   BORR     0x00451b60/0x00451e20, slot walk 0x00451cb0 (run twice per
//            slice build by 0x00454240, drawing when spacing is random);
//   DZHR     0x00452060/0x00452230, actor spawns 0x004520d0 -> 0x004409e0.
// 0x00453e60 jitters and sorts every section's DZHR records (0x00451ee0,
// 0x00451fc0) in course-graph order before the walkers start.
// Split/merge traversal retains two section iterators and builds the
// active half before its opposite.
// One DZHR actor record fired by a slice build: ring_sample is the slice's
// route sample (ring position >> 8), position_q8 the record's position.
struct RingActor {
    int32_t ring_sample = 0;
    uint32_t position_q8 = 0, word0 = 0, word1 = 0, section = 0, local = 0;
    int32_t road_offset = 0; // the physical sub-slice +cc, including the opposite split half
};
// Value-only walker snapshots for optional original terrain diagnostics.
// Collidable roadside objects of one ring slice (0x00454240): the BORR
// slots of its second 0x00451cb0 walk (+0x60 count, +0x63 selector, +0x6c
// laterals) and up to two 0x004520d0 billboards (+0x3d, entries +0x40).
// Laterals are units relative to the road centre (slice +0xcc).
struct RingStatics {
    int physical_half = -1; // -1: standalone fixture identified by section/local
    bool dual = false;
    int32_t road_offset = 0, road_left = 0, road_right = 0;
    uint8_t borr_count = 0, borr_selector = 0x3f;
    std::array<int32_t, 8> borr{};
    uint8_t billboards = 0;
    struct Billboard {
        uint8_t type = 0, selector = 0;
        int32_t offset = 0;
    };
    std::array<Billboard, 2> billboard{};
};
struct TrackWalkerState {
    int32_t position = 0;
    uint32_t node = 0;
    int32_t borr_position = 0, borr_record = 0, borr_start = 0, borr_end = 0;
    int32_t dzhr_position = 0, dzhr_index = 0;
    int32_t offset = 0; // section +0x0c, world units
    std::optional<TerrainIteratorState> terrain;
};
struct TrackRingState {
    TrackWalkerState front, back;
    TrackWalkerState front_other, back_other;
    bool front_dual = false, back_dual = false;
    uint8_t front_active = 0, back_active = 0;
};
class TrackRing {
  public:
    using Random = std::function<uint32_t()>;
    // Called for each DZHR actor record 0x004520d0 fires, with its lateral
    // jitter draw (lateral = (centre + signed 12 bits) * 256 - 2s + jitter %
    // (4s + 1), s = (word1 & 0xff0000) >> 14). The sink makes the 0x004409e0
    // draws (pool allocation, police).
    using ActorSink = std::function<void(const RingActor&, uint32_t jitter)>;
    // Called at each slice build with the front walker's terrain slice
    // (0x00451910 into the ring slice, 0x00454240), keyed by section and
    // local sample. The race reads ground heights from these slices.
    using SliceSink =
        std::function<void(uint32_t section, uint32_t local, const PathSample&, const TerrainSlice&)>;
    TrackRing(const Course&, int level);
    void set_slice_sink(SliceSink sink) { slice_sink_ = std::move(sink); }
    // Called at each slice build with its collidable statics, keyed by the
    // slice's route sample.
    using StaticSink =
        std::function<void(int32_t ring_sample, uint32_t section, uint32_t local, const RingStatics&)>;
    void set_static_sink(StaticSink sink) { static_sink_ = std::move(sink); }
    // Copyable for race snapshots (plan 34 Task 3): course data is shared,
    // the per-node DZHR record lists are copy-on-write.
    TrackRing(const TrackRing&) = default;
    TrackRing& operator=(const TrackRing&) = default;
    // 0x004552e0 (random part): DZHR jitter, walker init, walk to camera -
    // 0x3000, then 128 slice builds from the front walker.
    void setup(int32_t camera_q8, const Random&, const ActorSink&);
    // 0x00454120 with one slice crossing: back walker step, slice build
    // (old front forward, new back backward), then front walker step.
    // Smaller moves only advance the ring cursors.
    void advance(int32_t delta_q8, const Random&, const ActorSink&);
    int32_t front_q8() const { return front_; }
    int32_t back_q8() const { return back_; }
    //0044b0e0 services one queued MSRR request before each scheduler pass.
    //Completed work becomes visible at the next pass, after the preceding draw.
    void service_family_queue();
    uint16_t resident_family(uint8_t slot) const { return family_busy_[slot] ? 0 : resident_families_[slot]; }
    const std::vector<uint16_t>& family_history(uint8_t slot) const { return family_history_[slot]; }
    int32_t known_finish_q8() const { return known_finish_; }
    uint8_t slice_event(int32_t position_q8) const {
        return slice_events_[size_t((uint32_t(position_q8) >> 8) & 127)];
    }
    bool unsupported() const { return unsupported_; }
    std::optional<std::array<int32_t, 2>> slice_shoulders(int32_t position, uint8_t half) const {
        const auto& s = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (!s.count) return std::nullopt;
        return s.edges[s.count == 1 ? 0 : half];
    }
    std::optional<std::array<int32_t, 2>> slice_lanes(int32_t position, uint8_t half) const {
        const auto& s = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (!s.count) return std::nullopt;
        return s.lanes[s.count == 1 ? 0 : half]; // right, left Q8 lane words
    }
    bool forward_closed(int32_t position, uint8_t half) const {
        return (shoulders_[size_t((uint32_t(position) >> 8) & 127)].closed & (1u << half)) != 0;
    }
    int32_t slice_countdown(int32_t position) const {
        return shoulders_[size_t((uint32_t(position) >> 8) & 127)].countdown;
    }
    std::optional<std::pair<uint32_t, uint32_t>> slice_location(int32_t position, uint8_t half) const {
        const auto& s = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (!s.count) return std::nullopt;
        return s.location[s.count == 1 ? 0 : half]; // graph node and section-local sample
    }
    std::optional<uint32_t> closed_upper(uint32_t node, int32_t tracker_sample) const {
        const auto found = closed_ends_->find(node);
        if (found == closed_ends_->end() || tracker_sample <= found->second.first) return std::nullopt;
        return found->second.second;
    }
    std::optional<std::pair<int32_t, int32_t>> retained_motion(int32_t position, uint8_t physical_half) const {
        const auto& s = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (!(s.motion_valid & (1u << physical_half))) return std::nullopt;
        return s.motion[physical_half]; // unused half1 survives a common-half0 rebuild
    }
    std::optional<uint8_t> retained_roadway_mask(int32_t position, uint8_t physical_half) const {
        const auto& s = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (!(s.motion_valid & (1u << physical_half))) return std::nullopt;
        return s.roadway_masks[physical_half];
    }
    std::optional<int32_t> retained_offset(int32_t position, uint8_t physical_half) const {
        const auto& s = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (!(s.motion_valid & (1u << physical_half))) return std::nullopt;
        return s.offsets[physical_half];
    }
    std::pair<int32_t, uint8_t> forward_slice(int32_t position, uint8_t half) const {
        const auto& from = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (from.closed & (1u << half)) return {position, half};
        const int32_t next = position + 256;
        const auto& to = shoulders_[size_t((uint32_t(next) >> 8) & 127)];
        return {next, to.count == 1 ? uint8_t(0) : half};
    }
    std::optional<RouteSample> retained_sample(int32_t position, uint8_t half) const {
        const auto& s = shoulders_[size_t((uint32_t(position) >> 8) & 127)];
        if (!(s.motion_valid & (1u << half))) return std::nullopt;
        auto sample = s.samples[half];
        sample.path.shoulder_left = sample.offset - sample.path.left - s.edges[half][0];
        sample.path.shoulder_right = s.edges[half][1] - sample.offset - sample.path.right;
        return sample;
    }
    void commit_branch(uint8_t half); // 0x004540e0, shared front/back choice
    TrackRingState diagnostic_state() const;
    struct TrackerPoint { int32_t position = 0, local = 0; };
    std::array<std::optional<TrackerPoint>, 2> tracker_window(uint32_t node) const;
    std::vector<int64_t> tracker_state() const;
    // Terrain slice exported by a walker at its current sample.
    TerrainSlice front_terrain(int32_t left_edge, int32_t right_edge) const;

  private:
    struct Borr {
        const std::vector<uint32_t>* words = nullptr;
        int32_t position = 0;       // [2] Q8
        int32_t record = 0;         // [3]
        int32_t start = 0, end = 0; // [9], [10] samples
        int32_t current = 0;        // [11] sample
        uint8_t alternate = 0, left = 0, selector = 0x3f, random = 0, count = 0, units = 0, period = 0;
        int32_t spacing = 0;                                                  // [8]
        void load(int32_t index);                                             // 0x00451a90
        void init(const std::vector<uint32_t>*, int32_t samples, bool start); // 0x00451b60
        void step(int32_t delta);                                             // 0x00451e20
        void slots(const PathSample&, const Random&, RingStatics&) const;     // 0x00451cb0
    };
    struct Dzhr {
        size_t node = 0;      // index into events_ (per course node)
        int32_t position = 0; // [1] Q8
        int32_t index = 0;    // [2]
    };
    struct SectionWalker {
        bool stepped = false;
        uint32_t node = 0;
        int32_t position = 0; // Q8 within the node's section
        int32_t offset = 0;   // section +0x0c, road lateral centre
        std::optional<TerrainIterator> terrain;
        Borr borr;
        Dzhr dzhr;
        int32_t family_position = 0, family_index = 0;
    };
    struct Walker {
        std::array<SectionWalker, 2> halves;
        int32_t position = 0; // traversal +0: route Q8, independent of section position
        uint8_t active = 0;   // traversal +4: half stepped and built first
        uint32_t window = 0;
        uint8_t event = 0;
        bool front = false;
        int32_t countdown = 0, dx_sep = 0, x_sep = 0;
        bool committed = false, collapse_pending = false;
        bool dual = false;
        bool stopped = false;
    };
    // 0x00453b60 retains the fully advanced traversal at window+0x1c;
    // late entry into a split/merge clones its section iterator state.
    struct Window {
        std::array<SectionWalker, 2> halves;
        uint8_t active = 0;
        int32_t dx_sep = 0, x_sep = 0, threshold = 80;
    };
    struct Events {
        uint32_t section = 0;
        std::vector<std::array<uint32_t, 2>> records;
    };
    void probe_window(uint32_t node, int32_t direction);                              // 0x00453b60 draws
    void jitter_events(uint32_t node, std::vector<uint32_t>& visited, const Random&); // 0x00453e60
    void enter(SectionWalker&, uint32_t node, bool start = true);                     // 0x00452cb0
    void step_families(SectionWalker&, int32_t delta);
    void request_family(uint16_t family, uint8_t slot, uint8_t type);
    void activate_family(uint16_t family, uint8_t slot);
    const PathSample& path_of(const SectionWalker&) const;
    int step_section(SectionWalker&, int32_t delta);                    // 0x00452e10 section result (0/1/2/3)
    void collapse(Walker&);                                             // 0x00452fd0
    void step(Walker&, int32_t delta);                                  // 0x00453030 -> 0x00452e10
    void build_half(SectionWalker&, int32_t ring_q8, const ActorSink&, bool discover_finish,
                    uint8_t physical_half, bool dual); // 0x00454240
    void build_slice(Walker&, int32_t ring_q8, const ActorSink&);       // 0x00454c80 -> 0x00454240
    const Course* course_;
    int level_;
    std::vector<std::shared_ptr<Events>> events_;          // one per node, DFS order (copy-on-write)
    Events& events_mut(size_t node);                       // clones a node's list shared with a snapshot
    std::vector<std::pair<uint32_t, size_t>> event_index_; // node offset -> events_
    const Random* random_ = nullptr;                       // stream for the current call
    Random forward_;                                       // TerrainIterator's source
    Walker front_walker_, back_walker_;
    // Built once during setup, then read-only during traversal. Snapshots
    // share it; copying std::map reallocates every cached node under MSVC.
    using WindowCache = std::map<uint32_t, Window>;
    std::shared_ptr<WindowCache> windows_ = std::make_shared<WindowCache>();
    int32_t front_ = 0, back_ = 0;
    bool branch_committed_ = false;
    uint8_t chosen_half_ = 0;
    std::array<uint8_t, 128> slice_events_{};
    struct Shoulders {
        int32_t position = 0;
        uint8_t count = 0;
        uint8_t closed = 0;
        int8_t countdown = 0;
        uint8_t motion_valid = 0;
        std::array<std::array<int32_t, 2>, 2> edges{};
        std::array<std::array<int32_t, 2>, 2> lanes{};
        std::array<std::pair<uint32_t, uint32_t>, 2> location{};
        std::array<std::pair<int32_t, int32_t>, 2> motion{};
        std::array<uint8_t, 2> roadway_masks{};
        std::array<int32_t, 2> offsets{};
        std::array<RouteSample, 2> samples{};
    };
    std::array<Shoulders, 128> shoulders_{};
    struct TrackerRef {
        uint8_t slot = 0, half = 0;
        bool operator==(const TrackerRef&) const = default;
    };
    struct Tracker {
        bool active = false;
        uint8_t kind = 0;
        int32_t position = 0;
        TrackerRef slice;
    };
    std::array<Tracker, 4> trackers_{}; // 00450d00: four 24-byte tracker objects
    std::array<std::vector<size_t>, 2> tracker_lists_;
    using TrackerRefs = std::map<uint32_t, std::array<std::optional<TrackerRef>, 2>>;
    std::shared_ptr<TrackerRefs> tracker_refs_ = std::make_shared<TrackerRefs>();
    TrackerRefs& mutable_tracker_refs() {
        if (tracker_refs_.use_count() != 1) tracker_refs_ = std::make_shared<TrackerRefs>(*tracker_refs_);
        return *tracker_refs_;
    }
    int create_tracker(TrackerRef, int32_t position, uint8_t kind);
    void remove_tracker(size_t index);
    void step_trackers(uint8_t kind, int32_t delta);
    void setup_trackers();
    using ClosedEnds = std::map<uint32_t, std::pair<int32_t, uint32_t>>;
    std::shared_ptr<ClosedEnds> closed_ends_ = std::make_shared<ClosedEnds>();
    int32_t known_finish_ = 0;
    bool unsupported_ = false;
    SliceSink slice_sink_;
    StaticSink static_sink_;
    struct FamilyRequest { uint16_t family; uint8_t slot, type; };
    std::array<FamilyRequest, 8> family_queue_{};
    uint8_t family_read_ = 0, family_write_ = 0;
    std::optional<FamilyRequest> family_worker_;
    std::array<uint16_t, 4> resident_families_{};
    std::array<bool, 4> family_busy_{};
    std::array<std::vector<uint16_t>, 4> family_history_;
    std::vector<std::vector<uint8_t>> family_types_;
    bool family_enabled_ = false, family_synchronous_ = true, family_front_ = false;
};
} // namespace rr
