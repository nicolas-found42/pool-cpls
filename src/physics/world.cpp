#include "pool/physics/world.hpp"

#include "pool/physics/collision.hpp"
#include "pool/physics/cue.hpp"
#include "pool/physics/table.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace pool::physics {
namespace {

constexpr double kEpsilon = 1.0e-12;

// Slip below this is treated as rolling. It must be far above floating-point
// noise (kEpsilon) but far below any physically meaningful slip: a slip of
// 0.1 mm/s on a 28.6 mm ball is a rotation of 0.0035 rad/s, i.e. nothing.
// Using a single threshold for both the friction branch and the state flag
// matters: a band where the state says "rolling" but the branch says "sliding"
// silently skips rolling resistance and the table never settles.
constexpr double kSlipTolerance = 1.0e-4;

// Impulse needed to bring the contact slip to zero: u decays at (7/2) mu g, so
// the impulse that cancels a slip speed s is 2 m s / 7.
double stopping_impulse(double mass, double slip_speed) noexcept {
  return 2.0 * mass * slip_speed / 7.0;
}

// Torque arm from the centre to the cloth contact point.
Vec3 contact_arm(double radius) noexcept {
  return {0.0, 0.0, -radius};
}

// Path curvature per unit of vertical spin while sliding (rad/s of turn per
// rad/s of spin). Tuned so maximum english bends the path by a few degrees over
// a table length; it is an approximation of the massé/swerve family, not a 3D
// solve.
constexpr double kSwerveGain = 4.0e-4;

// Sequential-impulse passes. A third ball can push a resolved pair back into
// overlap, so the pair loop runs more than once.
constexpr int kContactIterations = 4;

} // namespace

World::World()
  : World(TableSpec{}) {}

World::World(TableSpec table, MaterialParams material)
  : table_(table),
    material_(material) {
  bounds_ = make_bounds(table_, kBallRadius);
  pockets_ = make_pockets(table_);
  balls_.reserve(kBallCount);
  for (std::uint8_t id = 0; id < kBallCount; ++id) {
    Ball entry{};
    entry.id = id;
    entry.on_table = false;
    balls_.push_back(entry);
  }
}

void World::reset() {
  for (std::size_t i = 0; i < balls_.size(); ++i) {
    balls_[i] = Ball{};
    balls_[i].id = static_cast<std::uint8_t>(i);
  }
  contacts_.clear();
}

void World::set_params(const SimulationParams& new_params) {
  params_ = new_params;
}

const Ball& World::ball(std::uint8_t id) const {
  if (id >= balls_.size()) {
    throw std::out_of_range("pool::physics::World::ball: bad id");
  }
  return balls_[id];
}

bool World::place(std::uint8_t id, Vec2 position) {
  if (id >= balls_.size()) {
    return false;
  }
  Ball& target = balls_[id];
  target.position = position;
  target.velocity = {};
  target.spin = {};
  target.orientation = {};
  target.on_table = true;
  target.state = BallState::resting;
  return true;
}

bool World::pocket(std::uint8_t id) {
  if (id >= balls_.size() || !balls_[id].on_table) {
    return false;
  }
  Ball& target = balls_[id];
  target.on_table = false;
  target.velocity = {};
  target.spin = {};
  target.state = BallState::pocketed;
  return true;
}

bool World::strike(const CueStrike& cue_strike) {
  Ball& cue = balls_[kCueBall];
  if (!cue.on_table) {
    return false;
  }
  apply_cue_strike(cue, cue_strike, BallSpec{}, material_, params_);
  return true;
}

