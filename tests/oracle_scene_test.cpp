#include "test_assets.hpp"
#include <filesystem>
// Natural-frame display-list check: the original's own pending display nodes for a captured race
// frame (oracle dump_scene) are converted to native DrawNodes, resources identified by their mip-0
// hash, and rendered over the original frame. Every pixel must equal the original final pixel.
//
// --native-actors mode (plan 11a Task 3 Step 2): per-node original comparison of player/opponent
// actor nodes against oracle dispatch records (actor.sel.*/actor.bb.* TSV keys, scene v4+). The
// display list streams through a 250-node buffer (0x00450470 flushes via 0x00450420 mid-projection),
// so the dump holds the post-flush tail; each surviving bb call maps to dump index nafter-1.
// Per bb call the test checks, each against its own ground truth:
//   descriptor import: sel 20B == native playback bank bytes (render/duration/control);
//   orientation: native actor_angles view vs sel view (+0xd0);
//   inputs: bb lateral == (sel f50 + sign-rounding) >> 8;
//   selection+projection: select_actor_view + actor_node with bb lateral/ground/pos/clip/occ_row
//     vs dump node frame/bank/4 view points/flip;
//   palette: traced colour rule (cop 0xde / player row byte / grid slot; shadow 0xdf iff cop)
//     vs dump node colour (+0x38).
#include "actor_nodes.hpp"
#include "course_scene.hpp"
#include "track_ring.hpp"
#include "terrain_ground.hpp"
#include "course.hpp"
#include "golden.hpp"
#include "rider_sprites.hpp"
#include "road.hpp"
#include "scene_dump.hpp"
#include "texture_library.hpp"
#include "view.hpp"
#include "world_renderer.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>
#include <memory>
#include <set>
#include <tuple>

