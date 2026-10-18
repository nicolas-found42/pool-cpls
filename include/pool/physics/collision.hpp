#pragma once

#include "pool/physics/ball.hpp"
#include "pool/physics/vec.hpp"

namespace pool::physics {

// Contact-point slip velocity: the velocity of the material point of the ball
// touching the cloth. Zero means the ball is rolling without slipping.
// r_contact = (0, 0, -R) for a ball resting on the cloth.
[[nodiscard]] Vec2 slip_velocity(const Ball& ball, double radius) noexcept;

// True once |slip| is negligible: the ball is in pure rolling.
[[nodiscard]] bool is_rolling(const Ball& ball, double radius, double tolerance) noexcept;

// Resolve an equal-mass ball/ball contact given the pre-collision states.
// Applies the normal impulse (restitution) and, when enabled, the tangential
// friction impulse that produces collision-induced throw.
// Returns the normal impulse magnitude applied.
double resolve_ball_ball_collision(Ball& a, Ball& b, double radius, double restitution,
                                   double friction, bool enable_throw, Vec2* out_normal,
                                   double* out_approach);

} // namespace pool::physics