void World::step() {
  const double radius = kBallRadius;

  // Substep when a fast ball would travel further than half a radius, which is
  // the tunnelling guard. A break shot at 8 m/s with dt = 1/480 moves ~17 mm,
  // under a radius, so this normally collapses to one iteration.
  double max_speed = 0.0;
  for (const Ball& entry : balls_) {
    if (entry.on_table) {
      max_speed = std::max(max_speed, length(entry.velocity));
    }
  }
  const double travel = max_speed * params_.fixed_dt;
  const double limit = radius * 0.5;
  int substeps = 1;
  if (travel > limit) {
    substeps = static_cast<int>(std::ceil(travel / limit));
    substeps = std::min(substeps, static_cast<int>(params_.max_substeps));
  }

  const double dt = params_.fixed_dt / static_cast<double>(substeps);
  for (int i = 0; i < substeps; ++i) {
    integrate(dt);
    for (int pass = 0; pass < kContactIterations; ++pass) {
      resolve_ball_ball(pass == 0);
    }
    resolve_cushions(dt);
    // A ball pushed out of a cushion can land on another ball.
    resolve_ball_ball(false);
    resolve_pockets();
    update_states();
  }
}

void World::advance(int steps) {
  for (int i = 0; i < steps; ++i) {
    step();
  }
}

void World::integrate(double dt) {
  for (Ball& entry : balls_) {
    if (!entry.on_table) {
      continue;
    }
    apply_cloth_forces(entry, dt);
    entry.position += entry.velocity * dt;
    entry.orientation = integrate_orientation(entry.orientation, entry.spin, dt);
  }
}

void World::apply_cloth_forces(Ball& entry, double dt) const {
  const double mass = kBallMass;
  const double radius = kBallRadius;
  const double inertia = ball_moment_of_inertia(BallSpec{});

  const Vec2 slip = slip_velocity(entry, radius);
  const double slip_speed = length(slip);

  if (slip_speed > kSlipTolerance) {
    // --- sliding ---------------------------------------------------------
    const Vec2 u_hat = slip / slip_speed;
    const double required = stopping_impulse(mass, slip_speed);
    const double available = material_.sliding_friction * mass * kGravity * dt;
    const double impulse = std::min(required, available);

    // Friction impulse on the entry is opposite the slip direction.
    entry.velocity -= u_hat * (impulse / mass);

    const Vec3 arm = contact_arm(radius);
    const Vec3 friction_force = Vec3{-u_hat.x, -u_hat.y, 0.0} * impulse;
    const Vec3 torque = cross(arm, friction_force);
    entry.spin += torque * (1.0 / inertia);

    if (required <= available) {
      // Slip cancelled exactly: the entry is now rolling.
      entry.state = BallState::rolling;
    } else {
      entry.state = BallState::sliding;
    }

    // Massé/swerve: an elevated cue leaves the cue entry with side spin that,
    // while the entry slides, curves its path. Modelled as a small rotation of
    // the velocity vector (a change of direction, not of speed), which keeps
    // kinetic energy monotonically decreasing and keeps the sim stable.
    if (params_.enable_swerve && std::abs(entry.spin.z) > kEpsilon) {
      const double turn = entry.spin.z * kSwerveGain * dt;
      const double c = std::cos(turn);
      const double s = std::sin(turn);
      const Vec2 v = entry.velocity;
      entry.velocity = {v.x * c - v.y * s, v.x * s + v.y * c};
    }
  } else {
    // --- rolling ---------------------------------------------------------
    // Rolling resistance on cloth is a Coulomb force, not viscous drag: the
    // deceleration is a = mu_r g, constant, so the entry stops in finite time
    // (v^2 / 2a of travel). An exponential decay would leave the entry creeping
    // forever and the table would never settle.
    const double decel = material_.rolling_resistance * kGravity * dt;
    const double rolling_speed = length(entry.velocity);
    if (rolling_speed <= decel) {
      entry.velocity = {};
    } else {
      entry.velocity -= entry.velocity * (decel / rolling_speed);
    }

    // Keep the rolling constraint, v = omega x (-R ẑ).
    entry.spin.x = -entry.velocity.y / radius;
    entry.spin.y = entry.velocity.x / radius;

    // Vertical spin decays independently under spinning friction.
    const double spin_decay = 2.5 * material_.spinning_friction * kGravity * dt / radius;
    if (std::abs(entry.spin.z) <= spin_decay) {
      entry.spin.z = 0.0;
    } else {
      entry.spin.z -= std::copysign(spin_decay, entry.spin.z);
    }

    entry.state = BallState::rolling;
  }

  const double speed = length(entry.velocity);
  const double spin = length(entry.spin);
  if (speed < params_.rest_speed && spin < params_.rest_spin) {
    entry.velocity = {};
    entry.spin = {};
    entry.state = BallState::resting;
  }
}

