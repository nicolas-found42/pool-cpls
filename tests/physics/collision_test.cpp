#include "pool/physics/collision.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pool::physics;

namespace {

Ball make_ball(std::uint8_t id, Vec2 position, Vec2 velocity, Vec3 spin = {}) {
  Ball ball{};
  ball.id = id;
  ball.on_table = true;
  ball.position = position;
  ball.velocity = velocity;
  ball.spin = spin;
  return ball;
}

} // namespace

TEST_CASE("slip velocity is zero for a rolling ball", "[collision]") {
  constexpr double kRadius = 0.028575;
  Ball ball = make_ball(1, {0.0, 0.0}, {1.0, 0.0});
  // Rolling along +x requires omega_y = v_x / R.
  ball.spin = {0.0, 1.0 / kRadius, 0.0};

  const Vec2 slip = slip_velocity(ball, kRadius);
  REQUIRE(slip.x == Catch::Approx(0.0).margin(1.0e-12));
  REQUIRE(slip.y == Catch::Approx(0.0).margin(1.0e-12));
  REQUIRE(is_rolling(ball, kRadius, 1.0e-9));
}

TEST_CASE("slip velocity is opposite for a ball sliding without spin", "[collision]") {
  constexpr double kRadius = 0.028575;
  Ball ball = make_ball(1, {0.0, 0.0}, {2.0, 0.0});

  const Vec2 slip = slip_velocity(ball, kRadius);
  REQUIRE(slip.x == Catch::Approx(2.0));
  REQUIRE_FALSE(is_rolling(ball, kRadius, 1.0e-9));
}

TEST_CASE("head-on collision exchanges velocity between equal masses", "[collision]") {
  constexpr double kRadius = 0.028575;
  Ball a = make_ball(1, {0.0, 0.0}, {2.0, 0.0});
  Ball b = make_ball(2, {2.0 * kRadius, 0.0}, {0.0, 0.0});

  Vec2 normal{};
  double approach = 0.0;
  const double impulse =
    resolve_ball_ball_collision(a, b, kRadius, 0.95, 0.0, false, &normal, &approach);

  REQUIRE(impulse > 0.0);
  REQUIRE(approach == Catch::Approx(2.0));
  REQUIRE(a.velocity.x == Catch::Approx(0.05));
  REQUIRE(b.velocity.x == Catch::Approx(1.95));
  REQUIRE(a.velocity.y == Catch::Approx(0.0).margin(1.0e-12));
  REQUIRE(b.velocity.y == Catch::Approx(0.0).margin(1.0e-12));
}

TEST_CASE("momentum is conserved by the normal impulse", "[collision]") {
  constexpr double kRadius = 0.028575;
  constexpr double kMass = 0.17;
  Ball a = make_ball(1, {0.0, 0.0}, {1.5, 0.4});
  Ball b = make_ball(2, {kRadius * 1.9, 0.0}, {-0.2, 0.0});

  const Vec2 before = a.velocity * kMass + b.velocity * kMass;
  resolve_ball_ball_collision(a, b, kRadius, 0.95, 0.06, true, nullptr, nullptr);
  const Vec2 after = a.velocity * kMass + b.velocity * kMass;

  REQUIRE(after.x == Catch::Approx(before.x).margin(1.0e-12));
  REQUIRE(after.y == Catch::Approx(before.y).margin(1.0e-12));
}

TEST_CASE("a separating pair receives no impulse", "[collision]") {
  constexpr double kRadius = 0.028575;
  Ball a = make_ball(1, {0.0, 0.0}, {-1.0, 0.0});
  Ball b = make_ball(2, {2.0 * kRadius, 0.0}, {1.0, 0.0});

  const double impulse =
    resolve_ball_ball_collision(a, b, kRadius, 0.95, 0.06, true, nullptr, nullptr);
  REQUIRE(impulse == 0.0);
  REQUIRE(a.velocity.x == -1.0);
}

TEST_CASE("tangential friction imparts throw spin to both balls", "[collision]") {
  constexpr double kRadius = 0.028575;
  // Cut shot: b is offset laterally so the contact normal is not along a's path.
  Ball a = make_ball(1, {0.0, 0.0}, {3.0, 0.0});
  Ball b = make_ball(2, {kRadius * 1.6, kRadius * 1.2}, {0.0, 0.0});

  resolve_ball_ball_collision(a, b, kRadius, 0.95, 0.06, true, nullptr, nullptr);

  // Throw rotates both balls about z, in the same sense.
  REQUIRE(std::abs(a.spin.z) > 0.0);
  REQUIRE(std::abs(b.spin.z) > 0.0);
  REQUIRE((a.spin.z > 0.0) == (b.spin.z > 0.0));
}

TEST_CASE("restitution bounds the energy out of a collision", "[collision]") {
  constexpr double kRadius = 0.028575;
  constexpr double kMass = 0.17;
  Ball a = make_ball(1, {0.0, 0.0}, {4.0, 0.0});
  Ball b = make_ball(2, {2.0 * kRadius, 0.0}, {0.0, 0.0});

  const double before =
    0.5 * kMass * length_squared(a.velocity) + 0.5 * kMass * length_squared(b.velocity);
  resolve_ball_ball_collision(a, b, kRadius, 0.95, 0.06, true, nullptr, nullptr);
  const double after =
    0.5 * kMass * length_squared(a.velocity) + 0.5 * kMass * length_squared(b.velocity);

  REQUIRE(after <= before + 1.0e-12);
}
