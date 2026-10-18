#include "pool/game/match.hpp"
#include "pool/game/rack.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pool::game;
namespace physics = pool::physics;
using physics::World;

namespace {

// Rule tests drive real shots through the physics, because the engine reads
// `first_contact` and `rail_after_contact` from the world's contact log. A test
// that pockets a ball by hand would exercise a path no real shot takes.
//
// The geometries below were verified against the simulator: exact numbers here
// are the difference between testing the rules and testing an accident.

void take_shot(Match& match, World& world, physics::Vec2 direction, double speed,
               physics::Vec2 tip_offset = {}) {
  match.begin_shot(world);
  physics::CueStrike strike{};
  strike.direction = direction;
  strike.speed = speed;
  strike.tip_offset = tip_offset;
  world.strike(strike);
  world.advance(world.substeps_until_rest());
  REQUIRE(world.at_rest());
}

// A minimal table: cue ball and one object ball on the centre line.
World two_ball_world() {
  World world;
  world.place(physics::kCueBall, {0.60, 0.635});
  world.place(1, {1.00, 0.635});
  return world;
}

// A legal break that opens the table and pockets nothing, so the turn passes.
void take_legal_break(Match& match, World& world) {
  take_shot(match, world, {1.0, 0.0}, 3.0);
  const ShotResult result = match.adjudicate(world);
  REQUIRE_FALSE(result.foul);
  REQUIRE(match.phase() == Phase::open_table);
}

// A cut-free pot into the middle pocket of the bottom rail, played with draw so
// the cue ball checks up instead of following the object ball into the pocket.
// A level strike on the same line scratches, which is correct but not what this
// fixture wants.
World pot_setup(int object_ball) {
  World world;
  world.place(physics::kCueBall, {1.27, 1.05});
  world.place(static_cast<std::uint8_t>(object_ball), {1.27, 0.45});
  return world;
}

constexpr physics::Vec2 kDownTheTable{0.0, -1.0};
constexpr physics::Vec2 kDrawOnly{0.0, -0.4};

} // namespace

TEST_CASE("a new match starts on the break with an open table", "[match]") {
  Match match;
  REQUIRE(match.phase() == Phase::break_shot);
  REQUIRE(match.current_player() == Player::one);
  REQUIRE(match.group_of_player(Player::one) == Group::unassigned);
  REQUIRE(match.group_of_player(Player::two) == Group::unassigned);
  REQUIRE(match.games_won(Player::one) == 0);
}

TEST_CASE("groups map from ball numbers", "[match]") {
  REQUIRE(group_of(0) == Group::unassigned);
  REQUIRE(group_of(1) == Group::solids);
  REQUIRE(group_of(7) == Group::solids);
  REQUIRE(group_of(8) == Group::unassigned);
  REQUIRE(group_of(9) == Group::stripes);
  REQUIRE(group_of(15) == Group::stripes);
}

TEST_CASE("a legal break opens the table and passes the turn when nothing drops", "[match]") {
  World world = two_ball_world();
  Match match;

  take_shot(match, world, {1.0, 0.0}, 3.0);
  const ShotFacts facts = match.facts_for(world);
  REQUIRE(facts.first_contact == 1);
  REQUIRE(facts.rail_after_contact);

  const ShotResult result = match.adjudicate(world);
  REQUIRE_FALSE(result.foul);
  REQUIRE(result.facts.pocketed.empty());
  REQUIRE(match.phase() == Phase::open_table);
  REQUIRE(match.current_player() == Player::two);
}

TEST_CASE("a break that drives too few balls to a rail is a foul", "[match]") {
  World world = two_ball_world();
  Match match;

  // Soft enough that the object ball never reaches a cushion and nothing drops.
  take_shot(match, world, {1.0, 0.0}, 0.6);

  const ShotFacts facts = match.facts_for(world);
  REQUIRE(facts.first_contact == 1);
  REQUIRE(facts.object_balls_to_rail == 0);
  REQUIRE(facts.object_balls_at_shot_start == 1);

  const ShotResult result = match.adjudicate(world);
  REQUIRE(result.foul);
  REQUIRE(match.current_player() == Player::two);
}

