#include "pool/physics/collision.hpp"

#include <algorithm>
#include <cmath>

namespace pool::physics {
namespace {

constexpr double kEpsilon = 1.0e-12;

// Tangential effective inverse mass at a sphere/sphere contact. Translation
// contributes 2/m and rotation 2R^2/I; with I = 2/5 m R^2 that is 5/m, so the
// contact behaves as a body of mass m/7 along the tangent.
constexpr double kTangentialMassDivisor = 7.0;

} // namespace

Vec2 slip_velocity(const Ball& ball, double radius) noexcept {
  // Contact point sits at (0, 0, -R). omega x r = (-R*wy, R*wx, 0).
  return {ball.velocity.x - radius * ball.spin.y, ball.velocity.y + radius * ball.spin.x};
}

bool is_rolling(const Ball& ball, double radius, double tolerance) noexcept {
  return length_squared(slip_velocity(ball, radius)) <= tolerance * tolerance;
}

double resolve_ball_ball_collision(Ball& a, Ball& b, double radius, double restitution,
                                   double friction, bool enable_throw, Vec2* out_normal,
                                   double* out_approach) {
  const Vec2 delta = b.position - a.position;
  const double distance = length(delta);
  if (distance <= kEpsilon) {
    return 0.0;
  }

  const Vec2 n = delta / distance; // a -> b
  // Positive when the centres are closing on each other: a's relative velocity
  // has a component along the a -> b normal.
  const double approach = dot(a.velocity - b.velocity, n);

  if (out_normal != nullptr) {
    *out_normal = n;
  }
  if (out_approach != nullptr) {
    *out_approach = approach;
  }

  if (approach <= 0.0) {
    return 0.0; // already separating
  }

  const double mass = 0.17;
  const double normal_impulse = (1.0 + restitution) * approach * mass * 0.5;
  a.velocity -= n * (normal_impulse / mass);
  b.velocity += n * (normal_impulse / mass);

  if (!enable_throw || friction <= 0.0) {
    return normal_impulse;
  }

  // Relative slip of the two contact points. Only vertical spin couples into
  // the tangential (in-plane) direction: omega_z x (+-R n) has no in-plane
  // component, but it does move each contact point sideways.
  const Vec2 spin_slip_a{-radius * a.spin.z * n.y, radius * a.spin.z * n.x};
  const Vec2 spin_slip_b{radius * b.spin.z * n.y, -radius * b.spin.z * n.x};
  Vec2 slip = (a.velocity + spin_slip_a) - (b.velocity + spin_slip_b);
  slip -= n * dot(slip, n);

  const double slip_speed = length(slip);
  if (slip_speed <= kEpsilon) {
    return normal_impulse;
  }

  const Vec2 t = slip / slip_speed;
  const double stopping_impulse = mass * slip_speed / kTangentialMassDivisor;
  const double tangential_impulse = std::min(stopping_impulse, friction * normal_impulse);

  // Impulse on a opposes the slip; b receives the reaction.
  const Vec2 impulse = t * -tangential_impulse;
  a.velocity += impulse / mass;
  b.velocity -= impulse / mass;

  // tau = r x J with r_a = R n, r_b = -R n. Both torques point along n x t, so
  // both balls acquire vertical spin in the same sense: that is throw.
  const double inertia = 0.4 * mass * radius * radius;
  const double spin_delta = -radius * tangential_impulse * cross(n, t) / inertia;
  a.spin.z += spin_delta;
  b.spin.z += spin_delta;

  return normal_impulse;
}

} // namespace pool::physics
