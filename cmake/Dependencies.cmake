# Third-party dependencies.
#
# Strategy: prefer a system/toolchain package when one is installed, otherwise
# fetch a pinned release tarball with a verified SHA-256. Nothing is vendored
# into the tree, so `git clone && cmake --preset dev` works offline only when
# the packages are already present.

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# --- SDL3 ------------------------------------------------------------------
# Only the front-end needs it. Physics and game rules stay dependency-free.
if(POOL_BUILD_APP)
  find_package(SDL3 3.2 CONFIG QUIET)

  if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not found; fetching pinned release-3.4.18")
    # On Linux, SDL3 refuses to configure without X11 or Wayland development
    # libraries. A CI runner only needs the dummy video driver, and SDL
    # documents -DSDL_UNIX_CONSOLE_BUILD=ON for exactly that case
    # (docs/README-cmake.md, "CMake fails to build without X11 or Wayland
    # support"). POOL_SDL_HEADLESS opts in; a Linux developer who wants a window
    # should install SDL3's build dependencies instead - see CONTRIBUTING.md.
    if(POOL_SDL_HEADLESS)
      set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
      set(SDL_ALSA OFF CACHE BOOL "" FORCE)
      set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
      set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
      set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
      message(STATUS "POOL_SDL_HEADLESS: SDL3 builds with the dummy video driver only")
    endif()
    FetchContent_Declare(SDL3
      URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.18.tar.gz
      URL_HASH SHA256=c4b08b950bd29d83caae0ab8d884298d27046a9d30700fb42841c50992665c90
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
  endif()

  add_library(pool_sdl INTERFACE)
  add_library(pool::sdl ALIAS pool_sdl)

  if(TARGET SDL3::SDL3)
    target_link_libraries(pool_sdl INTERFACE SDL3::SDL3)
  elseif(TARGET SDL3::SDL3-static)
    target_link_libraries(pool_sdl INTERFACE SDL3::SDL3-static)
  else()
    message(FATAL_ERROR "SDL3 was found but exposes no SDL3::SDL3 target")
  endif()
endif()

# --- Catch2 ----------------------------------------------------------------
if(POOL_BUILD_TESTS)
  find_package(Catch2 3 CONFIG QUIET)
  if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found; fetching pinned v3.16.1")
    FetchContent_Declare(Catch2
      URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz
      URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Catch2)
  endif()

  # Catch2's CTest integration lives in its extras directory, which is only
  # present in the source tree. An installed Catch2 package may not ship it, in
  # which case tests fall back to one CTest entry per binary.
  if(EXISTS "${Catch2_SOURCE_DIR}/extras/Catch.cmake")
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
  else()
    message(STATUS "Catch2 extras not found; per-binary CTest registration only")
  endif()
endif()

# --- Compiler launcher -----------------------------------------------------
function(pool_enable_ccache target)
  if(NOT POOL_ENABLE_CCACHE)
    return()
  endif()
  find_program(POOL_CCACHE_PROGRAM NAMES sccache ccache)
  if(POOL_CCACHE_PROGRAM)
    message(STATUS "Using compiler launcher: ${POOL_CCACHE_PROGRAM}")
    # Only the launcher is set. Flagging the launcher with compiler-specific
    # options breaks other compilers: an earlier version passed
    # `-Xclang -fno-pch-timestamp` here, which MSVC rejects outright.
    set(CMAKE_CXX_COMPILER_LAUNCHER "${POOL_CCACHE_PROGRAM}" PARENT_SCOPE)
    set(CMAKE_C_COMPILER_LAUNCHER "${POOL_CCACHE_PROGRAM}" PARENT_SCOPE)
  endif()
endfunction()