TEST_CASE("a shot that touches nothing is a foul", "[match]") {
  World world;
  world.place(physics::kCueBall, {0.60, 0.635});
  world.place(1, {2.40, 1.20}); // nowhere near the aim line

  Match match;
  take_shot(match, world, {-1.0, 0.0}, 2.0);

  const ShotResult result = match.adjudicate(world);
  REQUIRE(result.foul);
  REQUIRE(result.facts.first_contact == 0xFF);
  REQUIRE(result.reason != nullptr);
  REQUIRE(match.current_player() == Player::two);
}

TEST_CASE("scratching is a foul and passes the turn", "[match]") {
  World world;
  // Aim the cue ball straight into the bottom-left corner pocket.
  world.place(physics::kCueBall, {0.25, 0.25});
  world.place(1, {1.90, 1.00});

  Match match;
  take_shot(match, world, {-1.0, -1.0}, 3.0);

  const ShotResult result = match.adjudicate(world);
  REQUIRE(result.facts.cue_ball_pocketed);
  REQUIRE(result.foul);
  REQUIRE(match.current_player() == Player::two);
}

TEST_CASE("an open-table pot assigns groups to the shooter", "[match]") {
  World break_world = two_ball_world();
  Match match;
  take_legal_break(match, break_world);

  const Player shooter = match.current_player();
  const Player other = shooter == Player::one ? Player::two : Player::one;

  World pot = pot_setup(3);
  take_shot(match, pot, kDownTheTable, 1.6, kDrawOnly);

  const ShotResult result = match.adjudicate(pot);
  REQUIRE(result.facts.pocketed.size() == 1);
  REQUIRE(result.facts.pocketed.front() == 3);
  REQUIRE_FALSE(result.foul);
  REQUIRE(result.group_assigned);
  REQUIRE(match.phase() == Phase::assigned);
  REQUIRE(match.group_of_player(shooter) == Group::solids);
  REQUIRE(match.group_of_player(other) == Group::stripes);
  // Potted their own group, so the shooter stays at the table.
  REQUIRE(match.current_player() == shooter);
}

TEST_CASE("pocketing the eight on a legal break spots it instead of ending the rack", "[match]") {
  // WPA Rules of Play 4.3(e): "Pocketing the 8-ball on a legal break shot is not
  // a foul." The breaker spots it and keeps playing; the rack does not end.
  World world = pot_setup(physics::kEightBall);
  Match match;
  take_shot(match, world, kDownTheTable, 1.6, kDrawOnly);

  const ShotResult result = match.adjudicate(world);
  REQUIRE(result.facts.eight_ball_pocketed);
  REQUIRE_FALSE(result.facts.cue_ball_pocketed);
  REQUIRE_FALSE(result.foul);
  REQUIRE(result.eight_respotted);
  REQUIRE_FALSE(result.has_winner);
  REQUIRE(match.phase() == Phase::open_table);
  REQUIRE(match.games_won(Player::one) == 0);
  REQUIRE(match.games_won(Player::two) == 0);

  // The eight is back on the table, near the foot spot.
  const physics::Ball& eight = world.ball(physics::kEightBall);
  REQUIRE(eight.on_table);
  REQUIRE(eight.position.x >= world.table().length * 0.75 - 1.0e-9);
  REQUIRE(eight.position.y == Catch::Approx(world.table().width * 0.5));
}

TEST_CASE("a miss passes the turn after the table is open", "[match]") {
  World break_world = two_ball_world();
  Match match;
  take_legal_break(match, break_world);
  REQUIRE(match.current_player() == Player::two);

  World miss;
  miss.place(physics::kCueBall, {0.60, 0.30});
  miss.place(2, {1.60, 0.30});
  take_shot(match, miss, {1.0, 0.0}, 2.0);

  const ShotResult result = match.adjudicate(miss);
  REQUIRE_FALSE(result.foul);
  REQUIRE(match.current_player() == Player::one);
}

TEST_CASE("reset returns the match to the break", "[match]") {
  World world = two_ball_world();
  Match match;
  take_legal_break(match, world);

  match.reset();
  REQUIRE(match.phase() == Phase::break_shot);
  REQUIRE(match.current_player() == Player::one);
  REQUIRE(match.group_of_player(Player::one) == Group::unassigned);
}
