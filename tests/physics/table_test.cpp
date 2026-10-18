#include "pool/physics/table.hpp"
#include "pool/physics/world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace pool::physics;

namespace {
constexpr double kRadius = 0.028575;
}

TEST_CASE("pocket mouths match the WPA equipment specification", "[table]") {
  // WPA Recommended Equipment Specifications §9: corner pocket mouth 4.5-4.625
  // in, side pocket mouth 5-5.125 in, the side being ~0.5 in wider.
  const TableSpec table{};
  const auto pockets = make_pockets(table);

  REQUIRE(table.corner_mouth >= 0.1143);
  REQUIRE(table.corner_mouth <= 0.1175);
  REQUIRE(table.side_mouth >= 0.127);
  REQUIRE(table.side_mouth <= 0.130175);

  const double corner_diameter = pockets[0].radius * 2.0;
  const double side_diameter = pockets[1].radius * 2.0;
  REQUIRE(corner_diameter == Catch::Approx(table.corner_mouth));
  REQUIRE(side_diameter == Catch::Approx(table.side_mouth));
  // The side pocket is wider than the corner pocket, not narrower.
  REQUIRE(side_diameter > corner_diameter + 0.012);
}

TEST_CASE("ball and table dimensions match the WPA specification", "[table]") {
  // §5: 100 x 50 in playing surface. §16: balls 2.25 in diameter, 5.5-6 oz.
  const BallSpec ball{};
  REQUIRE(ball.radius * 2.0 == Catch::Approx(0.05715).margin(1.0e-5));
  REQUIRE(ball.mass >= 0.156);
  REQUIRE(ball.mass <= 0.170);

  const TableSpec table{};
  REQUIRE(table.length == Catch::Approx(2.54));
  REQUIRE(table.width == Catch::Approx(1.27));
}

TEST_CASE("a 9-foot table has six pockets", "[table]") {
  const auto pockets = make_pockets(TableSpec{});
  REQUIRE(pockets.size() == 6);
}

TEST_CASE("pockets sit on the playing-surface corners and rail midpoints", "[table]") {
  const TableSpec table{};
  const auto pockets = make_pockets(table);

  REQUIRE(pockets[0].centre.x == Catch::Approx(0.0));
  REQUIRE(pockets[0].centre.y == Catch::Approx(0.0));
  REQUIRE(pockets[3].centre.x == Catch::Approx(0.0));
  REQUIRE(pockets[3].centre.y == Catch::Approx(table.width));
  REQUIRE(pockets[1].centre.x == Catch::Approx(table.length * 0.5));
}

TEST_CASE("coefficients of restitution match measured bands", "[table]") {
  // Measured ranges: ball-to-ball 0.92-0.98 (nominal 0.93-0.94); ball-to-rail
  // 0.60-0.90 (nominal 0.70-0.72). Sources in docs/research.
  const MaterialParams material{};
  INFO("ball_restitution " << material.ball_restitution << ", cushion_restitution "
                           << material.cushion_restitution);
  REQUIRE(material.ball_restitution >= 0.92);
  REQUIRE(material.ball_restitution <= 0.98);
  REQUIRE(material.cushion_restitution >= 0.60);
  REQUIRE(material.cushion_restitution <= 0.90);
}

TEST_CASE("bounds inset the playing surface by one radius", "[table]") {
  const TableSpec table{};
  const Bounds bounds = make_bounds(table, kRadius);

  REQUIRE(bounds.min_x == Catch::Approx(kRadius));
  REQUIRE(bounds.max_x == Catch::Approx(table.length - kRadius));
  REQUIRE(bounds.min_y == Catch::Approx(kRadius));
  REQUIRE(bounds.max_y == Catch::Approx(table.width - kRadius));
}

TEST_CASE("the 8-ball rack holds 16 balls with no overlaps", "[table]") {
  World world;
  rack_eight_ball(world, world.table(), kRadius, 1.0e-4);

  const auto placed = std::count_if(world.balls().begin(), world.balls().end(),
                                    [](const Ball& entry) { return entry.on_table; });
  REQUIRE(placed == 16);

  for (std::size_t i = 0; i < world.balls().size(); ++i) {
    for (std::size_t j = i + 1; j < world.balls().size(); ++j) {
      const Ball& a = world.balls()[i];
      const Ball& b = world.balls()[j];
      if (!a.on_table || !b.on_table)
        continue;
      const double distance = length(b.position - a.position);
      INFO("balls " << int(a.id) << " and " << int(b.id));
      REQUIRE(distance >= 2.0 * kRadius);
    }
  }
}

TEST_CASE("the eight ball is centred in the third row", "[table]") {
  World world;
  rack_eight_ball(world, world.table(), kRadius, 1.0e-4);

  const Ball& eight = world.ball(kEightBall);
  REQUIRE(eight.position.y == Catch::Approx(world.table().width * 0.5));
  REQUIRE(eight.position.x > world.table().length * 0.75);
}

TEST_CASE("racked balls sit inside the cushion bounds", "[table]") {
  World world;
  rack_eight_ball(world, world.table(), kRadius, 1.0e-4);
  const Bounds bounds = world.bounds();

  for (const Ball& ball : world.balls()) {
    if (!ball.on_table)
      continue;
    INFO("ball " << int(ball.id));
    REQUIRE(ball.position.x >= bounds.min_x);
    REQUIRE(ball.position.x <= bounds.max_x);
    REQUIRE(ball.position.y >= bounds.min_y);
    REQUIRE(ball.position.y <= bounds.max_y);
  }
}
