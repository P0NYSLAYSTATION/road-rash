#pragma once
#include "road_panels.hpp"
#include "world_renderer.hpp"
namespace rr {
struct RoadMaterials {
    std::array<Cel, 8> entries;
    std::array<Cel, 3> terrain;
    std::array<Cel, 4> signals; // housing, phase0/1/2 overlays (00468ee8/ef0)
};
// 0x00441770 descriptor table at 0x0046dfb0; IDs are read from imported executable data.
RoadMaterials read_road_materials(const std::vector<uint8_t>& exe, const std::vector<uint8_t>& global);
std::vector<DrawNode> road_draw_nodes(const std::vector<RoadPanel>&, const RoadMaterials&);
} // namespace rr
