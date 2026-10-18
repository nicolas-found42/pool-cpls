#pragma once

#include "pool/game/rack.hpp"
#include "pool/physics/world.hpp"

#include <cstdint>
#include <vector>

namespace pool::game {

enum class Group : std::uint8_t { unassigned, solids, stripes };
enum class Player : std::uint8_t { one = 0, two = 1 };
enum class Phase : std::uint8_t { break_shot, open_table, assigned, finished };

// Facts a shot is judged on, derived from the world's contact log.
struct ShotFacts {
  std::vector<std::uint8_t> pocketed;
  std::uint8_t first_contact{0xFF}; // first object ball the cue ball touched
  bool rail_after_contact{false};   // a cushion contact followed first_contact
  bool cue_ball_pocketed{false};
  bool eight_ball_pocketed{false};
  // Distinct object balls that reached a cushion during the shot, and how many
  // were on the table when it started. The break has its own rail requirement,
  // so the counts cannot be folded into the rail_after_contact flag.
  int object_balls_to_rail{0};
  int object_balls_at_shot_start{0};
};

struct ShotResult {
  bool foul{false};
  bool has_winner{false};
  Player winner{Player::one};
  bool group_assigned{false};
  // WPA Rules of Play 4.3(e): pocketing the 8-ball on a legal break is not a
  // foul and is not a loss; the 8 is spotted and play continues.
  bool eight_respotted{false};
  Player next_player{Player::one};
  ShotFacts facts{};
  const char* reason{nullptr}; // short, human-readable adjudication note
};

inline Group group_of(std::uint8_t id) noexcept {
  if (id >= 1 && id <= 7)
    return Group::solids;
  if (id >= 9 && id <= 15)
    return Group::stripes;
  return Group::unassigned;
}

// Eight-ball rules engine. Holds only rule state; the physics world owns ball
// positions, so a match is reproducible from (rack, ordered shot list).
class Match {
public:
  Match() = default;

  [[nodiscard]] Phase phase() const noexcept {
    return phase_;
  }
  [[nodiscard]] Player current_player() const noexcept {
    return current_;
  }
  [[nodiscard]] Group group_of_player(Player player) const noexcept;
  [[nodiscard]] bool is_open_table() const noexcept {
    return phase_ == Phase::open_table;
  }
  [[nodiscard]] int games_won(Player player) const noexcept;

  // Record the table state before a shot and start collecting contacts.
  void begin_shot(physics::World& world);

  // Judge the shot that just finished in `world`. Does not mutate physics state.
  ShotResult adjudicate(physics::World& world);

  void reset();

  // Derive the shot facts from the world's recorded contacts and table diff.
  [[nodiscard]] ShotFacts facts_for(const physics::World& world) const;

private:
  void assign_groups(Player shooter, Group pocketed_group);
  static void respot_eight(physics::World& world);
  [[nodiscard]] static bool clears_group(const physics::World& world, Group group);

  Phase phase_{Phase::break_shot};
  Player current_{Player::one};
  Group player_one_group_{Group::unassigned};
  Group player_two_group_{Group::unassigned};
  int games_won_[2]{0, 0};
  TableSnapshot before_{};
  bool has_snapshot_{false};
};

} // namespace pool::game
