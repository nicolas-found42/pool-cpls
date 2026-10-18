#include "pool/game/rack.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>

// Golden replay: a fixed break must produce the same fingerprint on every run
// and on every platform built with the same compiler settings. The expected
// value lives in tests/replay/expected_fingerprint.txt so a physics change is a
// visible diff.
//
// Regenerate deliberately, then review the diff:
//   POOL_UPDATE_GOLDEN=1 ./build/dev/tests/pool_replay_tests
// A changed fingerprint means the physics changed.

namespace {

constexpr const char* kExpectedPath = POOL_GOLDEN_DIR "/expected_fingerprint.txt";

struct Replay {
  std::uint64_t fingerprint{0};
  std::size_t pocketed{0};
};

Replay replay() {
  pool::physics::World world;
  pool::game::rack(world);

  // A realistic break: the cue ball is re-placed slightly off the centre line
  // (as a breaker does) and driven straight into the rack at 8.5 m/s. This
  // pockets two balls and settles in about 15 s of simulated time.
  const double length = world.table().length;
  const double width = world.table().width;
  world.place(pool::physics::kCueBall, {length * 0.25, width * 0.55});

  pool::physics::CueStrike strike{};
  strike.direction = {1.0, 0.0};
  strike.speed = 8.5;
  world.strike(strike);

  world.advance(world.substeps_until_rest());
  world.advance(4);

  Replay out{};
  out.fingerprint = world.fingerprint();
  for (const pool::physics::Ball& ball : world.balls()) {
    if (!ball.on_table)
      ++out.pocketed;
  }
  return out;
}

// File helpers return an empty/false result instead of relying on the caller to
// check a raw handle, so the paths stay free of unchecked null dereferences.
std::optional<std::uint64_t> read_golden(const char* path) {
  std::FILE* file = std::fopen(path, "r");
  if (file == nullptr) {
    return std::nullopt;
  }
  char buffer[32]{};
  const bool read = std::fgets(buffer, sizeof(buffer), file) != nullptr;
  std::fclose(file);
  if (!read) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(std::strtoull(buffer, nullptr, 16));
}

bool write_golden(const char* path, std::uint64_t fingerprint) {
  std::FILE* out = std::fopen(path, "w");
  if (out == nullptr) {
    return false;
  }
  std::fprintf(out, "%016llx\n", static_cast<unsigned long long>(fingerprint));
  std::fclose(out);
  return true;
}

bool update_requested() {
  const char* value = std::getenv("POOL_UPDATE_GOLDEN");
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

} // namespace

TEST_CASE("a fixed break produces the recorded fingerprint", "[replay]") {
  const Replay actual = replay();

  if (update_requested()) {
    REQUIRE(write_golden(kExpectedPath, actual.fingerprint));
    WARN("golden fingerprint rewritten: " << kExpectedPath);
    return;
  }

  const std::optional<std::uint64_t> expected = read_golden(kExpectedPath);
  INFO("expected file not found or unreadable: "
       << kExpectedPath << " (run with POOL_UPDATE_GOLDEN=1 to create it)");
  REQUIRE(expected.has_value());
  INFO("recorded fingerprint " << std::hex << expected.value_or(0));
  REQUIRE(actual.fingerprint == expected.value_or(0));
}

TEST_CASE("the replay is stable across repeated runs", "[replay]") {
  REQUIRE(replay().fingerprint == replay().fingerprint);
}

TEST_CASE("a legal break pockets a ball and settles", "[replay]") {
  const Replay result = replay();
  REQUIRE(result.pocketed >= 1);

  pool::physics::World world;
  pool::game::rack(world);
  world.place(pool::physics::kCueBall, {world.table().length * 0.25, world.table().width * 0.55});
  pool::physics::CueStrike strike{};
  strike.direction = {1.0, 0.0};
  strike.speed = 8.5;
  world.strike(strike);

  const int steps = world.substeps_until_rest();
  REQUIRE(steps > 0);
  // A break settles well inside 20 s of simulated time at 1/480 s per substep.
  REQUIRE(steps < 20 * 480);
}
