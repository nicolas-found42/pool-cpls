#pragma once

// Minimal vector/quaternion types. The physics core deliberately has no
// third-party math dependency: everything here is trivially copyable, constexpr
// friendly and deterministic (no fast-math, no FMA fusion).

#include <cmath>
#include <cstddef>

namespace pool::physics {

struct Vec2 {
  double x{0.0};
  double y{0.0};
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept {
  return {a.x + b.x, a.y + b.y};
}
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept {
  return {a.x - b.x, a.y - b.y};
}
constexpr Vec2 operator*(Vec2 a, double s) noexcept {
  return {a.x * s, a.y * s};
}
constexpr Vec2 operator*(double s, Vec2 a) noexcept {
  return {a.x * s, a.y * s};
}
constexpr Vec2 operator/(Vec2 a, double s) noexcept {
  return {a.x / s, a.y / s};
}
constexpr Vec2 operator-(Vec2 a) noexcept {
  return {-a.x, -a.y};
}
constexpr Vec2& operator+=(Vec2& a, Vec2 b) noexcept {
  a = a + b;
  return a;
}
constexpr Vec2& operator-=(Vec2& a, Vec2 b) noexcept {
  a = a - b;
  return a;
}
constexpr Vec2& operator*=(Vec2& a, double s) noexcept {
  a = a * s;
  return a;
}

constexpr double dot(Vec2 a, Vec2 b) noexcept {
  return a.x * b.x + a.y * b.y;
}
constexpr double cross(Vec2 a, Vec2 b) noexcept {
  return a.x * b.y - a.y * b.x;
}
constexpr double length_squared(Vec2 a) noexcept {
  return dot(a, a);
}
inline double length(Vec2 a) noexcept {
  return std::sqrt(length_squared(a));
}

constexpr Vec2 perpendicular(Vec2 a) noexcept {
  return {-a.y, a.x};
}

inline Vec2 normalized(Vec2 a) noexcept {
  const double len = length(a);
  return len > 0.0 ? a / len : Vec2{};
}

struct Vec3 {
  double x{0.0};
  double y{0.0};
  double z{0.0};
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
constexpr Vec3 operator*(Vec3 a, double s) noexcept {
  return {a.x * s, a.y * s, a.z * s};
}
constexpr Vec3 operator/(Vec3 a, double s) noexcept {
  return {a.x / s, a.y / s, a.z / s};
}
constexpr Vec3& operator+=(Vec3& a, Vec3 b) noexcept {
  a = a + b;
  return a;
}
constexpr Vec3& operator*=(Vec3& a, double s) noexcept {
  a = a * s;
  return a;
}

constexpr double dot(Vec3 a, Vec3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
constexpr double length_squared(Vec3 a) noexcept {
  return dot(a, a);
}
inline double length(Vec3 a) noexcept {
  return std::sqrt(length_squared(a));
}

constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Unit quaternion, used only to roll the ball graphic. Physics never depends on
// orientation, so drift here cannot corrupt the simulation.
struct Quat {
  double w{1.0};
  double x{0.0};
  double y{0.0};
  double z{0.0};
};

constexpr Quat operator*(Quat a, Quat b) noexcept {
  return {
    a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
    a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
    a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
  };
}

inline Quat normalized(Quat q) noexcept {
  const double len = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  if (len <= 0.0)
    return {};
  return {q.w / len, q.x / len, q.y / len, q.z / len};
}

// Advance an orientation by angular velocity omega over dt.
inline Quat integrate_orientation(Quat q, Vec3 omega, double dt) noexcept {
  const Quat spin{0.0, omega.x, omega.y, omega.z};
  const Quat dq = spin * q;
  Quat out{q.w + 0.5 * dq.w * dt, q.x + 0.5 * dq.x * dt, q.y + 0.5 * dq.y * dt,
           q.z + 0.5 * dq.z * dt};
  return normalized(out);
}

} // namespace pool::physics
