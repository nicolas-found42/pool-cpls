#include "pool/game/rack.hpp"

#include <chrono>
#include <cstdio>

// Deliberately dependency-free: a benchmark you can run before and after a
// change without pulling in Google Benchmark. Prints milliseconds per shot.
int main() {
  constexpr int kShots = 200;
  double total_ms = 0.0;
  std::size_t substeps = 0;

  for (int shot = 0; shot < kShots; ++shot) {
    pool::physics::World world;
    pool::game::rack(world);

    pool::physics::CueStrike strike{};
    strike.direction = {1.0, 0.0};
    strike.speed = 4.0 + static_cast<double>(shot % 8);
    world.strike(strike);

    const auto start = std::chrono::steady_clock::now();
    const int steps = world.substeps_until_rest();
    world.advance(steps);
    const auto end = std::chrono::steady_clock::now();

    total_ms += std::chrono::duration<double, std::milli>(end - start).count();
    substeps += static_cast<std::size_t>(steps);
  }

  std::printf("%d shots, %.1f ms total, %.3f ms/shot, %zu substeps\n", kShots, total_ms,
              total_ms / kShots, substeps);
  return 0;
}