namespace {
const char* const course_files[5] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"}; // 0x0046df58
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int32_t blob_i32(const std::vector<uint8_t>& b, size_t p) {
    return int32_t(uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
                   uint32_t(b[p + 3]) << 24);
}
int64_t key(const std::map<std::string, int64_t>& f, const std::string& k, int64_t dflt = 0) {
    const auto it = f.find(k);
    return it == f.end() ? dflt : it->second;
}

// Expected palette colour for a scheduler bike object (traced 0x00446190 /
// 0x004460c0 via 0x00446380): the cop bike is 0xde; the player bike
// (DAT_004c7f70) takes the rider-table row byte (rider half +0x1e0 points at
// the 0x00463c78 row, +5); other bikes take their 1-based grid slot in
// DAT_004c7f74, or 0 when the slot equals the player row byte.
int32_t actor_colour(uint32_t bike_addr, const rr::scene::Memory& mem, uint8_t player_row_byte) {
    const uint32_t cop = mem.u32(0x004642dc);
    if (cop != 0 && bike_addr == cop) return 0xde;
    if (bike_addr == mem.u32(0x004c7f70)) return int32_t(player_row_byte);
    for (int slot = 1; slot <= 15; ++slot) {
        uint32_t entry = 0x004c7f74 + uint32_t(slot - 1) * 4;
        if (!mem.has(entry)) continue;
        if (mem.u32(entry) == bike_addr) return slot == player_row_byte ? 0 : slot;
    }
    return -1; // bike not on the grid: caller reports it
}

// Rider-table row byte (+5) for the player: the player half (+0x2fc of the
// player bike DAT_004642d8) carries the 0x00463c78 row pointer at +0x1e0.
uint8_t player_row_byte(const rr::scene::Dump& dump, const std::vector<uint8_t>& exe) {
    const uint32_t player = dump.memory.u32(0x004642d8);
    for (const auto& r : dump.riders) {
        if (r.address != player || r.half.empty()) continue;
        const uint32_t row = uint32_t(blob_i32(r.half, 0x1e0));
        if (dump.memory.has(row + 5)) return dump.memory.u8(row + 5);
        const size_t raw = 0x61600 + (row - 0x00463000) + 5; // .data file offset
        if (row >= 0x00463000 && raw < exe.size()) return exe[raw];
    }
    return 0;
}

int native_actors(const std::string& tsv, const rr::Assets& assets) {
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    if (!rr::golden::original_executable(golden.exe)) {
        std::cerr << "Oracle capture is from a different executable\n";
        return 1;
    }
    const int course = int(golden.frames.at(0).at("course"));
    const auto exe = assets.read("ROADRASH.EXE");
    const auto depths = rr::read_depth_table(exe);
    const auto course_data =
        rr::read_course(assets.read(std::string("DATA/COURSES/") + course_files[course] + ".CRS"));
    const auto routes = {rr::make_route(course_data, false), rr::make_route(course_data, true)};
    rr::ActorNodes actors(assets, exe, true);
    const auto& bank = actors.library().playback[0];
    int failures = 0, frames = 0, nodes = 0;
    for (size_t f = 0; f < golden.frames.size(); ++f) {
        const auto& frame = golden.frames[f];
        if (!frame.count("scene.file")) continue;
        const int64_t scene_file = frame.at("scene.file");
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(scene_file) + ".raw");
        if (dump.version < 4 || dump.riders.empty() || !frame.count("actor.sel.count")) {
            std::cerr << "FAIL frame " << scene_file
                      << ": native actor comparison requires v4 dump and actor hooks\n";
            ++failures;
            continue;
        }
        const uint8_t prow = player_row_byte(dump, exe);
        const uint32_t cam_pos = uint32_t(frame.at("camera.position"));
        const int32_t cam_x = int32_t(frame.at("camera.x")), cam_h = int32_t(frame.at("camera.height"));
        bool frame_ok = false;
        std::string frame_error;
        int frame_checked = 0;
        for (const auto& route : routes) {
            if ((cam_pos >> 8) >= route.size()) continue;
            rr::Camera camera{cam_x - route[cam_pos >> 8].offset, cam_h};
            const auto slices = rr::project_road(route, cam_pos, camera, depths);
            std::map<uint32_t, size_t> by_sample;
            for (size_t i = 0; i < slices.size(); ++i) by_sample[slices[i].sample] = i;
            // Half objects by scheduler address (halves carry the animation).
            std::map<uint32_t, const rr::scene::RiderObjects*> halves;
            for (const auto& r : dump.riders) halves[r.address] = &r;
            // Bike address per half (via bike +0x2fc).
            std::map<uint32_t, uint32_t> bike_of_half;
            for (const auto& r : dump.riders) {
                if (r.bike.size() < 0x300 || blob_i32(r.bike, 0xc4) != 4) continue;
                bike_of_half[uint32_t(blob_i32(r.bike, 0x2fc))] = r.address;
            }
            const size_t nsel = size_t(key(frame, "actor.sel.count")),
                         nbb = size_t(key(frame, "actor.bb.count"));
            bool usable = true;
            std::string diff;
            int checked = 0;
            // Group sel records by half; bb records pair with sels in dispatch order.
            for (size_t b = 0; b < nbb && usable; ++b) {
                const int32_t pos = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".pos"));
                const int32_t clip = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".clip"));
                const int32_t lateral = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".lateral"));
                const int32_t ground = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".ground"));
                const bool full_list=key(frame,"scene.full_list",0)!=0;
                if(full_list && !key(frame,"actor.bb.absolute_indices",0))
                    throw std::runtime_error("Full-list actor comparison requires absolute billboard indices; recapture with current hooks");
                const auto bp="actor.bb."+std::to_string(b)+".";
                const int64_t nafter=key(frame,bp+(full_list?"absolute_after":"nafter"),-1);
                const int64_t nbefore=key(frame,bp+(full_list?"absolute_before":"nbefore"),-1);
                if(full_list && nafter==nbefore) continue; //original call emitted no node
                if (nafter < 1 || nafter > int64_t(dump.nodes.size())) {
                    if(full_list) throw std::runtime_error("Absolute billboard index outside complete display list");
                    continue; //legacy tail capture lost this call in a flush
                }
                //The starting grid can place two riders at the same longitudinal
                //position. New captures retain the actual selected actor address.
                const rr::scene::RiderObjects* half = nullptr;
                const uint32_t selected=uint32_t(key(frame,bp+"actor",0));
                if(selected) {
                    const auto found=halves.find(selected);
                    if(found!=halves.end()) half=found->second;
                } else {
                    if(full_list) throw std::runtime_error("Full-list actor capture lacks selected actor identity");
                    for (const auto& r : dump.riders) {
                        if (r.bike.size() < 0xc8) continue;
                        if (blob_i32(r.bike, 0x1c) == pos && blob_i32(r.bike, 0xc4) == 8) {
                            half = &r;
                            break;
                        }
                    }
                }
                if (!half) {
                    usable = false;
                    diff = "no half for bb pos " + std::to_string(pos);
                    break;
                }
                // The half's sel records (shadow then body): find by bike address.
                size_t sel = nsel;
                int seen = 0;
                for (size_t s = 0; s < nsel; ++s) {
                    if (uint32_t(key(frame, "actor.sel." + std::to_string(s) + ".bike")) != half->address)
                        continue;
                    if ((seen++ == 0) != (clip == 0)) continue;
                    sel = s;
                    break;
                }
                if (sel >= nsel) {
                    usable = false;
                    diff = "no sel for half pos " + std::to_string(pos);
                    break;
                }
                const std::string sp = "actor.sel." + std::to_string(sel) + ".";
                // (1) Descriptor import parity: sel 20B vs native bank bytes.
                // The dump's +0x80/+0x81 is post-draw and may lag dispatch by
                // a frame advance (0x0043ae90 runs for RRAN halves), so find
                // the bank entry whose bytes equal the sel record exactly
                // (unique match required); the anim still comes from the dump.
                const int8_t anim = half->bike[0x80];
                if (anim < 0 || size_t(anim) >= bank.size() || !bank[size_t(anim)]) {
                    usable = false;
                    diff = "half anim out of range pos " + std::to_string(pos);
                    break;
                }
                const int64_t d0 = key(frame, sp + "d0"), d1 = key(frame, sp + "d1");
                const int64_t d2 = key(frame, sp + "d2"), d3 = key(frame, sp + "d3");
                const auto& seq = bank[size_t(anim)];
                int found = -1, hits = 0;
                for (size_t di = 0; di < seq->size(); ++di) {
                    const uint8_t* cb = seq->at(di).render.data();
                    if (d0 == (int64_t(uint32_t(cb[0]) | uint32_t(cb[1]) << 8 | uint32_t(cb[2]) << 16 |
                                       uint32_t(cb[3]) << 24)) &&
                        d1 == (int64_t(uint32_t(cb[4]) | uint32_t(cb[5]) << 8 | uint32_t(cb[6]) << 16 |
                                       uint32_t(cb[7]) << 24)) &&
                        (d2 & 0xffff) == (int64_t(cb[8]) | (int64_t(cb[9]) << 8)) &&
                        (d3 & 0xff) == int64_t(seq->at(di).duration) &&
                        ((d3 >> 8) & 0xff) == int64_t(seq->at(di).control)) {
                        found = int(di);
                        ++hits;
                    }
                }
                if (hits != 1) {
                    usable = false;
                    diff = "descriptor import pos " + std::to_string(pos);
                    break;
                }
                const auto& descriptor = seq->at(size_t(found));
                // (2) Input consistency: bb lateral == (sel f50 + sign-rounding) >> 8.
                const int32_t f50 = int32_t(key(frame, sp + "f50"));
                const int32_t want_lat = (f50 + ((f50 >> 31) & 0xff)) >> 8;
                if (want_lat != lateral) {
                    usable = false;
                    diff = "lateral consistency pos " + std::to_string(pos);
                    break;
                }
                // (3) Orientation: native view angle vs sel view (+0xd0).
                auto sit = by_sample.find(uint32_t(pos) >> 8);
                if (sit == by_sample.end() || sit->second >= slices.size() - 1)
                    continue; // not in slice window
                const auto &near = slices[sit->second], far = slices[sit->second + 1];
                if (near.count == 2) continue; // split roadways: game path only
                const auto& sample = route.at(near.sample);
                rr::ActorOrientation orientation;
                orientation.depth = near.depth;
                orientation.position = pos;
                orientation.road_x = near.road[0].road_x + camera.x;
                orientation.lateral_q8 = blob_i32(half->bike, 0x28) - sample.offset * 256;
                orientation.road_heading = near.road[0].heading;
                orientation.heading = blob_i32(half->bike, 0x5c);
                orientation.roll = blob_i32(half->bike, 0x60);
                orientation.rider = true; // halves are type 8 (0x00413ae0 rider rule)
                const auto angles = rr::actor_angles(orientation, camera.x);
                if (angles.view_angle != int32_t(key(frame, sp + "view"))) {
                    usable = false;
                    diff = "view angle pos " + std::to_string(pos) +
                           " native=" + std::to_string(angles.view_angle) +
                           " orig=" + std::to_string(key(frame, sp + "view"));
                    break;
                }
                // (4) Full dispatch parity: ActorNodes::rider (shadow axis +
                // body with select roll, yaw clamp, 0x00450ad0 rotation) vs
                // the dump node for this bb call. Ground is the shadow
                // ground (+0x4c); rider() adds vertical>>8 for the body.
                // Colour from the traced rule, via the half's bike.
                uint32_t bike_addr = 0;
                for (const auto& r : dump.riders) {
                    if (r.bike.size() < 0x300 || blob_i32(r.bike, 0xc4) != 4) continue;
                    if (uint32_t(blob_i32(r.bike, 0x2fc)) == half->address) {
                        bike_addr = r.address;
                        break;
                    }
                }
                const int32_t colour = actor_colour(bike_addr, dump.memory, prow);
                if (colour < 0) {
                    usable = false;
                    diff = "colour rule pos " + std::to_string(pos);
                    break;
                }
                rr::ActorProjection projection;
                projection.road_x = orientation.road_x;
                projection.far_x = far.road[0].road_x + camera.x;
                projection.road_y = near.road[0].world_y;
                projection.far_y = far.road[0].world_y;
                projection.lateral = lateral;
                projection.ground = ground;
                projection.depth = near.depth;
                projection.fraction = uint8_t(uint32_t(pos));
                projection.occlusion = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".occ_row"));
                // Body roll is the dispatch +0xbc (sel key), already yaw-
                // clamped by 0x004463e0 — not the +0x60-derived view roll.
                const int32_t sel_roll = int32_t(key(frame, sp + "bc"));
                const auto made = actors.rider(descriptor, 0, colour, orientation, projection, camera, depths,
                                               blob_i32(half->bike, 0x20), &sel_roll);
                const bool is_shadow = clip == 0;
                if (made.size() != 2) {
                    usable = false;
                    diff = "dispatch node count pos " + std::to_string(pos);
                    break;
                }
                const auto& node = made[is_shadow ? 0 : 1];
                const auto& dumped = dump.nodes[size_t(nafter - 1)];
                const auto& cell = actors.library()
                                       .views[0]
                                       .at(rr::select_actor_view(descriptor, actors.library().views[0],
                                                                 angles.yaw, angles.roll)
                                               .group)
                                       .at(rr::select_actor_view(descriptor, actors.library().views[0],
                                                                 angles.yaw, angles.roll)
                                               .subframe);
                if (dumped.u32(8) != 0x00466684 || int32_t(dumped.u32(0x34)) != int32_t(cell.frame)) {
                    usable = false;
                    diff = "frame/bank pos " + std::to_string(pos);
                    break;
                }
                const int32_t shadow_colour = (colour == 0xde) ? 0xdf : 0xe0;
                const int32_t want_colour = is_shadow ? shadow_colour : colour;
                if (int32_t(dumped.u32(0x38)) != want_colour) {
                    usable = false;
                    diff = "palette pos " + std::to_string(pos);
                    break;
                }
                for (int k = 0; k < 4; ++k) {
                    if (node.view_points[size_t(k)].x != int32_t(dumped.u32(0x44 + 8 * size_t(k))) ||
                        node.view_points[size_t(k)].y != int32_t(dumped.u32(0x48 + 8 * size_t(k)))) {
                        usable = false;
                        diff = "view point " + std::to_string(k) + " pos " + std::to_string(pos);
                        break;
                    }
                }
                if (!usable) break;
                if (node.flip_u != (dumped.bytes[0x64] != 0) || node.flip_v != (dumped.bytes[0x65] != 0)) {
                    usable = false;
                    diff = "flip pos " + std::to_string(pos);
                    break;
                }
                ++checked;
            }
            if (usable && checked > 0) {
                frame_ok = true;
                frame_checked = checked;
                nodes += checked;
                break;
            }
            if (frame_error.empty()) frame_error = diff + " (checked " + std::to_string(checked) + ")";
        }
        ++frames;
        if (!frame_ok) {
            ++failures;
            std::cout << "frame " << scene_file << ": MISMATCH " << frame_error << "\n";
        } else {
            std::cout << "frame " << scene_file << ": " << frame_checked << " actor nodes match\n";
        }
    }
    if (!frames || !nodes) {
        std::cerr << "FAIL native actor comparison has no measured frame/node coverage\n";
        ++failures;
    }
    std::cout << frames << " frames, " << nodes << " actor nodes, " << failures << " failures\n";
    return failures ? 1 : 0;
}

