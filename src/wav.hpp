#pragma once
#include <cstdint>
#include <vector>
namespace rr {
struct PcmSound {
    int rate = 0, channels = 0;
    std::vector<int16_t> samples;
    uint32_t loop_begin=0, loop_end=0; // end-exclusive; zero means the full buffer
};
PcmSound read_wav(const std::vector<uint8_t>& riff);
} // namespace rr
