#pragma once
#include <string>
namespace rr {
struct PortConfig {
    bool audio_3do = false;
    std::string audio_directory;
    bool directx9 = true;
};
std::string save_directory();
PortConfig port_config(const std::string& asset_root = {});
} // namespace rr
