# Test targets.
#
# Layout mirrors the libraries: one executable per layer, registered with CTest
# so `ctest` runs everything and `ctest -L physics` runs a slice.
#
# Tests are the contract: the physics core is deterministic and dependency-free,
# so every test below runs headlessly in milliseconds.

# Register a Catch2 test binary. Per-test discovery gives `ctest -R` the test
# case names; without Catch2's extras the whole binary is one CTest entry.
#
# `include(Catch)` is what defines catch_discover_tests; Dependencies.cmake put
# Catch2's extras directory on CMAKE_MODULE_PATH, which upstream documents as
# required when Catch2 comes from FetchContent.
include(Catch OPTIONAL)

function(pool_register_tests target prefix label)
  if(COMMAND catch_discover_tests)
    catch_discover_tests(${target}
      TEST_PREFIX "${prefix}:"
      PROPERTIES LABELS "${label}" TIMEOUT 120)
  else()
    add_test(NAME "${prefix}" COMMAND ${target})
    set_tests_properties("${prefix}" PROPERTIES LABELS "${label}" TIMEOUT 120)
  endif()
endfunction()

add_executable(pool_physics_tests
  physics/vec_test.cpp
  physics/collision_test.cpp
  physics/cue_test.cpp
  physics/world_test.cpp
  physics/table_test.cpp)
target_link_libraries(pool_physics_tests PRIVATE pool::physics Catch2::Catch2WithMain)
pool_register_tests(pool_physics_tests "physics" "unit;physics")

add_executable(pool_game_tests
  game/rack_test.cpp
  game/match_test.cpp)
target_link_libraries(pool_game_tests PRIVATE pool::game Catch2::Catch2WithMain)
pool_register_tests(pool_game_tests "game" "unit;game")

if(POOL_BUILD_APP)
  add_executable(pool_app_tests app/viewport_test.cpp)
  target_link_libraries(pool_app_tests PRIVATE pool::app_lib Catch2::Catch2WithMain)
  pool_register_tests(pool_app_tests "app" "unit;app")
endif()

# Golden replay: drives a fixed break through the simulator and compares the
# state fingerprint. Catches unintended physics changes, not compiler noise.
add_executable(pool_replay_tests replay/replay_test.cpp)
target_link_libraries(pool_replay_tests PRIVATE pool::game Catch2::Catch2WithMain)
target_compile_definitions(pool_replay_tests PRIVATE
  POOL_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/replay")
pool_register_tests(pool_replay_tests "replay" "regression;replay")

# Smoke: run the real binary for a few frames against SDL's dummy driver.
if(POOL_BUILD_APP)
  add_test(NAME "smoke:app"
           COMMAND pool_app --smoke
           WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
  set_tests_properties("smoke:app" PROPERTIES
    LABELS "smoke"
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy")
endif()

# Fuzzing is opt-in: `cmake --preset fuzz` sets POOL_FUZZ=ON, which needs
# clang's -fsanitize=fuzzer and is unavailable with Apple's Xcode clang.
if(POOL_FUZZ)
  add_executable(pool_fuzz_strike fuzz/strike_fuzz.cpp)
  target_link_libraries(pool_fuzz_strike PRIVATE pool::game)
  target_compile_options(pool_fuzz_strike PRIVATE -fsanitize=fuzzer,address
                                                  -fno-omit-frame-pointer)
  target_link_options(pool_fuzz_strike PRIVATE -fsanitize=fuzzer,address)
endif()
