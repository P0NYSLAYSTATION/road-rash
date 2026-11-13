#pragma once
#include "golden.hpp"

namespace rr::test {
// Use genuinely different roadway views, spread across the recording. The
// countdown can have many scene dumps while the camera never moves.
inline std::set<int64_t> moving_scene_files(const rr::golden::Frames& frames) {
    std::vector<int64_t> candidates;
    std::set<int64_t> positions;
    const auto initial = frames.front().at("camera.position");
    for (const auto& frame : frames) {
        if (!frame.count("scene.file") || !frame.count("pixels.file")) continue;
        const auto position = frame.at("camera.position");
        if (position != initial && positions.insert(position).second)
            candidates.push_back(frame.at("scene.file"));
    }
    if (candidates.size() < 5)
        throw std::runtime_error("Moving capture needs five distinct camera positions after startup");
    std::set<int64_t> selected;
    for (size_t i = 0; i < 5; ++i) selected.insert(candidates[i * (candidates.size() - 1) / 4]);
    return selected;
}
} // namespace rr::test