bool World::overlapping(const Ball& a, const Ball& b) const {
  const double min_distance = 2.0 * kBallRadius - params_.overlap_slop;
  return length_squared(b.position - a.position) < min_distance * min_distance;
}

void World::resolve_ball_ball(bool record_contacts) {
  const double radius = kBallRadius;
  const double diameter = 2.0 * radius;

  for (std::size_t i = 0; i < balls_.size(); ++i) {
    Ball& a = balls_[i];
    if (!a.on_table) {
      continue;
    }
    for (std::size_t j = i + 1; j < balls_.size(); ++j) {
      Ball& b = balls_[j];
      if (!b.on_table) {
        continue;
      }
      if (!overlapping(a, b)) {
        continue;
      }

      const Vec2 delta = b.position - a.position;
      const double distance = length(delta);
      if (distance <= kEpsilon) {
        // Perfectly coincident centres: nudge apart along a deterministic axis
        // so the pair cannot be stuck forever.
        a.position.x -= radius * 0.5;
        b.position.x += radius * 0.5;
        continue;
      }

      // Separate first so the post-condition is "no penetration".
      const Vec2 n = delta / distance;
      const double penetration = diameter - distance;
      a.position -= n * (penetration * 0.5);
      b.position += n * (penetration * 0.5);

      Vec2 normal{};
      double approach = 0.0;
      const double impulse = resolve_ball_ball_collision(a, b, radius, material_.ball_restitution,
                                                         material_.ball_friction,
                                                         params_.enable_throw, &normal, &approach);

      if (impulse > 0.0 && record_contacts) {
        Contact contact{};
        contact.kind = ContactKind::ball_ball;
        contact.a = a.id;
        contact.b = b.id;
        contact.point = (a.position + b.position) * 0.5;
        contact.normal = normal;
        contact.approach_speed = approach;
        contact.normal_impulse = impulse;
        record(contact);
      }
    }
  }
}

