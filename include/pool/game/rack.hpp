#pragma once

#include "pool/physics/world.hpp"

#include <cstdint>
#include <vector>

namespace pool::game {

// Rack the 15 object balls plus the cue ball into a fresh world.
void rack(physics::World& world);

// Snapshot of which balls are still on the table, used to diff after a shot.
struct TableSnapshot {
  bool on_table[physics::kBallCount]{};
};

[[nodiscard]] TableSnapshot snapshot(const physics::World& world);

// Balls that were on the table before the shot and are gone after it.
[[nodiscard]] std::vector<std::uint8_t> pocketed_between(const TableSnapshot& before,
                                                         const physics::World& after);

} // namespace pool::game
