#include "test_assets.hpp"
#include "game_types.hpp"
#include "pedestrians.hpp"
#include "terrain_ground.hpp"
#include "view_state.hpp"
#include "world_renderer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>

namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
std::unique_ptr<rr::Game> make_game(const rr::Assets& assets,const std::vector<uint8_t>& exe,
                                   bool high,std::optional<size_t> start={}) {
    auto game=std::make_unique<rr::Game>();
    game->config.course="CANYN";game->config.level=4;game->config.high_resolution=high;
    game->config.ring_rng_state=1;game->config.bike_from_table=true;
    if(start) game->config.prepare=[start](rr::Race& race) {
        for(auto& r:race.riders())r.frozen=r.half_frozen=true;
        auto& me=race.riders()[0];
        me.sample=me.previous_sample=*start;me.route=0;me.slice_half=0;
        me.bike.i32(0x1c)=me.bike.i32(0x138)=int32_t(*start*256);
        for(int k=0;k<5;++k)me.bike.i32(0x30+4*k)=me.bike.i32(0x1c+4*k);
        me.half_mirror.position=me.bike.i32(0x1c);me.half_mirror.sample=*start;
        me.half_mirror.lateral=me.bike.i32(0x28);me.half_mirror.route=0;me.half_mirror.slice_half=0;
        race.step(0); // settle the camera before the ring is constructed
        me.frozen=me.half_frozen=false;
    };
    game->palette=rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
    game->depth=rr::read_depth_table(exe);game->load_course(assets,exe);game->restart();game->countdown=0;
    return game;
}
void empty_shadow(const rr::Assets& assets, const std::vector<uint8_t>& exe, bool high) {
    const auto sprites = rr::load_rider_sprites(assets, exe, high);
    const auto texture = rr::rider_texture(sprites, 0xe0, 22);
    require(texture.has_value(), "empty shadow must retain its template-backed texture");
    require(texture->bounds == std::array<int32_t, 4>{0, 0, 0, 0}, "fixture must use an empty shadow");
    rr::DrawNode node;
    node.texture = &*texture;
    // Canyn level 5 scene 180: a projected empty shadow still covers 2 x 2
    // pixels. Its final row samples the untouched template at row 255.
    node.view_points = {{{165 * 65536, 104 * 65536}, {166 * 65536, 104 * 65536},
                         {166 * 65536, 105 * 65536}, {165 * 65536, 105 * 65536}}};
    rr::Framebuffer fb(640, 480);
    const auto body = rr::rider_texture(sprites, 0, 3);
    require(body.has_value(), "shared-page test needs a valid preceding rider");
    auto preceding = node;
    preceding.texture = &*body;
    std::vector<rr::DrawNode> nodes{preceding, node};
    rr::RiderPageState page(sprites);
    page.prepare(nodes, rr::view_mode(true), 16);
    require(nodes[1].texture->mips[0].pixels[257] == body->mips[0].pixels[257],
            "empty shadows must retain previous rider copies, not reset the page");
    rr::render_nodes(fb, nodes, rr::view_mode(true), 16);
    require(fb.row(210)[330] == sprites.template_pixels[65281],
            "empty shadow must render the wrapped template sample");
}
void pedestrian_hit(const rr::Assets& assets,const std::vector<uint8_t>& exe,bool high,size_t slot,bool crashed=false,bool detached=false,
                    std::optional<size_t> start={},std::optional<uint8_t> selector={},int32_t speed=12000) {
    auto game=make_game(assets,exe,high,start);
    for(int t=0;t<60;++t)game->race().countdown_step();
    auto& race=game->race();auto& me=race.riders()[0];
    for(size_t i=1;i<race.riders().size();++i)race.riders()[i].parked=true;
    if (selector) {
        bool found=false;
        for(size_t i=0;i<game->instance.pedestrians->pool().size();++i) {
            const auto& candidate=game->instance.pedestrians->pool()[i];
            if(candidate.active && candidate.loaded() && candidate.selector==*selector) {
                slot=i;found=true;break;
            }
        }
        if(!found)for(const auto& p:game->instance.pedestrians->pool())
            std::cout<<"pool active="<<p.active<<" selector="<<unsigned(p.selector)<<" loaded="<<p.loaded()
                <<" sample="<<p.body.sample<<" position="<<p.body.bike.i32(0x1c)<<" camera="<<race.camera_position()<<'\n';
        if(!found) throw std::runtime_error("No loaded target pedestrian at start="+
            std::to_string(start.value_or(0))+" selector="+std::to_string(*selector));
    }
    auto& ped=game->instance.pedestrians->pool().at(slot);
    require(ped.active && ped.loaded(),"level5 contact must use an actual loaded Canyn pedestrian");
    // Keep the placed pedestrian, its look, ground, scheduler and reaction.
    // Approach its actual shoulder position at speed, rather than moving it into the road.
    // Start outside contact range, so the first movement step cannot teleport
    // past an unscheduled pedestrian before the ordered contact pass.
    me.bike.i32(0x1c)=ped.body.bike.i32(0x1c)-96;me.sample=me.previous_sample=size_t(me.bike.i32(0x1c))>>8;
    me.bike.i32(0x138)=me.bike.i32(0x1c);
    me.route=ped.body.route;me.slice_half=ped.body.slice_half;me.previous_slice_half=ped.body.previous_slice_half;
    me.bike.i32(0x28)=ped.body.bike.i32(0x28);me.bike.i32(0x24)=ped.body.bike.i32(0x24);
    me.bike.i32(0x20)=0;
    for(int k=0;k<5;++k)me.bike.i32(0x30+4*k)=me.bike.i32(0x1c+4*k);
    if (crashed) {
        race.crash_now(me);
        me.bike.i32(0x20)=me.bike.i32(0x34)=0;
        me.bike.i32(0xf4)=0;
        if (detached) {
            auto& body = me.crash_actor.motion;
            body.i32(0x20)=body.i32(0x34)=body.i32(0xf4)=0;
            body.i32(0xec)=body.i32(0x108)=12000;
            // Only the half reaches this pedestrian. Keep its bike away so
            // the bike callback cannot make a missing half callback pass.
            me.bike.i32(0x1c)-=20*256;
            me.bike.i32(0x138)=me.bike.i32(0x1c);
            me.sample=me.previous_sample=size_t(me.bike.i32(0x1c))>>8;
            for(int k=0;k<5;++k)me.bike.i32(0x30+4*k)=me.bike.i32(0x1c+4*k);
        }
    }
    me.bike.i32(0xec)=me.bike.i32(0x108)=detached?0:speed;
    bool hit=false,fall_frame=false,fall_drawn=false;
    for(int t=0;t<90;++t) {
        game->tick(rr::control_accelerate);
        hit|=ped.state==2;
        fall_frame|=ped.draw_animation && ped.draw_animation->selected && (*ped.draw_animation->selected)[0]==3;
        rr::capture_view(game->view_cur,race,game->traffic(),true);
        game->view_drawn=game->view_cur;game->view_ready=true;
        if(!crashed && t<20 && ped.draw_animation && ped.draw_animation->selected &&
            (*ped.draw_animation->selected)[0]==3) {
            const auto slices=rr::project_road(game->render_route(),game->blend_camera_position(),
                                              game->blend_camera(),game->depth);
            const auto& route=game->render_route();
            game->assets.scene->draw_nodes(route,slices,game->blend_camera(),game->depth,
                [&](const rr::RoadSlice& near,const rr::RoadSlice& far,int road,int32_t ceiling,
                    bool back,int32_t low,int32_t high) {
                    auto nodes=game->rider_nodes(near,far,road,ceiling,back,low,high);
                    const auto pos=uint32_t(ped.body.bike.i32(0x1c));
                    if(near.sample==(pos>>8) && back==((pos&255)>=128)) {
                        auto sample=route.at(near.sample);
                        if(near.count==2)sample=rr::route_half(sample,uint8_t(road));
                        const auto expected=rr::pedestrian_node(ped,near,far,road,sample,
                                                                game->blend_camera(),game->depth,ceiling);
                        if(expected)for(const auto& node:nodes)
                            if(node.texture==expected->texture && std::equal(node.view_points.begin(),node.view_points.end(),expected->view_points.begin(),
                                    [](const auto& a,const auto& b){return a.x==b.x && a.y==b.y;}))
                                fall_drawn=true;
                    }
                    return nodes;
                },320,game->instance.ring.get(),nullptr,nullptr,nullptr,{},race.horizon_angle());
        }
    }
    std::cout<<"level5 pedestrian slot="<<slot<<" selector="<<unsigned(ped.selector)<<" hit="<<hit
        <<" approach-speed="<<speed<<" crashed-bike="<<crashed<<" detached="<<detached<<" fall-frame="<<fall_frame<<" fall-drawn="<<fall_drawn<<" start="<<start.value_or(0)<<" state="<<ped.state<<'\n';
    rr::Pedestrian posed=ped;
    posed.body.bike.i32(0x1c)=6*256;posed.body.bike.i32(0x28)=0;
    posed.body.bike.i32(0x20)=posed.body.bike.i32(0x24)=0;
    rr::RoadSlice near{},far{};near.sample=6;far.sample=7;near.count=far.count=1;
    near.depth=6*256;far.depth=7*256;
    const auto down=rr::pedestrian_node(posed,near,far,0,race.route()[6],{0,150},game->depth,240*65536);
    rr::set_animation(posed.animation,posed.look->bank,1,uint32_t(race.ticks()));
    rr::advance_animation(posed.animation,posed.look->bank,uint32_t(race.ticks()));
    posed.draw_animation=posed.animation;
    const auto up=rr::pedestrian_node(posed,near,far,0,race.route()[6],{0,150},game->depth,240*65536);
    require(down && up && down->texture!=up->texture,"knockdown must render different artwork from standing");
    require(hit,"level5 rider must knock down the placed shoulder pedestrian");
    require(fall_frame,"knockdown must reach the pedestrian draw animation");
    if(!crashed)require(fall_drawn,"upright impact must emit falling artwork through the live scene renderer");
}
void upright_across_course(const rr::Assets& assets,const std::vector<uint8_t>& exe,bool high) {
    auto scan=make_game(assets,exe,high);
    std::set<const rr::PedestrianLook*> seen;
    const auto& route=scan->race().route();
    for(size_t i=8;i+80<route.size();++i) {
        const auto& sample=route[i];
        for(const auto& record:scan->assets.scene->dzhr_actors(sample.section,sample.local)) {
            if((record.word0&15)>=11)continue;
            const auto selector=uint8_t(record.word0>>8);
            const auto* look=scan->assets.scene->pedestrian_look(sample.section,sample.local,selector);
            if(!look || !seen.insert(look).second)continue;
            // Keep a relocated ring's back cursor nonnegative; initial-grid
            // scenarios above separately cover the course-start boundary.
            pedestrian_hit(assets,exe,high,0,false,false,std::max<size_t>(49,i-6),selector);
        }
    }
    std::cout<<"level5 upright representative families="<<seen.size()<<'\n';
}
void pedestrian_artwork(const rr::Assets& assets,const std::vector<uint8_t>& exe,bool high) {
    auto game=make_game(assets,exe,high);
    std::set<const rr::PedestrianLook*> tested;
    rr::RoadSlice near{},far{};near.sample=6;far.sample=7;near.count=far.count=1;
    near.depth=6*256;far.depth=7*256;
    for(const auto& sample:game->race().route())
        for(const auto& record:game->assets.scene->dzhr_actors(sample.section,sample.local)) {
            if((record.word0&15)>=11)continue;
            const auto* look=game->assets.scene->pedestrian_look(sample.section,sample.local,uint8_t(record.word0>>8));
            if(!look || !tested.insert(look).second)continue;
            rr::Pedestrian ped;ped.active=true;ped.look=look;
            game->instance.pedestrians->apply_template(ped.body.bike);
            ped.body.bike.i32(0x1c)=6*256;ped.body.bike.i32(0x28)=0;
            require(rr::set_animation(ped.animation,look->bank,1,0),"level5 pedestrian must have standing animation");
            rr::advance_animation(ped.animation,look->bank,0);ped.draw_animation=ped.animation;
            const auto up=rr::pedestrian_node(ped,near,far,0,game->race().route()[6],{0,150},game->depth,240*65536);
            require(rr::set_animation(ped.animation,look->bank,3,1),"level5 pedestrian must have knockdown animation");
            for(uint32_t t=1;t<121;++t)rr::advance_animation(ped.animation,look->bank,t);
            ped.draw_animation=ped.animation;
            const auto down=rr::pedestrian_node(ped,near,far,0,game->race().route()[6],{0,150},game->depth,240*65536);
            require(up && down && up->texture!=down->texture,
                    "every tested Canyn pedestrian family must render falling artwork instead of standing");
        }
    require(tested.size()>2,"level5 artwork test must cover multiple pedestrian families");
    std::cout<<"level5 pedestrian artwork families="<<tested.size()<<'\n';
}
void scheduled_riders(const rr::Assets& assets,const std::vector<uint8_t>& exe,bool high) {
    auto game=make_game(assets,exe,high);
    for(int t=0;t<60;++t)game->race().countdown_step();
    rr::ViewState raw,drawn,previous_raw,previous_drawn;
    size_t filled=0,reduced=0;int32_t max_raw=0,max_drawn=0;
    for(int t=0;t<800;++t) {
        game->tick(rr::control_accelerate);
        rr::capture_view(raw,game->race(),game->traffic());
        const auto before=game->race().rng_state();
        rr::capture_view(drawn,game->race(),game->traffic(),true);
        require(before==game->race().rng_state(),"presentation cannot consume simulation RNG");
        for(size_t i=1;i<game->race().riders().size();++i) {
            const auto& rider=game->race().riders()[i];
            if(!rider.ai || rider.crash_active || rider.parked || !rider.half_slice_present)continue;
            if(i>=previous_raw.actors.size() || i>=previous_drawn.actors.size())continue;
            const auto& a=raw.actors[i];const auto& b=drawn.actors[i];
            if(a.id!=i+1 || b.id!=i+1 || previous_raw.actors[i].id!=i+1 || previous_drawn.actors[i].id!=i+1 || !previous_raw.actors[i].visible)continue;
            const int32_t d=int32_t(a.position_q8-previous_raw.actors[i].position_q8);
            const int32_t p=int32_t(b.position_q8-previous_drawn.actors[i].position_q8);
            if(d==0 && p>0)++filled;
            if(d>100 && p>0 && p<d)++reduced;
            max_raw=std::max(max_raw,d);max_drawn=std::max(max_drawn,p);
        }
        previous_raw=raw;previous_drawn=drawn;
    }
    std::cout<<"level5 scheduled riders filled="<<filled<<" reduced="<<reduced
        <<" max-raw="<<max_raw<<" max-drawn="<<max_drawn<<'\n';
    require(filled>50 && reduced>10,"distant riders must move between scheduled physics updates");
}
void crash_entry(const rr::Assets& assets,const std::vector<uint8_t>& exe,bool high) {
    auto game=make_game(assets,exe,high);
    auto& me=game->race().riders()[0];
    const auto before=me.ride_animation.selected;
    require(before.has_value(),"level5 crash test needs a drawn riding frame");
    game->race().crash_now(me);
    require(me.crash_active && me.crash_actor.animation.selected==before,
            "crash entry must preserve the selected frame before the next half think");
}
void cliff(const rr::Assets& assets,const std::vector<uint8_t>& exe,bool high,bool diagnose=false,bool continuity=true) {
    auto scan=make_game(assets,exe,high);size_t sample=0;int side=0;rr::WallProfile wall;
    for(size_t i=80;i+40<scan->race().route().size();++i) {
        const auto& at=scan->race().route()[i];const auto& terrain=scan->assets.scene->terrain(at.section,at.local);
        if(terrain.type!=1 || at.dual)continue;
        wall=rr::wall_profile(terrain,at.path,false);
        if(wall.height[0]<-100 || wall.height[1]<-100){sample=i;side=wall.height[0]<-100?0:1;break;}
    }
    require(sample!=0,"Canyn level5 must exercise an actual cliff");
    auto game=make_game(assets,exe,high,sample);auto& race=game->race();auto& me=race.riders()[0];
    // Cross the real course's outer cliff edge with a grounded bike.
    for(size_t i=1;i<race.riders().size();++i) {
        auto& other=race.riders()[i];other.parked=true;other.live=false;
        other.bike_slice_present=other.half_slice_present=false;
        other.bike.u8(0xc0)&=0xfc;
    }
    me.bike.i32(0x28)=(diagnose ? wall.base[size_t(side)]+(side?-5:5) :
        wall.top[size_t(side)]+(side?100:-100))*256;
    me.bike.i32(0x20)=me.bike.i32(0x24)=me.bike.i32(0x34)=me.bike.i32(0x38)=0;
    me.bike.i32(0x160)=100;me.bike.i32(0xec)=me.bike.i32(0x108)=5000;
    for(int k=0;k<5;++k)me.bike.i32(0x30+4*k)=me.bike.i32(0x1c+4*k);
    const auto before=me.ride_animation.selected;
    require(before.has_value(),"cliff scenario must start with a selected riding frame");
    bool entered=false,retained=false;size_t body_frames=0;
    int first_fall=-1;size_t visible_fall_frames=0;
    int64_t previous_draw_world=0,highest_draw_world=INT64_MIN,last_draw_world=0;
    for(int t=0;t<(diagnose?90:12);++t) {
        game->tick(rr::control_accelerate | (diagnose ? (side?rr::control_right:rr::control_left) : 0));
        if(me.crash_active) {
            entered=true;retained|=bool(me.crash_actor.animation.selected);
            body_frames+=me.crash_actor.animation.selected.has_value();
        }
        if(diagnose) {
            for(size_t i=1;i<race.riders().size();++i)race.riders()[i].parked=true;
            const auto before_rng=race.rng_state();
            rr::capture_view(game->view_cur,race,game->traffic(),continuity);
            require(before_rng==race.rng_state(),"cliff presentation must not consume gameplay RNG");
            game->view_drawn=game->view_cur;game->view_ready=true;
            const auto& pose=game->view_cur.actors.front();
            const int64_t draw_world=int64_t(pose.height_q8)*256+pose.vertical_q8;
            if(me.crash_active && me.bike.i32(0x2f0)==0 && pose.height_q8<0 && first_fall<0) {
                first_fall=t;
                require(draw_world>=previous_draw_world-8*256,"cliff entry must not teleport the bike to sea level");
            }
            if(first_fall>=0)highest_draw_world=std::max(highest_draw_world,draw_world);
            previous_draw_world=last_draw_world=draw_world;
            const auto slices=rr::project_road(game->render_route(),game->blend_camera_position(),game->blend_camera(),game->depth);
            size_t sprites=0,onscreen=0;int32_t miny=INT32_MAX,maxy=INT32_MIN;
            game->assets.scene->draw_nodes(game->render_route(),slices,game->blend_camera(),game->depth,
                [&](const rr::RoadSlice& near,const rr::RoadSlice& far,int road,int32_t ceiling,bool back,int32_t low,int32_t high) {
                    const auto nodes=game->rider_nodes(near,far,road,ceiling,back,low,high);
                    for(const auto& node:nodes)if(node.texture && node.texture->rider_page) {
                        ++sprites;int32_t left=INT32_MAX,right=INT32_MIN,top=INT32_MAX,bottom=INT32_MIN;
                        for(const auto& p:node.view_points){left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
                        miny=std::min(miny,top);maxy=std::max(maxy,bottom);
                        if(left<320*65536 && right>0 && top<240*65536 && bottom>0)++onscreen;
                    }
                    return nodes;
                },320,game->instance.ring.get(),nullptr,nullptr,nullptr,{},race.horizon_angle());
            if(first_fall>=0 && t<first_fall+12) {
                require(onscreen>0,"player must remain drawable during the initial cliff fall");
                ++visible_fall_frames;
            }
            std::cout<<"cliff draw tick="<<race.ticks()<<" bike-selected="<<bool(me.crash_bike_animation.selected)
                <<" bike-code="<<int(me.crash_bike_animation.animation)<<" body-code="<<int(me.crash_actor.animation.animation)
                <<" sprites="<<sprites<<" onscreen="<<onscreen<<" y="<<miny/65536<<'/'<<maxy/65536
                <<" camera="<<game->blend_camera_position()<<','<<game->blend_camera().height
                <<" draw-world="<<draw_world<<" bike-x="<<me.bike.i32(0x28)<<" body-ground="<<me.crash_actor.motion.i32(0x24)
                <<" body-height="<<me.crash_actor.motion.i32(0x20)<<'\n';
        }
        std::cout<<"level5 cliff tick="<<race.ticks()<<" sample="<<sample<<" side="<<side
            <<" crash="<<me.crash_active<<" reason="<<me.bike.i32(0x2f8)<<" ground="<<me.bike.i32(0x24)
            <<" height="<<me.bike.i32(0x20)<<" selected="<<bool(me.crash_actor.animation.selected)<<'\n';
    }
    require(entered && retained && body_frames>1,"cliff entry must retain a drawable crash body");
    if(diagnose) {
        require(first_fall>=0 && visible_fall_frames==12,"level5 check must exercise visible cliff continuity");
        require(last_draw_world<highest_draw_world-70*256,"the bike must fall, not hang above the cliff");
        std::cout<<"level5 visible cliff fall frames="<<visible_fall_frames<<" entry="<<first_fall
            <<" highest="<<highest_draw_world<<" final="<<last_draw_world<<'\n';
    }
}
}
int main(int argc,char** argv) {
    try {
        rr::Assets assets(rr::test::assets_root());const auto exe=assets.read("ROADRASH.EXE");
        for(bool high:{false,true}) {
            std::cout<<"CANYN level5 high-res-bikers="<<high<<'\n';
            if(argc>1 && std::string(argv[1])=="--cliff-raw") {cliff(assets,exe,high,true,false);continue;}
            if(argc>1 && std::string(argv[1])=="--cliff-only") {cliff(assets,exe,high,true);continue;}
            if(argc>1 && std::string(argv[1])=="--upright-speed") {
                for(int32_t speed:{8000,16000,24000,32000})
                    for(size_t slot:{size_t(0),size_t(1)})
                        pedestrian_hit(assets,exe,high,slot,false,false,{},{},speed);
                continue;
            }
            if(argc>1 && std::string(argv[1])=="--upright-only") {
                upright_across_course(assets,exe,high);continue;
            }
            empty_shadow(assets,exe,high);pedestrian_hit(assets,exe,high,0);pedestrian_hit(assets,exe,high,1);pedestrian_hit(assets,exe,high,0,true);pedestrian_hit(assets,exe,high,0,true,true);
            pedestrian_artwork(assets,exe,high);scheduled_riders(assets,exe,high);crash_entry(assets,exe,high);cliff(assets,exe,high);
        }
        std::cout<<"level5 gameplay passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