void World::resolve_cushions(double dt) {
  (void)dt;
  const double radius = kBallRadius;
  const double mass = kBallMass;

  for (Ball& entry : balls_) {
    if (!entry.on_table) {
      continue;
    }

    struct Wall {
      bool hit{false};
      Vec2 normal{}; // points into the table
      double overlap{0.0};
    };
    const Wall walls[4] = {
      {entry.position.x < bounds_.min_x, {1.0, 0.0}, bounds_.min_x - entry.position.x},
      {entry.position.x > bounds_.max_x, {-1.0, 0.0}, entry.position.x - bounds_.max_x},
      {entry.position.y < bounds_.min_y, {0.0, 1.0}, bounds_.min_y - entry.position.y},
      {entry.position.y > bounds_.max_y, {0.0, -1.0}, entry.position.y - bounds_.max_y},
    };

    for (const Wall& wall : walls) {
      if (!wall.hit) {
        continue;
      }

      // Seat the entry against the cushion first.
      entry.position += wall.normal * wall.overlap;

      // --- rail contact ------------------------------------------------------------------
      // The rail is NOT a contact at the ball's midline. The WPA equipment
      // specification puts the cushion nose at 63.5% of the ball DIAMETER above
      // the bed, i.e. 1.27 R above the cloth, while the ball's centre sits at R.
      // The nose therefore strikes ~0.27 R above centre. That offset matters:
      // the normal impulse acquires a torque about the rail-parallel axis, which
      // is precisely the axis of the ball's rolling spin, so the rail cancels
      // part of the ball's forward spin on the way out. Without it a rolling ball
      // rebounds far slower than measured, because its untouched forward spin
      // fights the new direction of travel all the way through the skid.
      constexpr double kNoseHeightFraction = 0.27; // multiples of the ball radius
      const double nose_offset = kNoseHeightFraction * radius;

      const Vec3 normal3{wall.normal.x, wall.normal.y, 0.0};
      const Vec3 arm{-wall.normal.x * radius, -wall.normal.y * radius, nose_offset};
      const Vec3 velocity3{entry.velocity.x, entry.velocity.y, 0.0};
      const Vec3 contact_velocity = velocity3 + cross(entry.spin, arm);

      const double normal_speed = -dot(contact_velocity, normal3); // >0 into the wall
      if (normal_speed <= 0.0) {
        continue;
      }

      // Restitution falls with impact speed: a hard rail hit loses more. The
      // floor keeps the effective coefficient inside the measured 0.60-0.90 band.
      const double speed_factor = std::clamp(1.0 - 0.012 * normal_speed, 0.80, 1.0);
      const double restitution = material_.cushion_restitution * speed_factor;

      const double inertia = ball_moment_of_inertia(BallSpec{});
      // Effective mass along the normal: the offset arm contributes |r x n|^2 / I,
      // and |r x n| is exactly the nose offset.
      const double normal_inverse_mass = 1.0 / mass + (nose_offset * nose_offset) / inertia;
      const double normal_impulse = (1.0 + restitution) * normal_speed / normal_inverse_mass;

      const Vec3 normal_impulse_vector = normal3 * normal_impulse;
      entry.velocity += Vec2{normal_impulse_vector.x, normal_impulse_vector.y} / mass;
      entry.spin += cross(arm, normal_impulse_vector) * (1.0 / inertia);

      // Tangential friction on the rail converts vertical spin into lateral
      // velocity and vice versa, which is what changes the rebound angle.
      const Vec3 slip_vector = contact_velocity - normal3 * dot(contact_velocity, normal3);
      const double slip_speed_t = length(slip_vector);
      if (slip_speed_t > kEpsilon) {
        const Vec3 slip_direction = slip_vector / slip_speed_t;
        // |r x t|^2 = R^2 + nose_offset^2, so the tangential effective mass is
        // 1/m + (R^2 + a^2)/I.
        const double tangent_inverse_mass =
          1.0 / mass + (radius * radius + nose_offset * nose_offset) / inertia;
        const double stopping = slip_speed_t / tangent_inverse_mass;
        const double max_friction = material_.cushion_friction * normal_impulse;
        const double tangential_impulse = std::min(stopping, max_friction);

        const Vec3 friction_vector = slip_direction * -tangential_impulse;
        entry.velocity += Vec2{friction_vector.x, friction_vector.y} / mass;
        entry.spin += cross(arm, friction_vector) * (1.0 / inertia);
      }

      Contact contact{};
      contact.kind = ContactKind::cushion;
      contact.a = entry.id;
      contact.point = entry.position - wall.normal * radius;
      contact.normal = wall.normal;
      contact.approach_speed = normal_speed;
      contact.normal_impulse = normal_impulse;
      record(contact);
    }
  }
}

void World::resolve_pockets() {
  for (Ball& entry : balls_) {
    if (!entry.on_table) {
      continue;
    }
    for (const Pocket& hole : pockets_) {
      const double capture = hole.radius;
      if (length_squared(entry.position - hole.centre) <= capture * capture) {
        Contact contact{};
        contact.kind = ContactKind::pocket;
        contact.a = entry.id;
        contact.point = hole.centre;
        contact.approach_speed = length(entry.velocity);
        record(contact);

        entry.on_table = false;
        entry.velocity = {};
        entry.spin = {};
        entry.state = BallState::pocketed;
        break;
      }
    }
  }
}

void World::update_states() {
  const double radius = kBallRadius;
  for (Ball& entry : balls_) {
    if (!entry.on_table) {
      continue;
    }
    if (length(entry.velocity) < params_.rest_speed && length(entry.spin) < params_.rest_spin) {
      entry.velocity = {};
      entry.spin = {};
      entry.state = BallState::resting;
    } else if (is_rolling(entry, radius, 1.0e-6)) {
      entry.state = BallState::rolling;
    } else {
      entry.state = BallState::sliding;
    }
  }
}

