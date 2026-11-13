#include "assets.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>
#ifdef RR_WEB_BROWSER
#include "platform_web.hpp"
#endif

namespace rr {
Assets::Assets(std::string root, std::string video_root)
    : root_(std::move(root)), video_root_(std::move(video_root)) {}

std::string Assets::path(const std::string& relative) const {
    // Explicit CD media location; never search renamed or backup directories.
    return ((!video_root_.empty() && relative.rfind("VIDEO/", 0) == 0) ? video_root_ : root_) + "/" + relative;
}

std::vector<uint8_t> Assets::read(const std::string& relative) const {
#ifdef RR_WEB_BROWSER
    if (root_ == "/assets") return web_read_asset(relative);
#endif
    const std::string path = this->path(relative);
    size_t size = 0;
    void* bytes = SDL_LoadFile(path.c_str(), &size);
    if (!bytes) throw std::runtime_error(path + ": " + SDL_GetError());
    auto start = static_cast<const uint8_t*>(bytes);
    std::vector<uint8_t> result(start, start + size);
    SDL_free(bytes);
    return result;
}

bool Assets::exists(const std::string& relative) const {
#ifdef RR_WEB_BROWSER
    if (root_ == "/assets") return web_asset_exists(relative);
#endif
    SDL_IOStream* io = SDL_IOFromFile(path(relative).c_str(), "rb");
    if (!io) return false;
    SDL_CloseIO(io);
    return true;
}
} // namespace rr
