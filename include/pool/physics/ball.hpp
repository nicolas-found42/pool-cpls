#pragma once

#include "pool/physics/vec.hpp"

#include <cstdint>

namespace pool::physics {

// Ball identity follows the standard 8-ball numbering: 0 is the cue ball,
// 1-7 solids, 8 the eight, 9-15 stripes.
inline constexpr int kCueBall = 0;
inline constexpr int kEightBall = 8;
inline constexpr int kBallCount = 16;

enum class BallState : std::uint8_t { resting, sliding, rolling, pocketed };

struct Ball {
  std::uint8_t id{0};
  bool on_table{true};
  Vec2 position{}; // m, centre, table frame: x across length, y across width
  Vec2 velocity{}; // m/s
  Vec3 spin{};     // rad/s, x/y horizontal, z vertical (english)
  Quat orientation{};
  BallState state{BallState::resting};
};

inline bool is_stripe(std::uint8_t id) noexcept {
  return id >= 9 && id <= 15;
}
inline bool is_solid(std::uint8_t id) noexcept {
  return id >= 1 && id <= 7;
}

} // namespace pool::physics
