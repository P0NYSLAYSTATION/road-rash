#pragma once
#include "net/protocol.hpp"
#include "race_setup.hpp"
#include <deque>
namespace rr::net {
// The host alone runs authoritative gameplay. Peers send controls, predict
// immediately and reconcile to complete state snapshots, never sealed inputs.
class SnapshotSession {
public:
    using Send=std::function<void(const Message&)>;
    using Sampler=std::function<uint16_t(int32_t)>;
    struct OutcomeEvent{int32_t tick;int slot;Race::HumanOutcome outcome;};
    SnapshotSession(RaceInstance&,Start,Send,uint64_t now_ms);
    bool receive(const Message&,uint64_t now_ms);
    void update(uint64_t now_ms,const Sampler&,int32_t stop_before);
    int32_t current_tick()const{return tick_;}
    bool host()const{return start_.your_seat==start_.authority_seat;}
    bool failed()const{return failed_;}
    uint64_t malformed=0,corrections=0;
    int correction_depth=0;
    std::vector<SoundEventRecord> take_sounds();
    std::optional<OutcomeEvent> poll_outcome();
private:
    void step(const std::array<uint16_t,8>&,bool authoritative);
    void outcomes();
    void publish();
    RaceInstance& instance_;
    Start start_;
    Send send_;
    int32_t tick_=0,received_tick_=-1;
    double next_step_ms_=0;
    uint32_t sequence_=0;
    std::array<uint16_t,8> controls_{};
    std::array<uint32_t,8> acknowledged_{},received_sequence_{};
    std::array<uint64_t,8> input_at_{};
    std::array<int,8> outcome_codes_{};
    uint8_t present_=0xff;
    std::deque<SnapshotControls> pending_;
    std::deque<OutcomeEvent> outcome_events_;
    std::vector<SoundEventRecord> sounds_,snapshot_sounds_;
    bool failed_=false,results_sent_=false;
};
}
