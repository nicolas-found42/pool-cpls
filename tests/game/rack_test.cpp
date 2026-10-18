#include "pool/game/rack.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace pool::game;
namespace physics = pool::physics;
using physics::Ball;
using physics::World;

TEST_CASE("a rack puts 16 balls on the table", "[rack]") {
  World world;
  rack(world);

  const auto on_table = std::count_if(world.balls().begin(), world.balls().end(),
                                      [](const Ball& entry) { return entry.on_table; });
  REQUIRE(on_table == physics::kBallCount);
  REQUIRE(world.at_rest());
}

TEST_CASE("the rack is deterministic", "[rack]") {
  World first;
  World second;
  rack(first);
  rack(second);

  REQUIRE(first.fingerprint() == second.fingerprint());
}

TEST_CASE("the snapshot records what is on the table", "[rack]") {
  World world;
  rack(world);
  const TableSnapshot snap = snapshot(world);

  for (bool present : snap.on_table) {
    REQUIRE(present);
  }
}

TEST_CASE("pocketed_between returns only newly pocketed balls", "[rack]") {
  World world;
  rack(world);
  const TableSnapshot before = snapshot(world);

  REQUIRE(world.pocket(3));
  const auto pocketed = pocketed_between(before, world);

  REQUIRE(pocketed.size() == 1);
  REQUIRE(pocketed.front() == 3);
}

TEST_CASE("pocketed_between is empty when nothing changed", "[rack]") {
  World world;
  rack(world);
  const TableSnapshot before = snapshot(world);

  REQUIRE(pocketed_between(before, world).empty());
}
