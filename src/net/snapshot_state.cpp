#include "net/snapshot_state.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace rr {
namespace {
constexpr size_t max_raw=2*1024*1024;
struct Archive {
    std::vector<uint8_t> bytes;
    std::span<const uint8_t> input;
    size_t at=0;
    bool reading=false;
    explicit Archive(std::span<const uint8_t> source):input(source),reading(true){}
    Archive()=default;
    template<class... T>void operator()(T&... values){(value(values),...);}
    uint8_t byte(){if(at>=input.size())throw std::runtime_error("Truncated snapshot");return input[at++];}
    template<class T>requires std::is_integral_v<T> void value(T& v){
        if constexpr(std::is_same_v<T,uint8_t>){if(reading)v=byte();else bytes.push_back(v);}
        else {
            uint64_t number=0;
            if(reading){
                int shift=0;
                for(;;){const auto b=byte();if(shift>=64||(shift==63&&(b&0x7e)))throw std::runtime_error("Snapshot integer overflow");number|=uint64_t(b&127)<<shift;if(!(b&128))break;shift+=7;}
                if constexpr(std::is_signed_v<T>){const int64_t decoded=int64_t(number>>1)^-int64_t(number&1);if(decoded<std::numeric_limits<T>::min()||decoded>std::numeric_limits<T>::max())throw std::runtime_error("Snapshot signed range");v=T(decoded);}
                else {if(number>uint64_t(std::numeric_limits<T>::max()))throw std::runtime_error("Snapshot unsigned range");v=T(number);}
            }else{
                if constexpr(std::is_signed_v<T>)number=(uint64_t(int64_t(v))<<1)^uint64_t(int64_t(v)>>63);
                else number=uint64_t(v);
                while(number>=128){bytes.push_back(uint8_t(number)|128);number>>=7;}bytes.push_back(uint8_t(number));
            }
        }
        if(bytes.size()>max_raw)throw std::runtime_error("Snapshot exceeds size limit");
    }
    template<class T,size_t N>void value(std::array<T,N>& values){for(auto& v:values)value(v);}
    template<class T,size_t N>void value(T(&values)[N]){for(auto& v:values)value(v);}
    template<class T>void value(std::optional<T>& v){bool have=v.has_value();value(have);if(reading){if(have)v.emplace();else v.reset();}if(have)value(*v);}
    template<class T>requires std::is_enum_v<T> void value(T& v){auto n=static_cast<std::underlying_type_t<T>>(v);value(n);if(reading)v=T(n);}
    template<class A,class B>void value(std::pair<A,B>& v){(*this)(v.first,v.second);}
    void value(PathSample& v){(*this)(v.bend,v.rise,v.left,v.right,v.left_q8,v.right_q8,v.shoulder_left,v.shoulder_right,v.height_left,v.height_right,v.side_flags,v.marking_flags,v.event_type,v.event_parameters,v.zone_end,v.event_start,v.event_end,v.elevation);}
    void value(RouteSample& v){(*this)(v.path,v.node,v.section,v.local,v.offset,v.dual,v.half,v.event,v.other,v.other_node,v.other_section,v.other_local,v.other_offset,v.dx_sep,v.x_sep);}
    void value(Bike& v){value(v.m);}
    void value(ActorAnimationState& v){(*this)(v.animation,v.frame,v.queued,v.loops,v.deadline,v.selected);}
    void value(SoundEventRecord& v){uint8_t code=uint8_t(v.code);(*this)(code,v.lateral,v.height,v.position,v.speed,v.argument,v.source);if(code>22)throw std::runtime_error("Invalid snapshot sound");v.code=SoundEvent(code);}
    void value(RiderStats& v){(*this)(v.strength,v.base,v.max,v.current);}
    void value(NitroConfig& v){(*this)(v.recharge,v.boost,v.max,v.charges,v.minimum,v.tail);}
    void value(Attack& v){(*this)(v.timer,v.weapon,v.right,v.backhand,v.fired);}
    void value(CrashActor& v){(*this)(v.motion,v.animation,v.route,v.sample,v.previous_sample,v.slice_half,v.previous_slice_half);}
    void value(Rider::HalfMirror& v){(*this)(v.position,v.lateral,v.height,v.velocity,v.speed,v.steer_rate,v.lat_vel,v.lat108,v.yaw,v.sample,v.route,v.slice_half);}
    void value(AiState& v){(*this)(v.target_x,v.target_position,v.target_speed,v.current_speed,v.current2,v.base_speed,v.replan_ticks,v.lane,v.bias,v.state,v.target_rider,v.target_step,v.nbr_ahead,v.nbr_behind,v.ov_ahead,v.ov_behind,v.rating,v.rating_tgt,v.ease,v.overtake_index,v.spd_a,v.spd_b,v.overtake_latched,v.overtake_type,v.overtake_position,v.reach,v.state_ticks,v.state_flag,v.kick_roll,v.backhand_roll,v.reaction,v.word_184);}
    void value(Rider& v){(*this)(v.bike,v.route,v.slot,v.rider_idx,v.sample,v.previous_sample,v.slice_half,v.previous_slice_half,v.wall_previous_sample,v.ai,v.cop,v.frozen,v.sched_last,v.sched_next,v.sched_interval,v.sched_live,v.bike_slice_present,v.half_slice_present,v.half_detached,v.parked,v.half_sched_last,v.half_sched_next,v.half_sched_interval,v.half_dt,v.live,v.virtual_local,v.virtual_section,v.half_lateral,v.half_position,v.half_speed,v.half_mirror,v.half_object,v.finish_distance,v.place,v.place_refresh,v.brain,v.finished,v.finish_controls_ended,v.result_code,v.finish_tick,v.crash_actor,v.crash_bike_animation,v.crash_active,v.half_frozen,v.crash_timer,v.colour,v.stamina,v.weapon,v.steal_until,v.hit_until,v.ride_animation,v.attack,v.contact_animation,v.nitro_charge,v.nitro_throttle,v.human,v.nitro_cfg,v.busted,v.nitro_count);}
    void value(TerrainPointState& v){(*this)(v.start,v.end,v.x,v.height,v.dx,v.dh);}
    void value(TerrainSlice& v){(*this)(v.type,v.side_heights,v.ring_adjusted,v.building_height,v.building_cut_roof,v.selectors,v.walls,v.points,v.shades);}
    void value(Race::ScheduleNode& v){(*this)(v.rider,v.type,v.position,v.sorted);}
    void value(TrafficObstacle& v){(*this)(v.position,v.lateral,v.velocity,v.lateral_velocity,v.type,v.road);}
    void value(TrafficTimers& v){(*this)(v.cruise,v.rate,v.oncoming_reload,v.other_reload,v.oncoming,v.behind,v.ahead,v.density);}
    void value(TrafficCar& v){(*this)(v.road,v.kind,v.vehicle,v.lane,v.position,v.lateral,v.speed,v.phase,v.jitter_a,v.jitter_b,v.jitter_c,v.serial,v.free_rank,v.active,v.pending_remove,v.last_tick,v.next_tick,v.interval,v.dt,v.motion,v.heading,v.signal,v.zone_start,v.zone_end,v.zone_lanes,v.collides,v.steer_error,v.lane_changed);}
    void value(IntersectionSignal& v){(*this)(v.road,v.active,v.free_rank,v.index,v.side,v.phase,v.accum,v.lane,v.position_q8,v.lateral_q8,v.controller,v.quiet);}
    void value(IntersectionController& v){(*this)(v.road,v.free_rank,v.active,v.durations,v.start_q8,v.end_q8,v.variant,v.lanes,v.position_q8,v.last_tick,v.next_tick,v.signals,v.pending_remove);}
    void value(Pedestrian& v){(*this)(v.active,v.pending_remove,v.free_order,v.body,v.animation,v.selector,v.type,v.state,v.side,v.heading,v.pace,v.trigger,v.order,v.lane,v.voice_tick,v.anchor,v.anchor_lateral,v.graphics_evicted,v.last_tick,v.next_tick,v.serial);}
    template<class T>void vector(std::vector<T>& values,size_t maximum){uint32_t count=uint32_t(values.size());value(count);if(count>maximum)throw std::runtime_error("Snapshot object limit");if(reading)values.resize(count);for(auto& v:values)value(v);}
};
// Zero-run coding is independent per snapshot: losing a packet never invalidates
// future snapshots, and decompression has a hard output bound.
std::vector<uint8_t> pack(const std::vector<uint8_t>& raw){
    std::vector<uint8_t> out;
    for(size_t at=0;at<raw.size();){
        size_t zeros=0;while(at+zeros<raw.size()&&!raw[at+zeros]&&zeros<128)++zeros;
        if(zeros>=3){out.push_back(uint8_t(128+zeros-1));at+=zeros;continue;}
        const size_t first=at++;
        while(at<raw.size()&&at-first<128){if(at+2<raw.size()&&!raw[at]&&!raw[at+1]&&!raw[at+2])break;++at;}
        out.push_back(uint8_t(at-first-1));out.insert(out.end(),raw.begin()+ptrdiff_t(first),raw.begin()+ptrdiff_t(at));
    }return out;
}
std::vector<uint8_t> unpack(std::span<const uint8_t> input){
    std::vector<uint8_t> out;
    for(size_t at=0;at<input.size();){const uint8_t tag=input[at++];const size_t count=(tag&127)+1;
        if(out.size()+count>max_raw)throw std::runtime_error("Snapshot decompression limit");
        if(tag&128)out.insert(out.end(),count,0);
        else{if(count>input.size()-at)throw std::runtime_error("Truncated snapshot literal");out.insert(out.end(),input.begin()+ptrdiff_t(at),input.begin()+ptrdiff_t(at+count));at+=count;}
    }return out;
}
}
struct SnapshotStateCodec {
    static void sounds(RaceState& copy,std::span<const SoundEventRecord> sounds){copy.race->sound_events_.assign(sounds.begin(),sounds.end());}
    static void terrain(Archive& a,TerrainIterator& value){
        a(value.position_,value.last_sample_,value.wall_record_,value.wall_values_,value.hill_record_,value.points_);
        if(a.reading && value.hill_record_>value.definition_->hills.size())throw std::runtime_error("Invalid terrain record");
    }
    static void ring(Archive& a,TrackRing& ring){
        a(ring.front_,ring.back_,ring.unsupported_,ring.branch_committed_,ring.chosen_half_,ring.slice_events_,ring.known_finish_);
        uint32_t count=uint32_t(ring.events_.size());a(count);
        if(count!=ring.events_.size())throw std::runtime_error("Ring event configuration mismatch");
        for(size_t i=0;i<count;++i){auto& event=ring.events_mut(i);a.vector(event.records,65536);if(event.records.size()!=ring.course_->sections[i].scenery_events.size())throw std::runtime_error("Invalid ring event count");}
        auto half=[&](auto& value){
            uint32_t node=value.node;a(node);
            if(a.reading){
                if(node){
                    if(!ring.course_->nodes.contains(node)||ring.course_->nodes.at(node).kind!=0)throw std::runtime_error("Invalid ring node");
                    auto forward=ring.forward_;ring.forward_=[] {return uint32_t(0);};
                    try{ring.enter(value,node);}catch(...){ring.forward_=forward;throw;}
                    ring.forward_=forward;if(value.terrain)value.terrain->random_=forward;
                }else value={};
            }
            a(value.stepped,value.position,value.offset,value.borr.position,value.borr.record,value.borr.start,value.borr.end,value.borr.current,value.borr.alternate,value.borr.left,value.borr.selector,value.borr.random,value.borr.count,value.borr.units,value.borr.period,value.borr.spacing,value.dzhr.node,value.dzhr.position,value.dzhr.index,value.family_position,value.family_index);
            bool have=value.terrain.has_value();a(have);if(have!=value.terrain.has_value())throw std::runtime_error("Ring terrain configuration mismatch");if(have)terrain(a,*value.terrain);
            if(a.reading&&value.dzhr.node>=ring.events_.size())throw std::runtime_error("Invalid ring actor section");
        };
        auto walker=[&](auto& v){a(v.position,v.active,v.window,v.event,v.front,v.countdown,v.dx_sep,v.x_sep,v.committed,v.collapse_pending,v.dual,v.stopped);if(v.active>1)throw std::runtime_error("Invalid active ring half");for(auto& h:v.halves)half(h);};
        walker(ring.front_walker_);walker(ring.back_walker_);
        uint32_t windows=uint32_t(ring.windows_->size());a(windows);if(windows>ring.course_->nodes.size())throw std::runtime_error("Invalid ring window count");
        if(a.reading)ring.windows_=std::make_shared<TrackRing::WindowCache>();
        auto window=[&](auto& v){a(v.active,v.dx_sep,v.x_sep,v.threshold);if(v.active>1)throw std::runtime_error("Invalid window half");for(auto& h:v.halves)half(h);};
        if(a.reading){for(uint32_t i=0;i<windows;++i){uint32_t key=0;a(key);if(!ring.course_->nodes.contains(key)||ring.windows_->contains(key))throw std::runtime_error("Invalid ring window key");window((*ring.windows_)[key]);}}
        else for(auto& [key,v]:*ring.windows_){auto k=key;a(k);window(v);}
        for(auto& v:ring.shoulders_)a(v.position,v.count,v.closed,v.countdown,v.motion_valid,v.edges,v.lanes,v.location,v.motion,v.roadway_masks,v.offsets,v.samples);
        auto ref=[&](auto& v){a(v.slot,v.half);if(v.slot>=128||v.half>1)throw std::runtime_error("Invalid tracker slice");};
        for(auto& v:ring.trackers_){a(v.active,v.kind,v.position);ref(v.slice);if(v.kind>1)throw std::runtime_error("Invalid tracker kind");}
        for(auto& list:ring.tracker_lists_){a.vector(list,4);for(auto index:list)if(index>=4)throw std::runtime_error("Invalid tracker index");}
        uint32_t refs=uint32_t(ring.tracker_refs_->size());a(refs);if(refs>ring.course_->nodes.size())throw std::runtime_error("Invalid tracker node count");
        if(a.reading)ring.tracker_refs_=std::make_shared<TrackRing::TrackerRefs>();
        auto refs_for=[&](auto& values){for(auto& entry:values){bool have=entry.has_value();a(have);if(a.reading){if(have)entry.emplace();else entry.reset();}if(have)ref(*entry);}};
        if(a.reading){for(uint32_t i=0;i<refs;++i){uint32_t key=0;a(key);if(!ring.course_->nodes.contains(key)||ring.tracker_refs_->contains(key))throw std::runtime_error("Invalid tracker node");refs_for((*ring.tracker_refs_)[key]);}}
        else for(auto& [key,values]:*ring.tracker_refs_){auto k=key;a(k);refs_for(values);}
        uint32_t closed=uint32_t(ring.closed_ends_->size());a(closed);if(closed>ring.course_->nodes.size())throw std::runtime_error("Invalid closed-end count");
        if(a.reading){ring.closed_ends_=std::make_shared<TrackRing::ClosedEnds>();for(uint32_t i=0;i<closed;++i){uint32_t key=0;a(key);if(!ring.course_->nodes.contains(key)||ring.closed_ends_->contains(key))throw std::runtime_error("Invalid closed-end node");a((*ring.closed_ends_)[key]);}}
        else for(auto& [key,value]:*ring.closed_ends_){auto k=key;a(k,value);}
        a(ring.family_read_,ring.family_write_,ring.resident_families_,ring.family_busy_,ring.family_enabled_,ring.family_synchronous_,ring.family_front_);
        auto request=[&](auto& v){a(v.family,v.slot,v.type);if(v.slot>=4)throw std::runtime_error("Invalid family slot");};
        for(auto& v:ring.family_queue_)request(v);
        bool have=ring.family_worker_.has_value();a(have);if(a.reading){if(have)ring.family_worker_.emplace();else ring.family_worker_.reset();}if(have)request(*ring.family_worker_);
        for(auto& history:ring.family_history_)a.vector(history,65536);
        if(a.reading)ring.random_=nullptr;
    }
    static void state(Archive& a,RaceState& copy){
        auto& r=*copy.race;
        uint32_t schema=2;a(schema);if(schema!=2)throw std::runtime_error("Unknown snapshot schema");
        a(r.countdown_started_,r.pace_band_tick_,r.actor_seat_,r.controls_,r.slot_ac_,r.box_draw_pending_,r.box_draw_camera_);
        a.vector(r.schedule_,32);
        uint32_t ground_count=uint32_t(r.ground_.size());a(ground_count);
        if(ground_count!=r.ground_.size())throw std::runtime_error("Ground configuration mismatch");
        for(size_t i=0;i<ground_count;++i){
            auto value=r.ground_[i];a(value.set);if(value.set)a(value.slice);
            if(a.reading)r.ground_.mut(i)=value;
        }
        a(r.ticks_,r.seed_,r.rng_draw_count_,r.roadside_order_,r.nitro_charges_,r.busted_,r.cop_removal_pending_,r.handicap_,r.handicap_tick_,r.spawn_order_,r.camera_heading_,r.camera_x_,r.camera_height_,r.camera_lift_,r.camera_back_,r.previous_camera_position_,r.seat_back_,r.seat_previous_,r.seat_heading_,r.seat_moved_);
        for(auto& n:r.roadside_nodes_)a(n.active,n.pending_remove,n.position,n.lateral,n.next_tick,n.last_tick,n.route,n.order);
        a.vector(r.riders_,16);a.vector(r.sound_events_,64);
        a.vector(r.car_scan_keys_,128);a.vector(r.ped_scan_keys_,120);
        auto scans=[&](auto& list){
            uint32_t count=uint32_t(list.size());a(count);if(count>512)throw std::runtime_error("Invalid scan count");if(a.reading)list.resize(count);
            for(auto& node:list){
                a(node.position,node.lateral,node.speed,node.type,node.order,node.key,node.rank,node.rider,node.pedestrian,node.car,node.roadside,node.slice_position,node.roadway_mask,node.slice_half);
                bool have=node.sample!=nullptr;a(have);uint8_t road=0;uint32_t index=0;
                if(!a.reading&&have){bool found=false;for(uint8_t k=0;k<2;++k){const auto& route=*r.routes_[k];const uintptr_t base=reinterpret_cast<uintptr_t>(route.data()),ptr=reinterpret_cast<uintptr_t>(node.sample);if(ptr>=base&&ptr-base<route.size()*sizeof(RouteSample)&&(ptr-base)%sizeof(RouteSample)==0){road=k;index=uint32_t((ptr-base)/sizeof(RouteSample));found=true;break;}}if(!found)throw std::runtime_error("Unknown scan slice");}
                if(have){a(road,index);if(road>1||index>=r.routes_[road]->size())throw std::runtime_error("Invalid scan slice");if(a.reading)node.sample=&(*r.routes_[road])[index];}else if(a.reading)node.sample=nullptr;
            }
        };
        scans(r.scan_);scans(r.scan_previous_);
        if(r.riders_.size()<size_t(r.humans()))throw std::runtime_error("Missing snapshot riders");
        for(const auto& rider:r.riders_){
            if(rider.route<0||rider.route>1||rider.sample>=r.routes_[rider.route]->size()||rider.previous_sample>=r.routes_[rider.route]->size()||rider.weapon<0||rider.weapon>3||rider.attack.weapon<0||rider.attack.weapon>3||rider.human>=r.humans()||rider.brain.state<0||rider.brain.state>9)throw std::runtime_error("Invalid snapshot rider");
        }
        bool have=copy.traffic.has_value();a(have);if(have!=copy.traffic.has_value())throw std::runtime_error("Traffic configuration mismatch");
        if(have){auto& t=*copy.traffic;a(t.tick_,t.scheduled_tick_,t.free_rank_,t.controller_free_rank_,t.signal_free_rank_,t.timers_,t.serial_,t.filled_,t.built_end_,t.zone_events_);a.vector(t.cars_,128);a.vector(t.controllers_,80);a.vector(t.signals_,320);a.vector(t.obstacles_,512);
            a(t.primary_front_,t.primary_built_end_,t.primary_filled_,t.building_seat_,t.configured_density_);
            uint32_t seat_count=uint32_t(t.seats_.size());a(seat_count);if(seat_count>7)throw std::runtime_error("Invalid traffic seat count");
            if(a.reading)t.seats_.resize(seat_count);
            for(auto& seat:t.seats_)a(seat.timers,seat.built_end,seat.filled);
            uint32_t window_count=uint32_t(t.windows_.size());a(window_count);if(window_count>8)throw std::runtime_error("Invalid traffic window count");
            if(a.reading)t.windows_.resize(window_count);
            for(auto& window:t.windows_){a(window.camera_q8,window.front_q8,window.distance,window.me_q8,window.velocity,window.ahead,window.behind,window.road_id);
                if(window.road_id<0||window.road_id>1)throw std::runtime_error("Invalid traffic window road");
                if(a.reading)window.road=r.routes_[window.road_id].get();}

            for(const auto& controller:t.controllers_){
                if(controller.road<0||controller.road>1)throw std::runtime_error("Invalid controller road");
                for(int signal:controller.signals)if(signal < -1 || (signal>=0&&size_t(signal)>=t.signals_.size()))throw std::runtime_error("Invalid controller signal");
            }
            for(const auto& signal:t.signals_)if(signal.road<0||signal.road>1||signal.controller < -1 || (signal.controller>=0&&size_t(signal.controller)>=t.controllers_.size())||signal.phase<0||signal.phase>2)throw std::runtime_error("Invalid intersection signal");
            for(const auto& c:t.cars_)if(c.road<0||c.road>1||(c.active&&(c.vehicle>=t.library_->vehicles().size()||c.kind>3)))throw std::runtime_error("Invalid snapshot car");
        }
        have=copy.pedestrians.has_value();a(have);if(have!=copy.pedestrians.has_value())throw std::runtime_error("Pedestrian configuration mismatch");
        if(have){auto& p=*copy.pedestrians;a(p.spawned_,p.serial_,p.free_order_,p.filled,p.built_back,p.built_front);a.vector(p.pool_,120);
            uint32_t consumed=uint32_t(p.consumed_.size());a(consumed);if(consumed>65536)throw std::runtime_error("Invalid consumed actor count");
            if(a.reading){p.consumed_.clear();for(uint32_t i=0;i<consumed;++i){uint32_t section=0,local=0;size_t index=0;a(section,local,index);p.consumed_.emplace(section,local,index);}}
            else for(const auto& item:p.consumed_){auto [section,local,index]=item;a(section,local,index);}
            if(a.reading)for(auto& pedestrian:p.pool_){auto& body=pedestrian.body;if(body.route<0||body.route>1||body.sample>=r.routes_[body.route]->size())throw std::runtime_error("Invalid pedestrian route");pedestrian.look=p.look(r.route_of(body)[body.sample],pedestrian.selector);}
        }
        bool have_ring=copy.ring.has_value();a(have_ring);if(have_ring!=copy.ring.has_value())throw std::runtime_error("Ring configuration mismatch");
        if(have_ring)ring(a,*copy.ring);
        uint32_t rings=uint32_t(copy.seat_rings.size());a(rings);if(rings!=copy.seat_rings.size())throw std::runtime_error("Seat ring configuration mismatch");
        for(auto& entry:copy.seat_rings){bool have=entry.has_value();a(have);if(have!=entry.has_value())throw std::runtime_error("Seat ring mismatch");if(have)ring(a,*entry);}
        if(a.reading){
            for(const auto& node:r.schedule_)if(node.rider<0||size_t(node.rider)>=r.riders_.size())throw std::runtime_error("Invalid rider schedule");
            r.static_car_=nullptr;
        }
    }
};
std::vector<uint8_t> encode_snapshot_state(const RaceInstance& instance,std::span<const SoundEventRecord> sounds){RaceState copy;save_state(instance,copy);SnapshotStateCodec::sounds(copy,sounds);Archive archive;SnapshotStateCodec::state(archive,copy);return pack(archive.bytes);}
bool apply_snapshot_state(RaceInstance& instance,std::span<const uint8_t> bytes){
    try{auto raw=unpack(bytes);Archive archive(raw);RaceState copy;save_state(instance,copy);SnapshotStateCodec::state(archive,copy);if(archive.at!=raw.size())return false;load_state(instance,copy);return true;}catch(const std::exception&){return false;}
}
}
