#include "check.hpp"
#include "assets.hpp"
#include "scenery.hpp"
#include "animation.hpp"
#include <filesystem>
#include "net/snapshot_state.hpp"
#include "net/snapshot_session.hpp"
#include "net/protocol.hpp"
#include <array>
#include <iostream>
#include <memory>
static std::vector<rr::RouteSample> straight(size_t n, int bend = 0) {
    rr::RouteSample s{{bend, 0, 500, 500, 512, 512}, 0, 0, 0};
    s.path.shoulder_left = s.path.shoulder_right = 200;
    return std::vector<rr::RouteSample>(n, s);
}

static rr::BikeSpec test_spec() {
    rr::BikeSpec spec{};
    spec.d[1] = -800;
    spec.d[2] = 1024;
    spec.d[3] = 160;
    spec.d[4] = 160;
    spec.d[5] = 1;
    spec.d[6] = 196;
    auto sp = [&](int offset) -> int32_t& { return spec.d[7 + (offset - 0x1c4) / 4]; };
    const int up[6] = {3000, 5000, 7000, 8500, 21504, 16777215},
              down[6] = {-999, 2500, 4500, 6500, 8000, 17152};
    for (int g = 0; g < 6; ++g) {
        sp(0x1d0 + 16 * g) = 256 - 8 * g;
        sp(0x1d4 + 16 * g) = up[g];
        sp(0x1d8 + 16 * g) = down[g];
    }
    sp(0x248) = 20000;
    sp(0x2ec) = 10000;
    sp(0x250) = 38;
    sp(0x254) = 10;
    sp(0x258) = 60;
    sp(0x25c) = 64;
    sp(0x26c) = -60;
    sp(0x270) = 10;
    sp(0x274) = 10;
    sp(0x290) = 64;
    sp(0x294) = 320;
    sp(0x29c) = 11776;
    sp(0x2a0) = 50;
    sp(0x2a4) = 30;
    sp(0x2b0) = 120;
    sp(0x2b8) = 196;
    sp(0x2bc) = 2560;
    sp(0x2d0) = 346;
    sp(0x2d4) = 128;
    return spec;
}

static rr::PhysicsTables tables() {
    auto t = rr::PhysicsTables::synthetic();
    t.bike_object = {3,     4,    1, 0,    0,      0,    0,      0x12c, 0x100, 0x82, 8, 0x25, -0xa00,
                     0x100, 0x4a, 4, 0x18, 0x1e00, 0x40, 0x2300, 0xc0,  0x100, 0,    1, 1,    0};
    return t;
}

