#include "net/snapshot_session.hpp"
#include "net/snapshot_state.hpp"
#include <algorithm>
namespace rr::net {
SnapshotSession::SnapshotSession(RaceInstance& instance,Start start,Send send,uint64_t now)
 :instance_(instance),start_(std::move(start)),send_(std::move(send)),next_step_ms_(double(now)+start_.start_tick*1000.0/60){
    outcome_codes_.fill(1);instance_.race->set_snapshot_authority(true);
    input_at_.fill(now);
}
void SnapshotSession::step(const std::array<uint16_t,8>& controls,bool authoritative){
    auto& race=*instance_.race;
    for(size_t i=0;i<start_.humans.size();++i)race.set_human_driven(int(i),(present_&(1u<<i))!=0);
    if(tick_<start_.countdown)race.countdown_step();
    else {std::array<uint32_t,8> words{};std::copy(controls.begin(),controls.end(),words.begin());race.step(std::span(words).first(start_.humans.size()));}
    ++tick_;
    auto sounds=race.take_sound_events();
    if(instance_.traffic){auto extra=instance_.traffic->take_sound_events();sounds.insert(sounds.end(),extra.begin(),extra.end());}
    if(authoritative){
        for(const auto& sound:sounds){if(sounds_.size()<256)sounds_.push_back(sound);if(snapshot_sounds_.size()<64)snapshot_sounds_.push_back(sound);}
        outcomes();
    }
}
void SnapshotSession::outcomes(){
    auto& race=*instance_.race;
    for(size_t i=0;i<start_.humans.size();++i){const auto outcome=race.human_outcome(int(i));if(outcome.code!=outcome_codes_[i]){outcome_codes_[i]=outcome.code;if(outcome_events_.size()<32)outcome_events_.push_back({tick_,int(i),outcome});}}
    if(host()&&!results_sent_&&race.all_humans_done()){
        publish();SnapshotResults result;result.race_id=start_.race_id;result.tick=tick_;
        for(size_t i=0;i<start_.humans.size();++i){const auto outcome=race.human_outcome(int(i));result.rows.push_back({start_.humans[i].seat,outcome.code==1?Outcome::left:Outcome(outcome.code),uint8_t(std::clamp(outcome.place,0,15))});}
        send_(result);results_sent_=true;
    }
}
void SnapshotSession::publish(){
    SnapshotState state;state.race_id=start_.race_id;state.tick=tick_;state.acknowledged=acknowledged_;state.controls=controls_;
    state.world=encode_snapshot_state(instance_,snapshot_sounds_);snapshot_sounds_.clear();send_(state);
}
bool SnapshotSession::receive(const Message& message,uint64_t now){
    if(const auto* value=std::get_if<SnapshotControls>(&message)){
        if(!host()||value->race_id!=start_.race_id||value->seat>=start_.humans.size()||value->seat==start_.authority_seat){++malformed;return true;}
        const size_t seat=value->seat;if(value->sequence<=received_sequence_[seat])return true;
        received_sequence_[seat]=value->sequence;controls_[seat]=value->controls;input_at_[seat]=now;return true;
    }
    if(const auto* value=std::get_if<SnapshotPresence>(&message)){
        if(value->race_id==start_.race_id)present_=value->connected;else ++malformed;return true;
    }
    if(const auto* value=std::get_if<SnapshotState>(&message)){
        if(host()||value->race_id!=start_.race_id){++malformed;return true;}
        if(value->tick<=received_tick_)return true;
        if(!apply_snapshot_state(instance_,value->world)||instance_.race->ticks()!=value->tick){++malformed;failed_=true;return true;}
        received_tick_=value->tick;tick_=value->tick;controls_=value->controls;acknowledged_=value->acknowledged;
        auto sound=instance_.race->take_sound_events();for(const auto& event:sound)if(sounds_.size()<256)sounds_.push_back(event);
        outcomes();
        const int local=start_.your_seat;
        if(local<int(start_.humans.size()))while(!pending_.empty()&&pending_.front().sequence<=acknowledged_[size_t(local)])pending_.pop_front();
        correction_depth=int(pending_.size());++corrections;
        // Re-run only controls the authority has not acknowledged. Other actors
        // are temporary prediction; only incoming host state is authoritative.
        for(const auto& input:pending_){auto predicted=controls_;predicted[size_t(local)]=input.controls;step(predicted,false);}
        return true;
    }
    return false;
}
void SnapshotSession::update(uint64_t now,const Sampler& sample,int32_t stop){
    if(failed_||results_sent_)return;
    int steps=0;
    while(double(now)>=next_step_ms_&&tick_<stop&&steps++<120){
        next_step_ms_+=1000.0/60;
        const int local=start_.your_seat;
        if(local<int(start_.humans.size())){
            const uint16_t word=sample(tick_);controls_[size_t(local)]=word;
            if(host())received_sequence_[size_t(local)]=++sequence_;
            else {
                SnapshotControls input{start_.race_id,++sequence_,word,0xff};pending_.push_back(input);send_(input);
                if(pending_.size()>120){failed_=true;return;} // bounded to two seconds of lost authority
            }
        }
        if(host()){
            for(size_t i=0;i<start_.humans.size();++i)if(i!=start_.authority_seat&&now-input_at_[i]>500)controls_[i]=0;
            acknowledged_=received_sequence_;step(controls_,true);if((tick_&1)==0&&!results_sent_)publish();
        }else step(controls_,false);
    }
}
std::vector<SoundEventRecord> SnapshotSession::take_sounds(){std::vector<SoundEventRecord> out;out.swap(sounds_);return out;}
std::optional<SnapshotSession::OutcomeEvent> SnapshotSession::poll_outcome(){if(outcome_events_.empty())return {};auto event=outcome_events_.front();outcome_events_.pop_front();return event;}
}