bool World::at_rest() const {
  return std::none_of(balls_.begin(), balls_.end(), [](const Ball& entry) {
    return entry.on_table && entry.state != BallState::resting;
  });
}

int World::substeps_until_rest(int limit) const {
  if (at_rest()) {
    return 0;
  }
  World copy = *this;
  copy.record_contacts_ = false;
  for (int i = 0; i < limit; ++i) {
    copy.step();
    if (copy.at_rest()) {
      return i + 1;
    }
  }
  return limit;
}

double World::kinetic_energy() const {
  const double inertia = ball_moment_of_inertia(BallSpec{});
  double total = 0.0;
  for (const Ball& entry : balls_) {
    if (!entry.on_table) {
      continue;
    }
    total += 0.5 * kBallMass * length_squared(entry.velocity);
    total += 0.5 * inertia * length_squared(entry.spin);
  }
  return total;
}

Vec2 World::total_momentum() const {
  const double mass = kBallMass;
  Vec2 total{};
  for (const Ball& entry : balls_) {
    if (!entry.on_table) {
      continue;
    }
    total += entry.velocity * mass;
  }
  return total;
}

std::uint64_t World::fingerprint() const {
  std::uint64_t hash = 1469598103934665603ULL; // FNV-1a offset basis
  const auto mix = [&hash](const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
      hash ^= bytes[i];
      hash *= 1099511628211ULL;
    }
  };

  for (const Ball& entry : balls_) {
    const std::uint64_t vx = std::bit_cast<std::uint64_t>(entry.position.x);
    const std::uint64_t vy = std::bit_cast<std::uint64_t>(entry.position.y);
    const std::uint64_t lx = std::bit_cast<std::uint64_t>(entry.velocity.x);
    const std::uint64_t ly = std::bit_cast<std::uint64_t>(entry.velocity.y);
    const std::uint64_t sx = std::bit_cast<std::uint64_t>(entry.spin.x);
    const std::uint64_t sy = std::bit_cast<std::uint64_t>(entry.spin.y);
    const std::uint64_t sz = std::bit_cast<std::uint64_t>(entry.spin.z);
    const unsigned char alive = entry.on_table ? 1U : 0U;
    mix(&entry.id, sizeof(entry.id));
    mix(&alive, sizeof(alive));
    mix(&vx, sizeof(vx));
    mix(&vy, sizeof(vy));
    mix(&lx, sizeof(lx));
    mix(&ly, sizeof(ly));
    mix(&sx, sizeof(sx));
    mix(&sy, sizeof(sy));
    mix(&sz, sizeof(sz));
  }
  return hash;
}

std::uint64_t World::state_digest(double quantum) const {
  std::uint64_t hash = 1469598103934665603ULL;
  const auto mix = [&hash](std::uint64_t value) {
    for (unsigned byte = 0; byte < 8; ++byte) {
      hash ^= (value >> (byte * 8U)) & 0xFFULL;
      hash *= 1099511628211ULL;
    }
  };
  const auto quantise = [quantum](double value) {
    // Round to the nearest quantum step, then keep the two's-complement bit
    // pattern as an unsigned value so the mixing below stays unsigned.
    const auto steps = static_cast<std::int64_t>(std::llround(value / quantum));
    return std::bit_cast<std::uint64_t>(steps);
  };

  for (const Ball& entry : balls_) {
    mix(entry.id);
    mix(entry.on_table ? 1ULL : 0ULL);
    mix(quantise(entry.position.x));
    mix(quantise(entry.position.y));
    mix(quantise(entry.velocity.x));
    mix(quantise(entry.velocity.y));
    mix(quantise(entry.spin.x));
    mix(quantise(entry.spin.y));
    mix(quantise(entry.spin.z));
  }
  return hash;
}

void World::record(Contact contact) {
  if (record_contacts_) {
    contacts_.push_back(contact);
  }
}

} // namespace pool::physics