// --native-terrain: native scene construction against the original's dumped
// display list. Each frame's sub-slice terrain values (drawn by the original
// from its live random stream, which CourseScene's isolated stream does not
// reproduce) are loaded into the native scene for the dumped strips. Every
// original terrain node (road GLOBAL CELs, WATER128, family categories 1, 3,
// 5 and 6) must then appear natively with identical texture, Q16 points and
// flips. The dump holds only the post-flush tail, so native nodes without an
// original counterpart are expected. Scenery and actors are reported only.
// Riders' ground (+0x24) is checked against 0x00439b10 on the same values.
int native_terrain(const std::string& tsv, const rr::Assets& assets, bool check_ground, bool window_mode, bool check_order, bool include_statics) {
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    const int course = int(golden.frames.at(0).at("course"));
    const auto exe = assets.read("ROADRASH.EXE");
    const auto depths = rr::read_depth_table(exe);
    const auto data =
        rr::read_course(assets.read(std::string("DATA/COURSES/") + course_files[course] + ".CRS"));
    if(std::getenv("RR_TERRAIN_VERBOSE"))
        for(const auto& [id,node]:data.nodes) if(node.kind==1 || node.kind==2)
            std::cout<<"window-node "<<id<<" kind="<<node.kind<<" length="<<node.length
                     <<" preferred="<<int(node.preferred)<<" primary="<<int(node.primary)<<'\n';
    rr::CourseScene scene(assets, exe, data, course_files[course]);
    const auto lib = rr::testlib::load_texture_library(assets, course_files[course]);
    const std::array<std::vector<rr::RouteSample>, 2> routes = {rr::make_route(data, false),
                                                                rr::make_route(data, true)};
    std::set<std::string> terrain = {"PLAIN",    "WHITE",    "YELLOW",   "START160", "START162",
                                     "SIDCRSS0", "SIDLANE0", "SIDLANE1", "FINISH",   "WATER128"};
    for (const auto& [id, family] : scene.families()) {
        std::function<void(uint32_t, int)> walk = [&](uint32_t offset, int depth) {
            if (const auto g = family.groups.find(offset); g != family.groups.end())
                for (const auto& cell : g->second.cells) terrain.insert(cell.texture.name);
            const auto n = family.nodes.find(offset);
            if (n == family.nodes.end() || n->second.tag || depth > 3) return;
            for (const auto child : n->second.children) walk(child, depth + 1);
        };
        if (const auto root = family.nodes.find(0); root != family.nodes.end())
            for (const uint32_t category : {1u, 3u, 5u, 6u})
                if (category < root->second.children.size()) walk(root->second.children[category], 1);
    }
    // Native nodes keyed by points and flips; textures compare by the dump's
    // mip-0 hash (several disc resources share pixels under other names).
    using Key = std::tuple<std::array<int32_t, 8>, bool, bool>;
    size_t frames = 0, matched = 0, missing = 0, extras = 0, others = 0, grounds = 0, ground_failures = 0, cached_ground_differences = 0;
    size_t order_failures = 0;
    struct IndexedNode { rr::DrawNode node; size_t order; };
    for (const auto& f : golden.frames) {
        if (!f.count("scene.file")) continue;
        const bool full_capture = f.count("scene.full_list") && f.at("scene.full_list");
        if ((check_order || full_capture) &&
            (!full_capture || !f.count("scene.full_capture_version") ||
             f.at("scene.full_capture_version") < 2))
            throw std::runtime_error("Complete/ordered terrain comparison requires full capture version 2; recapture with all seven flush hooks");
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(f.at("scene.file")) + ".raw");
        auto route = routes[f.at("camera.half") ? 1 : 0];
        std::vector<int32_t> countdowns(route.size(),INT32_MIN);
        std::vector<std::array<int8_t,2>> bands(route.size(),{4,4});
        std::vector<std::array<std::vector<rr::SceneryObject>,2>> placements(route.size());
        std::array<uint16_t,4> live_families{};
        for(uint32_t slot=0;slot<4;++slot) live_families[slot]=uint16_t(dump.memory.u32(0x004b7f10+slot*4));
        if(window_mode) {
            const uint32_t base=0x004c0a5c-31*0x224;
            uint32_t previous_node=0; bool previous_dual=false;
            uint8_t own=uint8_t(f.at("camera.half"));
            for(uint32_t k=0;k<33;++k) {
                const uint32_t strip=base+k*0x224;
                if(!dump.memory.has(strip+0x18)) continue;
                const int count=dump.memory.i32(strip+4);
                if(count<1 || count>2) continue;
                const auto record=dump.memory.u32(strip+0x14);
                const auto sub=dump.memory.u32(record+0xc);
                const auto owner=dump.memory.u32(sub+0x28);
                const auto index=dump.memory.u32(owner+8)>>8;
                if(index>=route.size()) continue;
                const auto seed=route[index];
                std::array<rr::RouteSample,2> half;
                for(int h=0;h<count;++h) {
                    const auto image=dump.memory.u32(dump.memory.u32(strip+0x14+h*4)+0xc);
                    auto rs=rr::route_half(seed,uint8_t(h));
                    auto I=[&](uint32_t offset) { return dump.memory.i32(image+offset); };
                    rs.node=uint32_t(I(0)); rs.local=uint32_t(I(4))>>8;
                    rs.offset=I(0xcc); rs.path.bend=I(0xb8); rs.path.rise=I(0xbc);
                    rs.path.elevation=I(0xc0);
                    if(include_statics) {
                        rr::RingStatics raw;
                        raw.road_offset=rs.offset;
                        raw.borr_count=dump.memory.u8(image+0x60);
                        raw.borr_size=dump.memory.u8(image+0x62);
                        raw.borr_mirror=(dump.memory.u8(image+0x64)&1)!=0;
                        raw.borr_selector=dump.memory.u8(image+0x63);
                        raw.billboards=dump.memory.u8(image+0x3d);
                        if(raw.borr_count>8 || raw.billboards>2) throw std::runtime_error("Invalid original static count");
                        for(uint8_t i=0;i<raw.borr_count;++i) {
                            raw.borr[i]=I(0x6c+4*i)-rs.offset;
                            raw.borr_ground[i]=I(0x8c+4*i);
                        }
                        for(uint8_t i=0;i<raw.billboards;++i) {
                            const uint32_t at=image+0x40+16*i;
                            raw.billboard_ground[i]=dump.memory.i32(at+12);
                            raw.billboard[i]={dump.memory.u8(at+2),dump.memory.u8(at),dump.memory.i32(at+8)-rs.offset,(dump.memory.u8(at+1)&1)!=0};
                        }
                        raw.grounds_ready=true;
                        placements[index][h]=rr::ring_scenery(raw);
                    }
                    rs.path.marking_flags=dump.memory.u8(image+0x37);
                    rs.path.event_type=dump.memory.u8(image+0x38);
                    for(int p=0;p<3;++p) rs.path.event_parameters[p]=dump.memory.u8(image+0x39+p);
                    rs.path.side_flags[0]=dump.memory.u8(image+0x14a);
                    rs.path.side_flags[1]=dump.memory.u8(image+0x14b);
                    rs.path.left=rs.offset-I(0xe0); rs.path.right=I(0xe4)-rs.offset;
                    rs.path.shoulder_left=I(0xe0)-I(0xe8); rs.path.shoulder_right=I(0xec)-I(0xe4);
                    rs.path.height_left=I(0x144); rs.path.height_right=I(0x140);
                    rs.dual=count==2; rs.half=uint8_t(h);
                    rs.event=rr::RoadEvent(dump.memory.u8(owner+0x2c));
                    rs.x_sep=dump.memory.i32(owner+0x24); rs.dx_sep=dump.memory.i32(owner+0x28);
                    half[h]=rs;
                }
                if(count==2) {
                    const int event=int(half[0].event);
                    if(!previous_dual && (event==4 || event==6))
                        own=uint8_t(half[1].node==previous_node);
                    for(int h=0;h<2;++h) {
                        half[h].other=half[1-h].path; half[h].other_offset=half[1-h].offset;
                        half[h].other_node=half[1-h].node; half[h].other_section=half[1-h].section;
                        half[h].other_local=half[1-h].local;
                    }
                }
                route[index]=half[count==1?0:own];
                if(std::getenv("RR_TERRAIN_VERBOSE") && (k<5 || (k>=20 && k<27)))
                    std::cout<<"window row="<<k<<" own="<<int(own)<<" event="<<int(route[index].event)
                             <<" sep="<<route[index].x_sep<<" countdown="<<int(int8_t(dump.memory.u8(owner+0x1e)))
                             <<" bands="<<int(int8_t(dump.memory.u8(owner+0x1f)))<<'/'<<int(int8_t(dump.memory.u8(owner+0x20)))
                             <<" ceiling="<<dump.memory.i32(strip+0x10)<<" limit="<<dump.memory.i32(0x004c1160+k*4)<<" margin="<<dump.memory.i32(0x004734a0+k*4)
                             <<" original0visible="<<int(dump.memory.u8(record+0x100))<<'\n';
                countdowns[index]=int8_t(dump.memory.u8(owner+0x1e));
                bands[index]={int8_t(dump.memory.u8(owner+0x1f)),int8_t(dump.memory.u8(owner+0x20))};
                previous_node=route[index].node; previous_dual=count==2;
            }
        }
        // Sub-slice values of a dumped ring entry, as native TerrainSlice
        // inputs (0x00454240 inverted for the wall offsets).
        auto load = [&](uint32_t sub, rr::TerrainSlice& t, size_t& sample, const rr::RouteSample* source = nullptr) {
            if (!dump.memory.has(sub + 0x160)) return false;
            const auto owner=dump.memory.u32(sub+0x28);
            if(!dump.memory.has(owner+8)) return false;
            sample = dump.memory.u32(owner+8) >> 8;
            if (sample >= route.size()) return false;
            const auto& rs = source ? *source : route[sample];
            auto I = [&](uint32_t o) { return dump.memory.i32(sub + o); };
            const int32_t cc = I(0xcc);
            const int type = dump.memory.u8(sub + 0x3c);
            if (cc != rs.offset) return false;
            t = scene.terrain(rs.section, rs.local);
            t.side_heights = std::array{I(0x144), I(0x140)};
            t.selectors = {dump.memory.u8(sub + 0x148), dump.memory.u8(sub + 0x149)};
            if (type != t.type)
                throw std::runtime_error("Sub-slice type differs at sample " + std::to_string(sample));
            if (type == 4)
                for (int i = 0; i < 6; ++i) {
                    t.points[size_t(i)] = {I(0xf0 + 8 * uint32_t(i)) - cc, I(0xf4 + 8 * uint32_t(i))};
                    t.shades[size_t(i)] = I(0x120 + 4 * uint32_t(i));
                }
            if (type == 1 || type == 3) {
                const int32_t l = -rs.path.left, r = rs.path.right;
                int32_t el = l - rs.path.shoulder_left, er = r + rs.path.shoulder_right;
                if (type == 3) {
                    if (I(0x160) < 1)
                        er = 2 * er - r;
                    else
                        el = 2 * el - l;
                }
                t.walls[0] = {el - (I(0x14c) - cc), el - (I(0x150) - cc), I(0x154) >= 1 ? I(0x154) : 0};
                t.walls[1] = {(I(0x158) - cc) - er, (I(0x15c) - cc) - er, I(0x160) >= 1 ? I(0x160) : 0};
            }
            return true;
        };
        const uint32_t base = 0x004c0a5c - 31 * 0x224;
        for (uint32_t k = 0; k < 33; ++k) {
            const uint32_t strip = base + k * 0x224;
            if (!dump.memory.has(strip + 0x14)) continue;
            const int count=dump.memory.i32(strip+4);
            for(int half=0;half<count && half<2;++half) {
                const uint32_t record=dump.memory.u32(strip+0x14+half*4);
                if(!dump.memory.has(record+0xc)) continue;
                const auto sub=dump.memory.u32(record+0xc);
                if(!dump.memory.has(sub+0x28)) continue;
                const auto owner=dump.memory.u32(sub+0x28);
                if(!dump.memory.has(owner+8)) continue;
                const auto index=dump.memory.u32(owner+8)>>8;
                if(index>=route.size()) continue;
                auto source=route[index];
                if(source.dual && source.half!=half) {
                    source.path=source.other; source.offset=source.other_offset;
                    source.section=source.other_section; source.local=source.other_local;
                }
                if(std::getenv("RR_TERRAIN_VERBOSE") && k<4) {
                    const auto node=dump.memory.u32(sub);
                    const auto section=dump.memory.has(node+0x10)?dump.memory.u32(node+0x10):0;
                    const auto path=dump.memory.has(section+4)?dump.memory.u32(section+4):0;
                    const auto at=path+16+uint32_t(dump.memory.i32(sub+4)>>8)*2;
                    std::cout<<"path node="<<node<<" section="<<section<<" data="<<path
                             <<" resource-rise="<<(dump.memory.has(at+1)?int(int8_t(dump.memory.u8(at+1))):99999)<<'\n';
                }
                if(std::getenv("RR_TERRAIN_VERBOSE") && k<4)
                    std::cout<<"import sample="<<index<<" half="<<half<<" local="<<(dump.memory.i32(sub+4)>>8)
                             <<" bend="<<dump.memory.i32(sub+0xb8)<<'/'<<source.path.bend
                             <<" rise="<<dump.memory.i32(sub+0xbc)<<'/'<<source.path.rise<<" before_rise="<<(dump.before.has(sub+0xbc)?dump.before.i32(sub+0xbc):99999)
                             <<" offset="<<dump.memory.i32(sub+0xcc)<<'/'<<source.offset<<'\n';
                rr::TerrainSlice t; size_t sample=0;
                if(load(sub,t,sample,&source)) scene.set_terrain(source.section,source.local,t);
            }
        }
        const uint32_t position = uint32_t(f.at("camera.position"));
        const rr::Camera camera{int32_t(f.at("camera.x")) - route[position >> 8].offset,
                                int32_t(f.at("camera.height"))};
        rr::CourseScene::ScenerySource scenery_source;
        if(window_mode && include_statics)
            scenery_source=[&](uint32_t sample,uint8_t half){return placements.at(sample)[half];};
        std::multimap<Key, IndexedNode> native;
        size_t native_order = 0;
        for (const auto& n :
             scene.draw_nodes(route, rr::project_road(route, position, camera, depths), camera, depths,{},320,nullptr,window_mode?&countdowns:nullptr,window_mode?&live_families:nullptr,window_mode?&bands:nullptr,scenery_source)) {
            std::array<int32_t, 8> v{};
            for (size_t k = 0; k < 4; ++k) {
                v[2 * k] = n.view_points[k].x;
                v[2 * k + 1] = n.view_points[k].y;
            }
            native.insert({{v, n.texture && n.flip_u, n.texture && n.flip_v}, {n, native_order++}});
        }
        size_t frame_missing = 0;
        std::optional<size_t> previous_order;
        for (const auto& s : dump.nodes) {
            if (s.u32(8) == 0x00466684) continue; // dynamic rider resource
            std::string name = s.bytes[0x66] ? "flat" + std::to_string(s.bytes[0x67]) : "";
            if (name.empty()) {
                const auto* t = lib.find(s.hash, s.size);
                name = t ? t->name : "?";
            }
            if (!include_statics && !terrain.count(name) && name.rfind("flat",0)!=0) {
                ++others;
                continue;
            }
            std::array<int32_t, 8> v{};
            for (size_t k = 0; k < 4; ++k) {
                v[2 * k] = int32_t(s.u32(0x44 + 8 * k));
                v[2 * k + 1] = int32_t(s.u32(0x48 + 8 * k));
            }
            const auto range = native.equal_range({v, !s.bytes[0x66] && s.bytes[0x64] != 0, !s.bytes[0x66] && s.bytes[0x65] != 0});
            auto it = range.first;
            for (; it != range.second; ++it) {
                const auto& n = it->second.node;
                if (s.bytes[0x66] ? (!n.texture && n.flat_index && *n.flat_index == s.bytes[0x67])
                                  : (n.texture && !n.texture->mips.empty() && s.size &&
                                     n.texture->mips[0].pixels.size() >= s.size &&
                                     rr::testlib::fnv(n.texture->mips[0].pixels.data(), s.size) == s.hash))
                    break;
            }
            if (it == range.second) {
                ++frame_missing;
                if (std::getenv("RR_TERRAIN_VERBOSE")) {
                    std::cout << "  missing " << name << " hash " << s.hash << " size " << s.size << " at";
                    for (auto value : v) std::cout << ' ' << value;
                    for (auto j = range.first; j != range.second; ++j)
                        std::cout << " | native " << (j->second.node.texture ? j->second.node.texture->name : "flat");
                    std::cout << '\n';
                    const Key* nearest = nullptr;
                    int64_t distance = INT64_MAX;
                    for (const auto& [key, indexed] : native) {
                        const auto& node = indexed.node;
                        if (!node.texture || node.texture->name != name) continue;
                        int64_t candidate = 0;
                        for (size_t k = 0; k < v.size(); ++k)
                            candidate += std::abs(int64_t(v[k]) - std::get<0>(key)[k]);
                        if (candidate < distance) { distance = candidate; nearest = &key; }
                    }
                    if (nearest) {
                        std::cout << "    nearest " << name << " at";
                        for (auto value : std::get<0>(*nearest)) std::cout << ' ' << value;
                        std::cout << " camera " << position << " half " << f.at("camera.half") << '\n';
                    }
                }
                continue;
            }
            if (check_order && previous_order && it->second.order <= *previous_order) {
                if (order_failures < 20)
                    std::cerr << "terrain draw-order mismatch frame " << f.at("scene.file")
                              << " at " << name << ": native node " << it->second.order
                              << " follows " << *previous_order << '\n';
                ++order_failures;
            }
            previous_order = it->second.order;
            native.erase(it);
            ++matched;
        }
        // Riders' ground: 0x0043a190 -> 0x00439b10(+0x140 sub-slice, +0x28, +0x138).
        for (const auto& r : dump.riders) {
            if (r.bike.size() < 0x340) continue;
            const uint32_t sub = uint32_t(r.bike_i32(0x140));
            rr::TerrainSlice t, tn;
            size_t sample = 0, next_sample = 0;
            if (!load(sub, t, sample)) continue;
            const uint32_t forward = sub + (t.type == 4 ? 0x0c : 8);
            if (!dump.memory.has(forward) || !load(dump.memory.u32(forward), tn, next_sample)) {
                tn = t;
                next_sample = sample;
            }
            const auto& rs = route[sample];
            const auto& rn = route[next_sample];
            const int32_t got =
                rr::terrain_height(t, rs.path, tn, rn.path, r.bike_i32(0x28) - rs.offset * 256,
                                   uint8_t(r.bike_i32(0x138)), rn.offset - rs.offset, rs.offset);
            const std::string query = "terrain.query." + std::to_string(r.address) + ".";
            const bool fresh = f.count(query + "height") != 0;
            if (fresh && (f.at(query + "sub") != sub || f.at(query + "lateral") != r.bike_i32(0x28) ||
                          uint32_t(f.at(query + "fraction")) != uint32_t(r.bike_i32(0x138))))
                throw std::runtime_error("Ground query inputs differ from captured object");
            const int32_t original = fresh ? int32_t(f.at(query + "height")) : r.bike_i32(0x24);
            if (fresh && original != r.bike_i32(0x24)) ++cached_ground_differences;
            ++grounds;
            if (got != original) {
                if (ground_failures < 20)
                    std::cerr << "ground mismatch frame " << f.at("scene.file") << " rider " << r.address
                              << " sample " << sample << " type " << int(t.type) << " original "
                              << original << " native " << got << " lateral " << r.bike_i32(0x28)
                              << " swept " << r.bike_i32(0x138) << '\n';
                ++ground_failures;
            }
        }
        if(f.count("scene.full_list") && f.at("scene.full_list")) {
            size_t frame_extras=0;
            for(const auto& [key,indexed]:native) {
                const auto& node = indexed.node;
                if((node.texture && (include_statics || terrain.count(node.texture->name))) || (!node.texture && node.flat_index)) {
                    ++frame_extras;
                    if(std::getenv("RR_TERRAIN_VERBOSE") && frame_extras<12) {
                        std::cout<<"  extra "<<(node.texture?node.texture->name:"flat")<<" at";
                        for(const auto& point:node.view_points) std::cout<<' '<<point.x<<' '<<point.y;
                        std::cout<<'\n';
                    }
                }
            }
            extras+=frame_extras;
        }
        ++frames;
        missing += frame_missing;
        if (frame_missing)
            std::cout << "frame " << f.at("scene.file") << ": " << frame_missing
                      << " original terrain nodes without a native match\n";
    }
    if (!frames) {
        if (window_mode) throw std::runtime_error("Requested terrain-window capture contains no scene frames");
        std::cout << "skip: capture has no scene frames\n";
        return rr::golden::skip;
    }
    std::cout << frames << " frames: " << matched << (include_statics?" terrain/scenery nodes match, ":" terrain nodes match, ") << missing << " missing, "
              << extras << " extra native terrain nodes; " << others << " scenery/actor nodes not compared; " << grounds << " rider grounds, "
              << ground_failures << " differ; " << cached_ground_differences
              << " stored grounds differ from fresh queries\n";
    if (check_order) std::cout << order_failures << " terrain draw-order mismatches\n";
    return missing || extras || order_failures || (check_ground && ground_failures) ? 1 : 0;
}
} // namespace

