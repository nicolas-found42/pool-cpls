#include "pool/physics/vec.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using pool::physics::Vec2;
using pool::physics::Vec3;

TEST_CASE("vec2 arithmetic is componentwise", "[vec]") {
  const Vec2 a{1.0, 2.0};
  const Vec2 b{3.0, 5.0};

  REQUIRE((a + b).x == 4.0);
  REQUIRE((a + b).y == 7.0);
  REQUIRE((b - a).x == 2.0);
  REQUIRE((a * 2.0).x == 2.0);
  REQUIRE((2.0 * a).y == 4.0);
  REQUIRE((-a).x == -1.0);
  REQUIRE((a / 2.0).y == 1.0);
}

TEST_CASE("dot and cross match their definitions", "[vec]") {
  const Vec2 a{3.0, 4.0};
  const Vec2 b{-4.0, 3.0};

  REQUIRE(pool::physics::dot(a, b) == 0.0);
  REQUIRE(pool::physics::cross(a, b) == Catch::Approx(25.0));
  REQUIRE(pool::physics::length(a) == Catch::Approx(5.0));
}

TEST_CASE("perpendicular rotates by 90 degrees and preserves length", "[vec]") {
  const Vec2 a{3.0, 4.0};
  const Vec2 p = pool::physics::perpendicular(a);

  REQUIRE(p.x == -4.0);
  REQUIRE(p.y == 3.0);
  REQUIRE(pool::physics::dot(a, p) == 0.0);
  REQUIRE(pool::physics::length(p) == Catch::Approx(pool::physics::length(a)));
}

TEST_CASE("normalized returns a unit vector and handles zero", "[vec]") {
  const Vec2 unit = pool::physics::normalized(Vec2{3.0, 4.0});
  REQUIRE(pool::physics::length(unit) == Catch::Approx(1.0));

  const Vec2 zero = pool::physics::normalized(Vec2{});
  REQUIRE(zero.x == 0.0);
  REQUIRE(zero.y == 0.0);
}

TEST_CASE("vec3 cross product follows the right-hand rule", "[vec]") {
  const Vec3 x{1.0, 0.0, 0.0};
  const Vec3 y{0.0, 1.0, 0.0};
  const Vec3 z = pool::physics::cross(x, y);

  REQUIRE(z.x == 0.0);
  REQUIRE(z.y == 0.0);
  REQUIRE(z.z == 1.0);
}

TEST_CASE("orientation integration keeps the quaternion normalized", "[vec]") {
  pool::physics::Quat q{};
  const Vec3 omega{0.0, 0.0, 12.0};

  for (int i = 0; i < 10'000; ++i) {
    q = pool::physics::integrate_orientation(q, omega, 1.0 / 480.0);
  }

  const double norm = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  REQUIRE(norm == Catch::Approx(1.0).margin(1.0e-12));
}
