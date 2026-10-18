#include "pool/game/rack.hpp"

#include "pool/physics/table.hpp"

namespace pool::game {

void rack(physics::World& world) {
  world.reset();
  physics::rack_eight_ball(world, world.table(), physics::BallSpec{}.radius, 1.0e-4);
}

TableSnapshot snapshot(const physics::World& world) {
  TableSnapshot snap{};
  for (const physics::Ball& ball : world.balls()) {
    snap.on_table[ball.id] = ball.on_table;
  }
  return snap;
}

std::vector<std::uint8_t> pocketed_between(const TableSnapshot& before,
                                           const physics::World& after) {
  std::vector<std::uint8_t> pocketed;
  for (const physics::Ball& ball : after.balls()) {
    if (before.on_table[ball.id] && !ball.on_table) {
      pocketed.push_back(ball.id);
    }
  }
  return pocketed;
}

} // namespace pool::game
