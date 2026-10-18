#include "pool/game/match.hpp"

#include <algorithm>

namespace pool::game {
namespace {

constexpr std::uint8_t kNoBall = 0xFF;

Player opponent(Player player) noexcept {
  return player == Player::one ? Player::two : Player::one;
}

} // namespace

Group Match::group_of_player(Player player) const noexcept {
  return player == Player::one ? player_one_group_ : player_two_group_;
}

int Match::games_won(Player player) const noexcept {
  return games_won_[static_cast<int>(player)];
}

void Match::begin_shot(physics::World& world) {
  before_ = snapshot(world);
  has_snapshot_ = true;
  world.clear_contacts();
  world.set_record_contacts(true);
}

ShotFacts Match::facts_for(const physics::World& world) const {
  ShotFacts facts{};
  facts.pocketed = pocketed_between(before_, world);
  facts.cue_ball_pocketed = world.ball(physics::kCueBall).state == physics::BallState::pocketed;
  facts.eight_ball_pocketed = world.ball(physics::kEightBall).state == physics::BallState::pocketed;

  bool railed[physics::kBallCount]{};
  for (const physics::Contact& contact : world.contacts()) {
    if (contact.kind == physics::ContactKind::ball_ball) {
      if (facts.first_contact == kNoBall) {
        if (contact.a == physics::kCueBall) {
          facts.first_contact = contact.b;
        } else if (contact.b == physics::kCueBall) {
          facts.first_contact = contact.a;
        }
      }
    } else if (contact.kind == physics::ContactKind::cushion) {
      if (facts.first_contact != kNoBall) {
        // A rail struck by any ball after the cue ball's first contact counts.
        facts.rail_after_contact = true;
      }
      if (contact.a < physics::kBallCount) {
        railed[contact.a] = true;
      }
    }
  }

  for (std::uint8_t id = 1; id < physics::kBallCount; ++id) {
    if (railed[id]) {
      ++facts.object_balls_to_rail;
    }
    if (before_.on_table[id]) {
      ++facts.object_balls_at_shot_start;
    }
  }

  return facts;
}

void Match::respot_eight(physics::World& world) {
  const double spot_x = world.table().length * 0.75;
  const double y = world.table().width * 0.5;
  const double diameter = 2.0 * physics::kBallRadius;
  // Walk back toward the foot rail until a free spot is found (WPA 1.5).
  for (int attempt = 0; attempt < 200; ++attempt) {
    const physics::Vec2 candidate{spot_x + static_cast<double>(attempt) * diameter, y};
    if (candidate.x > world.table().length - physics::kBallRadius) {
      break;
    }
    const bool free = std::none_of(
      world.balls().begin(), world.balls().end(), [candidate, diameter](const physics::Ball& b) {
        return b.on_table && physics::length(b.position - candidate) < diameter;
      });
    if (free) {
      world.place(physics::kEightBall, candidate);
      return;
    }
  }
  world.place(physics::kEightBall, {spot_x, y});
}

bool Match::clears_group(const physics::World& world, Group group) {
  if (group == Group::unassigned) {
    return false;
  }
  return std::none_of(
    world.balls().begin(), world.balls().end(),
    [group](const physics::Ball& ball) { return ball.on_table && group_of(ball.id) == group; });
}

void Match::assign_groups(Player shooter, Group pocketed_group) {
  if (pocketed_group == Group::unassigned) {
    return;
  }
  const Group other = pocketed_group == Group::solids ? Group::stripes : Group::solids;
  if (shooter == Player::one) {
    player_one_group_ = pocketed_group;
    player_two_group_ = other;
  } else {
    player_two_group_ = pocketed_group;
    player_one_group_ = other;
  }
  phase_ = Phase::assigned;
}

ShotResult Match::adjudicate(physics::World& world) {
  ShotResult result{};
  result.facts = facts_for(world);
  result.next_player = current_;
  result.winner = current_;

  const bool breaking = phase_ == Phase::break_shot;
  const Group shooter_group = group_of_player(current_);
  const bool group_cleared = clears_group(world, shooter_group);

  // --- legality of the shot itself -----------------------------------------
  const std::uint8_t first = result.facts.first_contact;
  bool legal_contact = false;

  if (breaking) {
    legal_contact = first != kNoBall;
  } else if (phase_ == Phase::open_table) {
    legal_contact = first != kNoBall && first != physics::kEightBall;
  } else if (group_cleared) {
    legal_contact = first == physics::kEightBall;
  } else {
    legal_contact = group_of(first) == shooter_group;
  }

  const bool pocketed_anything = !result.facts.pocketed.empty();
  // WPA/BCA break requirement: pocket a ball, or drive at least four object
  // balls to a cushion. On a table holding fewer than four object balls the
  // requirement scales down to the balls that are there, otherwise a legal
  // break would be impossible by construction.
  const int rail_requirement = std::min(4, result.facts.object_balls_at_shot_start);
  const bool rail_ok =
    pocketed_anything || (breaking ? result.facts.object_balls_to_rail >= rail_requirement
                                   : result.facts.rail_after_contact);

  result.foul = result.facts.cue_ball_pocketed || !legal_contact || !rail_ok;
  if (result.foul) {
    if (result.facts.cue_ball_pocketed) {
      result.reason = "cue ball pocketed";
    } else if (!legal_contact) {
      result.reason = "illegal first contact";
    } else {
      result.reason = "no rail after contact";
    }
  }

  // --- the eight ball decides the game --------------------------------------
  if (result.facts.eight_ball_pocketed) {
    // WPA 4.3(e): on a legal break the eight is spotted, not scored. The ball is
    // returned to the foot spot (or as close to it along the long string as the
    // table allows, per 1.5 Spotting Balls) and the break continues.
    if (breaking && !result.foul) {
      respot_eight(world);
      result.eight_respotted = true;
      phase_ = Phase::open_table;
      result.next_player = current_;
      return result;
    }

    result.has_winner = true;
    const bool legal_eight =
      !result.foul && !breaking && (phase_ == Phase::open_table || group_cleared);
    if (legal_eight) {
      result.winner = current_;
      games_won_[static_cast<int>(current_)] += 1;
    } else {
      result.winner = opponent(current_);
      games_won_[static_cast<int>(result.winner)] += 1;
    }
    phase_ = Phase::finished;
    result.next_player = result.winner;
    return result;
  }

  // --- break ---------------------------------------------------------------
  if (breaking) {
    if (!result.foul) {
      phase_ = Phase::open_table;
      if (result.facts.pocketed.empty()) {
        result.next_player = opponent(current_);
      }
    } else {
      result.next_player = opponent(current_);
    }
    current_ = result.next_player;
    return result;
  }

  // --- open table: the first legally potted object ball assigns groups -------
  if (phase_ == Phase::open_table && !result.foul) {
    for (std::uint8_t id : result.facts.pocketed) {
      const Group group = group_of(id);
      if (group == Group::unassigned) {
        continue;
      }
      assign_groups(current_, group);
      result.group_assigned = true;
      break;
    }
  }

  // --- continuation ---------------------------------------------------------
  const Group shooter_group_now = group_of_player(current_);
  const bool potted_own =
    std::any_of(result.facts.pocketed.begin(), result.facts.pocketed.end(),
                [shooter_group_now](std::uint8_t id) {
                  if (id == physics::kCueBall || id == physics::kEightBall) {
                    return false;
                  }
                  const Group group = group_of(id);
                  if (group == Group::unassigned) {
                    return false;
                  }
                  return shooter_group_now == Group::unassigned || group == shooter_group_now;
                });

  if (result.foul || !potted_own) {
    result.next_player = opponent(current_);
  }
  current_ = result.next_player;
  return result;
}

void Match::reset() {
  phase_ = Phase::break_shot;
  current_ = Player::one;
  player_one_group_ = Group::unassigned;
  player_two_group_ = Group::unassigned;
  before_ = {};
  has_snapshot_ = false;
}

} // namespace pool::game
