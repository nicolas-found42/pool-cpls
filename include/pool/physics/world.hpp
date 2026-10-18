#pragma once

#include "pool/physics/ball.hpp"
#include "pool/physics/constants.hpp"

#include <optional>
#include <vector>

namespace pool::physics {

// Axis-aligned table bounds in metres, origin at the bottom-left corner of the
// playing surface. Cushion noses sit on the bounds.
struct Bounds {
  double min_x{0.0};
  double min_y{0.0};
  double max_x{2.54};
  double max_y{1.27};
};

struct Pocket {
  Vec2 centre{};
  double radius{0.057};
};

enum class ContactKind : std::uint8_t { ball_ball, cushion, pocket };

// One event per collision resolution, recorded only when events are enabled.
struct Contact {
  ContactKind kind{ContactKind::ball_ball};
  std::uint8_t a{0};
  std::uint8_t b{0};
  Vec2 point{};
  Vec2 normal{};
  double approach_speed{0.0};
  double normal_impulse{0.0};
};

// Cue input in the cue ball's own frame: offset is the tip contact point
// relative to centre, expressed in ball radii (|offset| <= 0.5).
struct CueStrike {
  Vec2 direction{};      // unit vector in table space
  double speed{4.0};     // m/s of the resulting cue-ball velocity
  Vec2 tip_offset{};     // in radii; x = side english, y = draw/follow
  double elevation{0.0}; // rad; 0 = level cue
};

// Deterministic simulation world. No RNG, no wall-clock, no I/O: identical
// inputs produce bit-identical outputs for a given build.
class World {
public:
  World();
  explicit World(TableSpec table, MaterialParams material = {});

  void reset();
  void set_params(const SimulationParams& params);
  [[nodiscard]] const SimulationParams& params() const noexcept {
    return params_;
  }
  [[nodiscard]] const TableSpec& table() const noexcept {
    return table_;
  }
  [[nodiscard]] const MaterialParams& material() const noexcept {
    return material_;
  }

  [[nodiscard]] const std::vector<Ball>& balls() const noexcept {
    return balls_;
  }
  [[nodiscard]] std::vector<Ball>& balls() noexcept {
    return balls_;
  }
  [[nodiscard]] const Ball& ball(std::uint8_t id) const;
  [[nodiscard]] const std::vector<Pocket>& pockets() const noexcept {
    return pockets_;
  }
  [[nodiscard]] const Bounds& bounds() const noexcept {
    return bounds_;
  }

  // Place a ball; returns false if the id is out of range or already placed.
  bool place(std::uint8_t id, Vec2 position);
  bool pocket(std::uint8_t id);

  // Apply a cue strike to the cue ball. Returns false when the cue ball is not
  // on the table.
  bool strike(const CueStrike& strike);

  // Advance the simulation by exactly one fixed substep.
  void step();
  // Advance by an integer number of substeps.
  void advance(int substeps);

  // True when every on-table ball has come to rest.
  [[nodiscard]] bool at_rest() const;
  // Substeps remaining until rest, or 0 when at rest. Saturates at a bound.
  [[nodiscard]] int substeps_until_rest(int limit = 200000) const;

  // Total kinetic energy (translational + rotational) in joules.
  [[nodiscard]] double kinetic_energy() const;
  // Sum of linear momenta in kg m/s.
  [[nodiscard]] Vec2 total_momentum() const;

  void set_record_contacts(bool enabled) noexcept {
    record_contacts_ = enabled;
  }
  [[nodiscard]] const std::vector<Contact>& contacts() const noexcept {
    return contacts_;
  }
  void clear_contacts() {
    contacts_.clear();
  }

  // Deterministic 64-bit fingerprint of the dynamic state.
  [[nodiscard]] std::uint64_t fingerprint() const;

private:
  void integrate(double dt);
  // Resolve overlapping pairs. `record_contacts` is true only on the first
  // sequential-impulse pass, so one collision logs one contact.
  void resolve_ball_ball(bool record_contacts);
  void resolve_cushions(double dt);
  void resolve_pockets();
  void update_states();
  void apply_cloth_forces(Ball& entry, double dt) const;
  [[nodiscard]] bool overlapping(const Ball& a, const Ball& b) const;
  void record(Contact contact);

  TableSpec table_{};
  MaterialParams material_{};
  SimulationParams params_{};
  Bounds bounds_{};
  std::vector<Ball> balls_{};
  std::vector<Pocket> pockets_{};
  std::vector<Contact> contacts_{};
  bool record_contacts_{false};
};

} // namespace pool::physics
