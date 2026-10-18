#include "pool/app/input.hpp"
#include "pool/app/renderer.hpp"
#include "pool/game/match.hpp"
#include "pool/game/rack.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kMaxFrameTime = 0.25; // clamp after a stall

// A --smoke run advances a fixed number of substeps, renders a few frames (the
// CI job uses SDL_VIDEODRIVER=dummy) and exits. It proves the binary links and
// the render path does not trap, without needing a display.
int run_smoke(SDL_Renderer* renderer, pool::physics::World& world, const pool::game::Match& match,
              int frames) {
  pool::app::Renderer painter(renderer, world);
  for (int i = 0; i < frames; ++i) {
    world.advance(8);
    int out_w = 0;
    int out_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &out_w, &out_h);
    const pool::app::Viewport view =
      pool::app::fit_viewport(static_cast<float>(out_w), static_cast<float>(out_h));
    painter.draw(world, match, view, true, {1.0, 0.0});
    SDL_RenderPresent(renderer);
  }
  std::printf("smoke: %d frames, %zu balls, fingerprint %016llx\n", frames, world.balls().size(),
              static_cast<unsigned long long>(world.fingerprint()));
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  bool smoke = false;
  const char* screenshot_path = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (SDL_strcmp(argv[i], "--smoke") == 0) {
      smoke = true;
    }
    if (SDL_strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      screenshot_path = argv[++i];
    }
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("pool-cpls", kWindowWidth, kWindowHeight,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderVSync(renderer, 1);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  pool::physics::World world;
  pool::game::Match match;
  pool::game::rack(world);

  if (screenshot_path != nullptr) {
    // Render one frame and write it out. Used to verify the renderer without a
    // display: `pool_app --screenshot shot.bmp` then inspect the file.
    pool::app::Renderer painter(renderer, world);
    int out_w = 0;
    int out_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &out_w, &out_h);
    const pool::app::Viewport view =
      pool::app::fit_viewport(static_cast<float>(out_w), static_cast<float>(out_h));
    painter.draw(world, match, view, true, {1.0, -0.18});

    SDL_Surface* frame = SDL_RenderReadPixels(renderer, nullptr);
    int code = 1;
    if (frame != nullptr) {
      code = SDL_SaveBMP(frame, screenshot_path) ? 0 : 1;
      SDL_DestroySurface(frame);
    }
    std::printf("screenshot: %s (%dx%d) code %d\n", screenshot_path, out_w, out_h, code);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return code;
  }

  if (smoke) {
    const int code = run_smoke(renderer, world, match, 4);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return code;
  }

  pool::app::Renderer painter(renderer, world);
  pool::app::Input input;

  std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();
  double accumulator = 0.0;
  bool running = true;
  bool show_aim = true;
  bool shot_pending = false;

  while (running) {
    const auto now = std::chrono::steady_clock::now();
    double frame_time = std::chrono::duration<double>(now - previous).count();
    previous = now;
    frame_time = std::min(frame_time, kMaxFrameTime);

    int out_w = 0;
    int out_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &out_w, &out_h);
    const pool::app::Viewport view =
      pool::app::fit_viewport(static_cast<float>(out_w), static_cast<float>(out_h));

    const pool::app::InputFrame frame = input.poll(window);
    running = !frame.quit;
    if (frame.toggle_aim) {
      show_aim = !show_aim;
    }

    // The mouse points where the cue ball should go; the keyboard rotates the
    // aim instead. Both feed the same strike.
    pool::physics::Vec2 aim = frame.aim;
    const pool::physics::Ball& cue_ball = world.ball(pool::physics::kCueBall);
    if (frame.has_pointer && cue_ball.on_table) {
      const pool::physics::Vec2 pointer_world =
        pool::app::Renderer::screen_to_world(view, {frame.pointer_x, frame.pointer_y});
      const pool::physics::Vec2 to_pointer = pointer_world - cue_ball.position;
      if (pool::physics::length_squared(to_pointer) > 1.0e-6) {
        aim = pool::physics::normalized(to_pointer);
      }
    }

    if (frame.reset) {
      pool::game::rack(world);
      match.reset();
      accumulator = 0.0;
      shot_pending = false;
    }

    if (frame.shoot && world.at_rest()) {
      match.begin_shot(world);
      world.strike(pool::app::Input::to_strike(frame));
      shot_pending = true;
    }

    if (frame.step_once && world.at_rest()) {
      world.step();
    }

    accumulator += frame_time;
    const double dt = world.params().fixed_dt;
    int guard = 0;
    while (accumulator >= dt && guard < 64) {
      world.step();
      accumulator -= dt;
      ++guard;
    }

    // Adjudicate exactly once per shot, after the table settles.
    if (shot_pending && world.at_rest()) {
      const pool::game::ShotResult result = match.adjudicate(world);
      shot_pending = false;
      if (result.facts.cue_ball_pocketed) {
        world.place(pool::physics::kCueBall,
                    {world.table().length * 0.25, world.table().width * 0.5});
      }
      if (result.has_winner) {
        std::printf("game over: player %d wins\n", static_cast<int>(result.winner) + 1);
      }
    }

    painter.draw(world, match, view, show_aim && world.at_rest(), aim);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
