#include "pool/app/input.hpp"

#include <algorithm>
#include <cmath>

namespace pool::app {
namespace {

constexpr float kAimStep = 0.035F; // rad per key repeat
constexpr float kPowerStep = 0.04F;
constexpr double kMaxSpeed = 8.0;

} // namespace

InputFrame Input::poll(SDL_Window* window) {
  InputFrame frame{};

  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      frame.quit = true;
      break;
    case SDL_EVENT_KEY_DOWN: {
      const SDL_Keycode key = event.key.key;
      if (key == SDLK_ESCAPE || key == SDLK_Q) {
        frame.quit = true;
      }
      if (key == SDLK_SPACE) {
        frame.shoot = true;
      }
      if (key == SDLK_R) {
        frame.reset = true;
      }
      if (key == SDLK_TAB) {
        frame.toggle_aim = !frame.toggle_aim;
      }
      if (key == SDLK_N) {
        frame.step_once = true;
      }

      // Aim rotates counter-clockwise with A/D and the arrow keys.
      if (key == SDLK_LEFT || key == SDLK_A) {
        aim_angle_ += kAimStep;
      }
      if (key == SDLK_RIGHT || key == SDLK_D) {
        aim_angle_ -= kAimStep;
      }
      if (key == SDLK_UP || key == SDLK_W) {
        power_ = std::min(1.0F, power_ + kPowerStep);
      }
      if (key == SDLK_DOWN || key == SDLK_S) {
        power_ = std::max(0.05F, power_ - kPowerStep);
      }

      // English: Z/C for side spin, X/V for draw and follow.
      if (key == SDLK_Z) {
        side_spin_ = std::max(-0.5F, side_spin_ - 0.05F);
      }
      if (key == SDLK_C) {
        side_spin_ = std::min(0.5F, side_spin_ + 0.05F);
      }
      if (key == SDLK_X) {
        draw_spin_ = std::max(-0.5F, draw_spin_ - 0.05F);
      }
      if (key == SDLK_V) {
        draw_spin_ = std::min(0.5F, draw_spin_ + 0.05F);
      }
      if (key == SDLK_B) {
        side_spin_ = 0.0F;
        draw_spin_ = 0.0F;
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      // Mouse aims from the cue ball toward the cursor.
      float mx = 0.0F;
      float my = 0.0F;
      SDL_GetMouseState(&mx, &my);
      int width = 0;
      int height = 0;
      SDL_GetWindowSize(window, &width, &height);
      mouse_x_ = mx;
      mouse_y_ = my;
      mouse_valid_ = true;
      (void)height;
      break;
    }
    default:
      break;
    }
  }

  frame.aim = {std::cos(static_cast<double>(aim_angle_)),
               std::sin(static_cast<double>(aim_angle_))};
  frame.power = power_;
  frame.side_spin = side_spin_;
  frame.draw_spin = draw_spin_;
  if (mouse_valid_) {
    // The renderer converts world to screen; the caller resolves the aim from
    // the cue ball to the mouse, so here we only report the raw pointer.
    frame.pointer_x = mouse_x_;
    frame.pointer_y = mouse_y_;
    frame.has_pointer = true;
  }
  return frame;
}

physics::CueStrike Input::to_strike(const InputFrame& frame) {
  physics::CueStrike strike{};
  strike.direction = frame.aim;
  strike.speed = static_cast<double>(frame.power) * kMaxSpeed;
  strike.tip_offset = {static_cast<double>(frame.side_spin), static_cast<double>(frame.draw_spin)};
  strike.elevation = std::abs(static_cast<double>(frame.draw_spin)) > 0.4 ? 0.18 : 0.0;
  return strike;
}

} // namespace pool::app
