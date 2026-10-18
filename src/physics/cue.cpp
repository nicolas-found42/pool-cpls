#include "pool/physics/cue.hpp"

#include <algorithm>
#include <cmath>

namespace pool::physics {
namespace {

// Squirt (cue-ball deflection) grows roughly linearly with side english.
// 0.06 rad per unit radius offset is ~1.7 degrees at half-radius, inside the
// published 2-6 degree range for a normal cue at maximum offset.
constexpr double kSquirtPerRadiusOffset = 0.060;

// A tip cannot grip beyond half a ball radius without miscueing.
constexpr double kMaxTipOffset = 0.5;

// Energy actually delivered to the ball; the rest is lost at the tip.
constexpr double kCueEfficiency = 0.98;

// omega = (5/(2R)) * v for an impulse applied at the sphere surface.
double spin_scale(const BallSpec& spec) noexcept {
  return 2.5 / spec.radius;
}

} // namespace

Vec2 apply_cue_strike(Ball& cue_ball, const CueStrike& strike, const BallSpec& spec,
                      const MaterialParams& material, const SimulationParams& params) {
  (void)material;

  const Vec2 aim = normalized(strike.direction);
  const double side = std::clamp(strike.tip_offset.x, -kMaxTipOffset, kMaxTipOffset);
  const double vertical = std::clamp(strike.tip_offset.y, -kMaxTipOffset, kMaxTipOffset);
  const double speed = std::max(0.0, strike.speed) * kCueEfficiency;

  // Squirt deflects the cue ball away from the side the tip strikes.
  double deflection = 0.0;
  if (params.enable_squirt) {
    deflection = -side * kSquirtPerRadiusOffset;
  }
  const Vec2 velocity =
    normalized(aim * std::cos(deflection) + perpendicular(aim) * std::sin(deflection)) * speed;

  // Draw and follow act along the in-plane axis perpendicular to the aim, so the
  // model is correct at any shot angle. Side english is pure vertical spin.
  const Vec2 draw_axis = perpendicular(aim);
  const double scale = spin_scale(spec) * speed;

  cue_ball.velocity = velocity;
  cue_ball.spin.x = draw_axis.x * vertical * scale;
  cue_ball.spin.y = draw_axis.y * vertical * scale;
  cue_ball.spin.z = -side * scale;
  cue_ball.state = BallState::sliding;

  // Swerve: an elevated cue with side english bends the path. A lateral spin
  // component is enough to make a massé visibly curve once the cloth force
  // couples spin back into velocity in `World::apply_cloth_forces`.
  if (params.enable_swerve && strike.elevation > 0.0) {
    const double curve = std::sin(strike.elevation) * side * speed * 0.08;
    cue_ball.spin.x += -draw_axis.y * 0.0 + (-aim.y) * curve;
    cue_ball.spin.y += (aim.x) * curve;
  }

  return velocity;
}

} // namespace pool::physics
