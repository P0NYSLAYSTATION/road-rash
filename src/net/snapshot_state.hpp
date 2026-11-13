#pragma once
#include "race_state.hpp"
#include <span>
namespace rr {
// Portable, bounded state messages. No host pointers, padding or immutable game
// assets cross the network. A failed decode leaves the live instance unchanged.
std::vector<uint8_t> encode_snapshot_state(const RaceInstance&, std::span<const SoundEventRecord> sounds = {});
bool apply_snapshot_state(RaceInstance&, std::span<const uint8_t>);
}