int run(int argc, char** argv) {
    bool actors_mode = false, terrain_mode = false, check_ground = true, window_mode = false, check_order = false, include_statics = false, discover_scenes = false;
    const std::string dir = rr::golden::oracle_dir();
    std::string tsv = dir + "/city-scene.tsv";
    std::string assets_root = rr::test::assets_root();
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--native-actors")
            actors_mode = true;
        else if (a == "--discover-scenes")
            discover_scenes = true;
        else if (a == "--native-window")
            terrain_mode = window_mode = true;
        else if (a == "--native-static-window")
            terrain_mode = window_mode = check_order = include_statics = true;
        else if (a == "--ordered-terrain")
            terrain_mode = window_mode = check_order = true;
        else if (a == "--native-terrain")
            terrain_mode = true;
        else if (a == "--nodes-only")
            check_ground = false; // report rider grounds without gating
        else if (a == "--assets" && i + 1 < argc)
            assets_root = argv[++i];
        else if (a[0] != '-')
            tsv = a;
    }
    rr::Assets assets(assets_root);
    std::ifstream probe(tsv);
    if (!probe || !assets.exists("DATA/GLOBAL.RSC")) {
        std::cout << "skip: no scene capture or disc\n";
        return rr::golden::skip;
    }
    if (actors_mode) return native_actors(tsv, assets);
    if (terrain_mode) return native_terrain(tsv, assets, check_ground, window_mode, check_order, include_statics);
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    // Simulation captures report the selected menu course (1..5); legacy
    // scene probes report the zero-based runtime table index.
    const int course = int(golden.frames.at(0).at("course")) - (discover_scenes ? 1 : 0);
    if(course<0 || course>=5) throw std::runtime_error("Invalid captured scene course");
    const auto lib = rr::testlib::load_texture_library(assets, course_files[course]);
    const auto exe = assets.read("ROADRASH.EXE");
    const auto sprites = rr::load_rider_sprites(assets, exe, std::getenv("RR_SCENE_LOBIKES") == nullptr);
    std::cout << lib.textures.size() << " textures\n";
    std::vector<rr::golden::Frame> scene_frames;
    if(discover_scenes) {
        // SIM traces are indexed by simulation tick, while these files are
        // indexed by render frame. Read their descriptors from the captured
        // original memory rather than inventing metadata in the golden trace.
        const auto source=std::filesystem::path(tsv);
        const auto prefix=source.stem().string()+"-scene";
        std::map<uint32_t,rr::golden::Frame> ordered;
        for(const auto& entry:std::filesystem::directory_iterator(source.parent_path())) {
            const auto name=entry.path().filename().string();
            if(!entry.is_regular_file() || !name.starts_with(prefix) || !name.ends_with(".raw") ||
                name.ends_with("-native.raw")) continue;
            const auto dump=rr::scene::load(entry.path().string());
            rr::golden::Frame frame;
            frame["scene.file"]=frame["pixels.file"]=dump.frame;
            frame["pixels.width"]=dump.memory.i32(0x0047539c);
            frame["pixels.height"]=dump.memory.i32(0x004753a0);
            frame["view.high"]=dump.memory.u8(0x00475380);
            frame["view.quality_mask"]=dump.memory.u32(0x00475648);
            if(frame["pixels.width"]<1 || frame["pixels.height"]<1)
                throw std::runtime_error("Invalid captured framebuffer dimensions");
            if(!ordered.emplace(dump.frame,std::move(frame)).second)
                throw std::runtime_error("Duplicate captured scene frame");
        }
        for(auto& [index,frame]:ordered) scene_frames.push_back(std::move(frame));
        if(scene_frames.empty()) throw std::runtime_error("No original scene files discovered");
    } else {
        for(const auto& frame:golden.frames)
            if(frame.count("scene.file") && frame.count("pixels.file")) scene_frames.push_back(frame);
    }
    int failures = 0, frames = 0;
    for (const auto& f : scene_frames) {
        if (!f.count("scene.file") || !f.count("pixels.file")) continue;
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(f.at("scene.file")) + ".raw");
        const auto pixels = read_file(stem + "-f" + std::to_string(f.at("pixels.file")) + ".raw");
        const int width = int(f.at("pixels.width")), height = int(f.at("pixels.height"));
        if (pixels.size() != size_t(width) * size_t(height)) {
            ++failures;
            continue;
        }
        rr::Framebuffer fb(width, height);
        for (int y = 0; y < height; ++y)
            std::memcpy(fb.row(y), pixels.data() + size_t(y) * size_t(width), size_t(width));
        std::vector<rr::DrawNode> nodes;
        std::vector<std::unique_ptr<rr::CelTexture>> dynamic;
        int unmatched = 0;
        for (const auto& s : dump.nodes) {
            rr::DrawNode n;
            for (int k = 0; k < 4; ++k)
                n.view_points[size_t(k)] = {int32_t(s.u32(0x44 + 8 * size_t(k))),
                                            int32_t(s.u32(0x48 + 8 * size_t(k)))};
            n.flip_u = s.bytes[0x64] != 0;
            n.flip_v = s.bytes[0x65] != 0;
            if (s.u32(8) == 0x00466684) { // dynamic "Bob" resource (0x00466688 holds its address)
                auto t = rr::rider_texture(sprites, int32_t(s.u32(0x38)), int32_t(s.u32(0x34)));
                if (!t) continue;
                dynamic.push_back(std::make_unique<rr::CelTexture>(std::move(*t)));
                n.texture = dynamic.back().get();
            } else if (s.bytes[0x66])
                n.flat_index = s.bytes[0x67];
            else if (!(n.texture = lib.find(s.hash, s.size))) {
                std::string name;
                for(size_t k=16;k<32 && s.resource[k];++k) name.push_back(char(s.resource[k]));
                std::cerr<<"unresolved frame="<<f.at("scene.file")<<" name="<<name
                    <<" resource="<<s.u32(8)<<" hash="<<s.hash<<" size="<<s.size<<'\n';
                ++unmatched;
                continue;
            }
            nodes.push_back(n);
        }
        try {
            for (size_t i = 0; i < nodes.size(); ++i) {
                try {
                    rr::render_nodes(fb, {nodes[i]}, rr::view_mode(f.at("view.high") != 0),
                                     uint32_t(f.count("view.quality_mask") ? f.at("view.quality_mask") : 16));
                } catch (const std::exception& e) {
                    const auto* t = nodes[i].texture;
                    if (t)
                        std::cerr << "texture " << t->name << " bounds " << t->bounds[0] << ','
                                  << t->bounds[1] << ',' << t->bounds[2] << ',' << t->bounds[3] << " mip0 "
                                  << t->mips[0].width << 'x' << t->mips[0].height << '\n';
                    throw std::runtime_error("node " + std::to_string(i) + ": " + e.what());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "frame " << f.at("scene.file") << ": " << e.what() << "\n";
            ++failures;
            continue;
        }
        size_t differ = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                differ += fb.row(y)[x] != pixels[size_t(y) * size_t(width) + size_t(x)];
        std::cout << "frame " << f.at("scene.file") << ": " << dump.nodes.size() << " nodes, " << nodes.size()
                  << " rendered, " << unmatched << " unmatched, " << differ << " differing pixels\n";
        if (differ || unmatched) {
            ++failures;
            std::ofstream out(stem + "-scene" + std::to_string(f.at("scene.file")) + "-native.raw",
                              std::ios::binary);
            for (int y = 0; y < height; ++y) out.write(reinterpret_cast<const char*>(fb.row(y)), width);
        }
        ++frames;
    }
    if (!frames) {
        std::cout << "skip: capture has no scene frames\n";
        return rr::golden::skip;
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "oracle scene: " << e.what() << '\n';
        return 1;
    }
}