static rr::RaceInstance make_race(int humans=2) {
    rr::RaceInstance instance;
    auto spec=test_spec();
    std::vector<rr::Opponent> others;
    for(int seat=1;seat<humans;++seat){
        rr::Opponent rider{spec,90,90,90,seat%3,seat};
        rider.human=seat;
        others.push_back(rider);
    }
    instance.race=std::make_unique<rr::Race>(straight(4000),straight(4000),tables(),spec,90,others);
    instance.race->set_net(true);
    instance.race->set_snapshot_authority(true);
    return instance;
}
int main(int argc, char** argv) {
    rr::test::Checks check;
    auto original=make_race(8), restored=make_race(8);
    std::array<uint32_t,8> words{};
    for(int tick=0;tick<100;++tick){
        for(int seat=0;seat<8;++seat)words[size_t(seat)]=rr::control_accelerate|((tick+seat)%40<20?rr::control_left:rr::control_right);
        original.race->step(words);
    }
    const auto bytes=rr::encode_snapshot_state(original);
    check(rr::apply_snapshot_state(restored,bytes),"portable snapshot decodes into independently constructed race");
    check(rr::state_hash(original)==rr::state_hash(restored),"all rider simulation state round-trips");
    for(int tick=0;tick<60;++tick){
        original.race->step(words);restored.race->step(words);
        check(rr::state_hash(original)==rr::state_hash(restored),"restored state continues with identical physics and RNG");
    }
    auto stable=rr::state_hash(restored);
    for(size_t cut:std::array<size_t,5>{0,1,bytes.size()/4,bytes.size()/2,bytes.size()-1}){
        check(!rr::apply_snapshot_state(restored,std::span(bytes).first(cut)),"truncated snapshots rejected");
        check(rr::state_hash(restored)==stable,"failed decode does not mutate live state");
    }
    check(!rr::apply_snapshot_state(restored,std::vector<uint8_t>(3*1024*1024,255)),"decompression size bound enforced");
    using namespace rr::net;
    Start start;start.race_id=9;start.options.snapshots=true;start.countdown=4;
    start.humans={{0,1,0,"Host"},{1,2,0,"Peer"}};start.your_seat=0;
    auto host_world=make_race(),peer_world=make_race();
    std::vector<Message> host_messages,peer_messages;
    SnapshotSession host(host_world,start,[&](const Message& m){host_messages.push_back(m);},1000);
    auto peer_start=start;peer_start.your_seat=1;
    SnapshotSession peer(peer_world,peer_start,[&](const Message& m){peer_messages.push_back(m);},1000);
    for(uint64_t now=1000;now<1600;now+=17){
        peer.update(now,[](int){return rr::control_accelerate;},1000);
        for(const auto& m:peer_messages){auto input=std::get<SnapshotControls>(m);input.seat=1;host.receive(input,now);}
        peer_messages.clear();
        host.update(now,[](int){return rr::control_accelerate;},1000);
        for(const auto& m:host_messages)peer.receive(m,now);
        host_messages.clear();
    }
    check(!host.failed()&&!peer.failed(),"host/client prediction exchange remains live");
    check(peer.corrections>10,"clients reconcile repeated authoritative snapshots");
    check(host.current_tick()>30&&peer.current_tick()>30,"both run at the fixed simulation rate");
    SnapshotState forged;forged.race_id=9;forged.tick=999;forged.world=bytes;
    auto before=rr::state_hash(host_world);host.receive(forged,1600);
    check(rr::state_hash(host_world)==before&&host.malformed>0,"authority rejects client world state");
    {
        auto first=straight(4000),second=straight(4000);
        for(size_t i=0;i<first.size();++i){
            first[i].local=second[i].local=uint32_t(i);
            first[i].section=0;second[i].section=1;
            first[i].path.left=first[i].path.right=1600;
            second[i].path.left=second[i].path.right=1600;
        }
        rr::Opponent remote{test_spec(),90,90,90,1,1};remote.human=1;
        rr::Race race(first,second,tables(),test_spec(),90,{remote});
        rr::AnimationFrame tree;tree.boxes={{-14848,-43008,11264,-2048}};
        const std::vector<rr::SceneryObject> objects{{600,-980,5,8,0,false,false}},empty;
        race.set_static_frames([&](uint32_t,uint32_t,uint8_t,bool){return &tree;});
        race.set_authority_scenery([&](const rr::RouteSample& sample)->const auto& {
            return sample.section==1&&sample.local==600?objects:empty;
        });
        race.set_snapshot_authority(true);
        auto& rider=race.riders()[1];rider.route=1;rider.sample=rider.previous_sample=600;
        rider.bike.i32(0x1c)=600*256+128-rider.bike.i32(0x178)-5;
        for(int k=0;k<5;++k)rider.bike.i32(0x30+4*k)=rider.bike.i32(0x1c+4*k);
        rider.bike.i32(0x28)=-980*256-9000;rider.bike.i32(0x3c)=rider.bike.i32(0x28);
        rider.bike.i32(0xec)=4000;rider.bike.i32(0x2f0)=1000;
        check(race.static_slice_state(rider).boxes_ready,"remote branch collider prepared without rendering");
        const std::array<uint32_t,2> neutral{};race.step(neutral);
        check(rider.bike.i32(0x2f0)<1000,"remote player hits scenery far beyond host camera on another road");
        check(race.riders()[0].sample<100,"host remains outside the remote collision region");
    }
    if(argc>1) {
        rr::Assets assets(argv[1]);
        const auto exe=assets.read("ROADRASH.EXE");
        rr::NetRaceConfig net;net.authoritative_snapshots=true;net.seed=12345;
        net.humans={{0,1,-1,"Host"},{1,2,-1,"Peer"}};
        auto config=rr::net_race_config("NAPA",0,net);
        auto course=rr::load_race_assets(assets,exe,config);
        auto live=rr::build_race(course,exe,config), copy=rr::build_race(course,exe,config);
        for(int t=0;t<360;++t)live.race->countdown_step();
        const std::array<uint32_t,2> controls{rr::control_accelerate,rr::control_accelerate|rr::control_left};
        for(int t=0;t<180;++t)live.race->step(controls);
        auto packet=rr::encode_snapshot_state(live);
        check(rr::apply_snapshot_state(copy,packet),"real course snapshot decodes");
        check(rr::state_hash(live)==rr::state_hash(copy),"real course riders, cars, pedestrians and ring cursors round-trip");
        for(int t=0;t<120;++t){
            live.race->step(controls);copy.race->step(controls);
            check(rr::state_hash(live)==rr::state_hash(copy),"real restored race continues identically without reanchoring RNG");
        }
        std::cout<<"real snapshot bytes "<<packet.size()<<"\n";
    }
    std::cout<<"snapshot bytes "<<bytes.size()<<", peer corrections "<<peer.corrections<<"\n";
    return check.finish("snapshot");
}
