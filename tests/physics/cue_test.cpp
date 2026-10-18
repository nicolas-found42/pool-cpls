#include "pool/physics/cue.hpp"
#include "pool/physics/world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pool::physics;

namespace {

constexpr double kRadius = 0.028575;

CueStrike strike(Vec2 direction, double speed, Vec2 offset = {}, double elevation = 0.0) {
  CueStrike s{};
  s.direction = direction;
  s.speed = speed;
  s.tip_offset = offset;
  s.elevation = elevation;
  return s;
}

} // namespace

TEST_CASE("a centred strike sends the cue ball straight at full speed", "[cue]") {
  Ball cue{};
  cue.on_table = true;
  const Vec2 velocity = apply_cue_strike(cue, strike({1.0, 0.0}, 3.0), BallSpec{}, MaterialParams{},
                                         SimulationParams{});

  // Slight efficiency loss at the tip only.
  REQUIRE(velocity.x > 2.9);
  REQUIRE(velocity.x < 3.0);
  REQUIRE(velocity.y == Catch::Approx(0.0).margin(1.0e-12));
  REQUIRE(cue.state == BallState::sliding);
}

TEST_CASE("side english produces vertical spin and a squirt deflection", "[cue]") {
  Ball cue{};
  cue.on_table = true;
  const Vec2 velocity = apply_cue_strike(cue, strike({1.0, 0.0}, 3.0, {0.4, 0.0}), BallSpec{},
                                         MaterialParams{}, SimulationParams{});

  REQUIRE(std::abs(cue.spin.z) > 0.0);
  // Squirt deflects the ball away from the side the tip strikes: the tip meets
  // the +y side of the ball, so the cue ball leaves toward -y.
  REQUIRE(velocity.y < 0.0);
  REQUIRE(velocity.x > 2.5);
}

TEST_CASE("squirt can be disabled for debugging", "[cue]") {
  SimulationParams params{};
  params.enable_squirt = false;

  Ball cue{};
  cue.on_table = true;
  const Vec2 velocity = apply_cue_strike(cue, strike({1.0, 0.0}, 3.0, {0.4, 0.0}), BallSpec{},
                                         MaterialParams{}, params);

  REQUIRE(velocity.y == Catch::Approx(0.0).margin(1.0e-12));
  REQUIRE(std::abs(cue.spin.z) > 0.0);
}

TEST_CASE("follow and draw spin opposite each other", "[cue]") {
  Ball follow{};
  follow.on_table = true;
  Ball draw{};
  draw.on_table = true;

  apply_cue_strike(follow, strike({1.0, 0.0}, 3.0, {0.0, 0.4}), BallSpec{}, MaterialParams{},
                   SimulationParams{});
  apply_cue_strike(draw, strike({1.0, 0.0}, 3.0, {0.0, -0.4}), BallSpec{}, MaterialParams{},
                   SimulationParams{});

  REQUIRE(follow.spin.y * draw.spin.y < 0.0);
  REQUIRE(std::abs(follow.spin.y) > 0.0);
}

TEST_CASE("tip offsets beyond half a radius are clamped", "[cue]") {
  Ball cue{};
  cue.on_table = true;
  apply_cue_strike(cue, strike({1.0, 0.0}, 2.0, {4.0, -4.0}), BallSpec{}, MaterialParams{},
                   SimulationParams{});

  // omega = (5 v b) / (2 R^2) with b <= 0.5 R, i.e. 1.25 * v / R.
  const double bound = 1.25 * 2.0 / kRadius;
  REQUIRE(std::abs(cue.spin.z) <= bound + 1.0e-9);
  REQUIRE(std::abs(cue.spin.y) <= bound + 1.0e-9);
}

TEST_CASE("the cue ball gains rolling spin as it slides", "[cue]") {
  World world;
  world.place(kCueBall, {0.4, 0.635});
  world.strike(strike({1.0, 0.0}, 4.0));

  // Sliding first.
  world.step();
  REQUIRE(world.ball(kCueBall).state == BallState::sliding);
  REQUIRE(std::abs(world.ball(kCueBall).spin.y) > 0.0);

  // After ~0.4 s a centred shot is rolling.
  world.advance(280);
  REQUIRE(world.ball(kCueBall).state == BallState::rolling);
}
