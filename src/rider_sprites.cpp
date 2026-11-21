#include "rider_sprites.hpp"
#include "byte_reader.hpp"
#include "raster_rgb.hpp"
#include <stdexcept>
#include <algorithm>

namespace rr {
RiderSprites load_rider_sprites(const Assets& assets, const std::vector<uint8_t>& exe, bool high) {
    const std::string p = std::string("DATA/BIKERS/") + (high ? "HI" : "LO");
    RiderSprites s;
    s.bob = read_bobs(assets.read(p + "BOB.DAT"), bob_frames);
    if (assets.exists(p + "COP.DAT"))
        s.cop = read_bobs(assets.read(p + "COP.DAT"), cop_frames);
    else if (assets.exists("DATA/BIKERS/LOCOP.DAT"))
        s.cop = read_bobs(assets.read("DATA/BIKERS/LOCOP.DAT"), cop_frames);
    s.shadow = read_bobs(assets.read(p + "SHADOW.DAT"), bob_frames);
    s.shadoc = read_bobs(assets.read(p + "SHADOC.DAT"), cop_frames);
    // 00432f90 loads the standalone MIP body after its size word into
    // resource +28; mip offsets are relative to that body. 00443820 only
    // overwrites the selected frame rectangle, leaving the template outside.
    const auto page = assets.read("DATA/BIKERS/TEMPLATE.MIP");
    const ByteReader header(page);
    if (header.le<uint32_t>(0) != page.size() - 4 || header.le<uint32_t>(8) != 256 ||
        header.le<uint32_t>(12) != 256 || header.le<uint32_t>(16) == 0)
        throw std::runtime_error("Invalid rider template MIP");
    const auto pixels = header.bytes(size_t(header.le<uint32_t>(32)) + 4, 65536);
    s.template_pixels.assign(pixels.begin(), pixels.end());
    s.remaps = read_rider_remaps(exe, 15);
    return s;
}

std::optional<CelTexture> rider_texture(const RiderSprites& s, int32_t colour, int32_t frame) {
    // 0x00410080 validity rule (riders = 0x004642d4 - 1 = 15).
    if (colour < 0 || frame < 0) return std::nullopt;
    if ((colour == 0xde || colour == 0xdf) && frame >= 0x2d) return std::nullopt;
    if ((colour < 15 || colour == 0xe0) && frame >= 0x146) return std::nullopt;
    const std::vector<Image8>* sheet = nullptr;
    if (colour < 15)
        sheet = &s.bob;
    else if (colour == 0xde)
        sheet = &s.cop;
    else if (colour == 0xdf)
        sheet = &s.shadoc;
    else if (colour == 0xe0)
        sheet = &s.shadow;
    if (!sheet || size_t(frame) >= sheet->size()) return std::nullopt;
    const Image8& f = (*sheet)[size_t(frame)];
    CelTexture t;
    t.bounds = {0, 0, f.width, f.height};
    t.flags = 2; // transparent index 0
    t.rider_page = true;
    CelMip mip;
    mip.width = 256;
    mip.height = 256;
    mip.stride_shift = 8;
    // Empty shadow frames still pass through 00443820 and rasterization.
    // Their zero bounds produce wrapped unsigned UVs which sample the
    // untouched last row of TEMPLATE.MIP. Skipping them loses real pixels.
    if (s.template_pixels.size() != 65536)
        throw std::runtime_error("Missing rider template pixels");
    mip.pixels = s.template_pixels;
    for (int y = 0; y < f.height; ++y)
        for (int x = 0; x < f.width; ++x) {
            const uint8_t v = f.pixels[size_t(y) * size_t(f.width) + size_t(x)];
            mip.pixels[size_t(y) * 256 + size_t(x)] = colour < 15 ? s.remaps.tables.at(size_t(colour))[v] : v;
        }
    t.mips.push_back(std::move(mip));
    return t;
}
void RiderPageState::prepare(std::vector<DrawNode>& nodes, const ViewMode& mode, uint32_t flat_mask) {
    size_t used = 0;
    for (auto& node : nodes) {
        if (!node.texture || !node.texture->rider_page || (node.texture->flags & flat_mask)) continue;
        std::array<RasterPoint, 4> points;
        for (size_t i = 0; i < points.size(); ++i)
            points[i] = {mode.map(node.view_points[i].x), mode.map(node.view_points[i].y)};
        int32_t minx = points[0].x, maxx = minx, miny = points[0].y, maxy = miny;
        for (const auto& p : points) {
            minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
            miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
        }
        if (minx > mode.surface_width || maxx < 0 || miny > mode.surface_height - 1 || maxy < 0) continue;
        bool flip = node.flip_u;
        if (!normalize_quad(points, node.texture->flags, &flip,
                            mode.surface_width > 640 || mode.surface_height > 480)) continue;
        const auto& source = *node.texture;
        if (pixels_.size() != 65536 || source.mips.empty())
            throw std::runtime_error("Invalid dynamic rider page");
        const int width = source.bounds[2], height = source.bounds[3];
        if (width < 0 || width > 256 || height < 0 || height > 256)
            throw std::runtime_error("Invalid dynamic rider rectangle");
        // 00443820 does not clear between nodes, including empty shadows.
        // Keep this serial and copy snapshots before raster workers read them.
        for (int y = 0; y < height; ++y)
            std::copy_n(source.mips[0].pixels.begin() + size_t(y) * 256, width,
                        pixels_.begin() + size_t(y) * 256);
        if (used == snapshots_.size()) snapshots_.push_back(std::make_unique<CelTexture>());
        auto& snapshot = *snapshots_[used++];
        invalidate_rgba_mip_cache(snapshot);
        snapshot = source;
        snapshot.rider_page = false;
        snapshot.mips[0].pixels = pixels_;
        node.texture = &snapshot;
    }
}
} // namespace rr
