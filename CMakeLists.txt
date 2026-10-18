cmake_minimum_required(VERSION 3.28)

project(
  pool_cpls
  VERSION 0.1.0
  DESCRIPTION "Top-down 2D pool (billiards) with a realistic physics core"
  HOMEPAGE_URL "https://github.com/nicolas-found42/pool-cpls"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CTest)

# Options must exist before Dependencies.cmake, which gates fetching on them.
option(POOL_BUILD_APP "Build the SDL3 front-end" ON)
option(POOL_BUILD_TESTS "Build the test suite" ${BUILD_TESTING})
option(POOL_BUILD_BENCH "Build micro-benchmarks" OFF)
option(POOL_FUZZ "Build libFuzzer targets (needs clang -fsanitize=fuzzer)" OFF)
option(POOL_SDL_HEADLESS
  "Build a fetched SDL3 with the dummy video driver only (for CI on Linux, which has no X11/Wayland dev libraries)"
  OFF)
option(POOL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(POOL_ENABLE_CCACHE "Use ccache/sccache when available" ON)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Dependencies.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep third-party warnings out of our own -Werror gate.
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
add_library(pool_flags INTERFACE)
add_library(pool::flags ALIAS pool_flags)
pool_apply_warnings(pool_flags)
pool_apply_sanitizers(pool_flags)
pool_enable_ccache(pool_flags)

# Pure physics: no SDL, no I/O, no globals, no RNG.
add_library(pool_physics STATIC
  src/physics/collision.cpp
  src/physics/cue.cpp
  src/physics/table.cpp
  src/physics/world.cpp)
add_library(pool::physics ALIAS pool_physics)
target_include_directories(pool_physics PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(pool_physics PUBLIC pool::flags)
target_compile_features(pool_physics PUBLIC cxx_std_23)
# FP contraction is enabled by default on clang/gcc; pin it off so goldens are
# stable across compilers that fuse differently.
target_compile_options(pool_physics PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-ffp-contract=off>)

# Game rules on top of physics (no rendering).
add_library(pool_game STATIC
  src/game/match.cpp
  src/game/rack.cpp)
add_library(pool::game ALIAS pool_game)
target_link_libraries(pool_game PUBLIC pool::physics)
target_compile_features(pool_game PUBLIC cxx_std_23)

if(POOL_BUILD_APP)
  add_library(pool_app_lib STATIC
    src/app/renderer.cpp
    src/app/input.cpp)
  add_library(pool::app_lib ALIAS pool_app_lib)
  target_link_libraries(pool_app_lib PUBLIC pool::game pool::sdl)
  target_compile_features(pool_app_lib PUBLIC cxx_std_23)

  add_executable(pool_app src/app/main.cpp)
  target_link_libraries(pool_app PRIVATE pool::app_lib)
  target_compile_features(pool_app PRIVATE cxx_std_23)
endif()

if(POOL_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(POOL_BUILD_BENCH)
  add_subdirectory(bench)
endif()
