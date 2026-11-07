#include "port_config.hpp"
#include "utf8_path.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <stdexcept>
namespace rr {
std::string save_directory() {
    const auto base = path_from_utf8(SDL_GetBasePath() ? SDL_GetBasePath() : ".");
    auto path = base / "saves";
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) throw std::runtime_error("Cannot create save folder: " + path_to_utf8(path));
    return path_to_utf8(path);
}
PortConfig port_config(const std::string& root) {
    PortConfig c;
    const std::filesystem::path base = path_from_utf8(SDL_GetBasePath() ? SDL_GetBasePath() : ".");
    c.audio_directory = path_to_utf8(base / "3do_audio");
    auto parse = [&](const std::filesystem::path& path) {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            auto key = line.substr(0, eq), value = line.substr(eq + 1);
            if (key == "audio") c.audio_3do = value == "3do";
            if (key == "3do_audio") {
                auto p = path_from_utf8(value);
                c.audio_directory = path_to_utf8(p.is_absolute() ? p : base / p);
            }
            if (key == "renderer") c.directx9 = value != "sdl";
        }
    };
    if (!root.empty()) parse(path_from_utf8(root) / "rr-port.ini");
    parse(base / "rr-port.ini");
    if (const char* mode = SDL_getenv("RR_AUDIO_MODE")) c.audio_3do = std::string(mode) == "3do";
    if (const char* dir = SDL_getenv("RR_3DO_AUDIO_DIR")) c.audio_directory = dir;
    if (const char* renderer = SDL_getenv("RR_RENDERER")) c.directx9 = std::string(renderer) == "dx9";
    return c;
}
} // namespace rr
