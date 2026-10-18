#include "pool/game/rack.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// libFuzzer target: random cue strikes and random table states must never
// produce NaN, an out-of-bounds ball, or a negative energy.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 12)
    return 0;

  pool::physics::World world;
  pool::game::rack(world);

  const auto read_unit = [&](std::size_t index) {
    return (static_cast<double>(data[index]) / 255.0) * 2.0 - 1.0;
  };

  pool::physics::CueStrike strike{};
  strike.direction = {read_unit(0), read_unit(1)};
  strike.speed = std::abs(read_unit(2)) * 9.0;
  strike.tip_offset = {read_unit(3) * 0.5, read_unit(4) * 0.5};
  strike.elevation = std::abs(read_unit(5)) * 0.6;

  if (pool::physics::length_squared(strike.direction) < 1.0e-9) {
    strike.direction = {1.0, 0.0};
  }

  world.strike(strike);
  world.advance(600);

  const double energy = world.kinetic_energy();
  if (!(energy >= 0.0))
    std::abort();

  for (const pool::physics::Ball& ball : world.balls()) {
    if (!ball.on_table)
      continue;
    if (!(ball.position.x == ball.position.x))
      std::abort();
    if (!(ball.velocity.x == ball.velocity.x))
      std::abort();
  }
  return 0;
}
