#pragma once

#include <cstdint>

// Physical constants for a 9-foot American pool table with worsted cloth.
// Sources: WPA equipment specifications and the standard billiards-physics
// literature; see docs/research/04-pool-physics.md for the derivation of every
// coefficient.

namespace pool::physics {

inline constexpr double kGravity = 9.80665; // m/s^2

// Canonical ball constants. These are plain named constants rather than only
// aggregate members because static analysers (cppcheck's legacyUninitvar in
// particular) do not model default member initializers on aggregates, and a
// false positive in the gate is worse than one extra name.
inline constexpr double kBallMass = 0.170;      // kg, regulation 6 oz ball
inline constexpr double kBallRadius = 0.028575; // m, 2.25 in diameter

struct BallSpec {
  double mass = kBallMass;
  double radius = kBallRadius;
};

struct TableSpec {
  // Playing surface between cushion noses, 9 ft table.
  // Source: WPA Recommended Equipment Specifications §5: 100 x 50 in = 2.54 x
  // 1.27 m, tolerance +3.175 mm.
  double length = 2.54; // m
  double width = 1.27;  // m

  // Pocket mouths, measured between opposing cushion noses (§9): corner pocket
  // 4.5-4.625 in, side pocket 5-5.125 in. The side pocket is deliberately
  // ~1.27 cm wider than the corner pocket.
  double corner_mouth = 0.1143; // m, 4.5 in
  double side_mouth = 0.127;    // m, 5.0 in

  double corner_angle = 0.7853981633974483; // 45 degrees, rad
};

// Coulomb coefficients and restitution values.
//
// Friction follows measured values rather than folklore:
//   * sliding friction ball/cloth: Mathavan, Jackson & Parkin, "Application of
//     high-speed imaging to determine the dynamics of billiards", Am. J. Phys.
//     77(9) 2009, report 1.75-2.40 m/s^2 during sliding, i.e. mu_s = 0.178-0.245
//     (mean ~0.21). 0.20 sits inside that band.
//   * rolling resistance: the same paper reports a pure-rolling deceleration of
//     0.124-0.126 m/s^2 (mu_r ~ 0.0128) on snooker cloth; Marlow reports
//     0.011-0.024 (mean ~0.016) for American pool. 0.014 sits between them and
//     satisfies the WPA table-speed requirement (a ball struck from the head
//     spot with a firm stroke must travel at least 4-4.5 table lengths).
struct MaterialParams {
  double sliding_friction = 0.20;    // ball/cloth while slipping
  double rolling_resistance = 0.014; // ball/cloth once rolling
  // Vertical spin deceleration: measured at 5-15 rad/s^2 for a ball spinning on
  // cloth. The model uses d(omega_z)/dt = 5 mu_sp g / (2R), so 0.015 gives
  // ~12.9 rad/s^2, inside the measured band.
  double spinning_friction = 0.015;
  // Ball-to-ball COR is measured at 0.92-0.98 (nominal 0.93-0.94).
  double ball_restitution = 0.95;
  // Ball-to-rail COR is measured at 0.60-0.90, nominal 0.70-0.72. This value is
  // the normal-component restitution and the rail model multiplies it by a
  // speed-dependent factor, so the effective value stays inside the measured
  // band.
  //
  // Known deviation: measured data also reports that a *forward-rolling* ball
  // driven perpendicular into a rail carries only ~50% of its inbound speed once
  // natural roll resumes, because the rail cancels part of its spin. A rigid
  // single-contact rail model with the nose offset reproduces the mechanism but
  // under-predicts the loss (about 0.65 instead of 0.50). Closing that gap needs
  // a contact-time rail model with rubber hysteresis; see
  // docs/research/dev-toolchain-and-physics.md.
  double cushion_restitution = 0.72;
  double cushion_friction = 0.20; // rail/cloth, produces spin transfer
  double ball_friction = 0.06;    // tangential impulse at ball/ball contact
};

struct SimulationParams {
  double fixed_dt = 1.0 / 480.0; // s, one substep
  double max_substeps = 64.0;    // guard against spiral of death
  double rest_speed = 0.004;     // m/s, below this a ball is asleep
  double rest_spin = 0.05;       // rad/s
  double overlap_slop = 1.0e-9;  // m
  bool enable_squirt = true;
  bool enable_swerve = true;
  bool enable_throw = true;
};

constexpr double ball_moment_of_inertia(const BallSpec& spec) noexcept {
  // Solid sphere: I = 2/5 m r^2.
  return 0.4 * spec.mass * spec.radius * spec.radius;
}

} // namespace pool::physics
