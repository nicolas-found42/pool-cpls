#pragma once

#include "pool/physics/world.hpp"

namespace pool::physics {

// Convert a cue strike into the cue ball's initial linear and angular velocity.
// Models:
//   * follow/draw from the vertical tip offset (y),
//   * side english from the horizontal tip offset (x),
//   * squirt (cue-ball deflection) proportional to side english,
//   * swerve only when the cue is elevated.
// Returns the velocity assigned to the cue ball.
Vec2 apply_cue_strike(Ball& cue_ball, const CueStrike& strike, const BallSpec& spec,
                      const MaterialParams& material, const SimulationParams& params);

} // namespace pool::physics
