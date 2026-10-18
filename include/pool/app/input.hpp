#pragma once

#include "pool/physics/world.hpp"

#include <SDL3/SDL.h>

namespace pool::app {

// One frame's worth of intent, resolved from SDL events. Keeping input in a
// plain struct makes the app layer testable without a window.
struct InputFrame {
  physics::Vec2 aim{1.0, 0.0};
  float power{0.5F};     // 0..1
  float side_spin{0.0F}; // -0.5..0.5 tip offsets, in radii
  float draw_spin{0.0F}; // -0.5..0.5
  float pointer_x{0.0F};
  float pointer_y{0.0F};
  bool has_pointer{false};
  bool shoot{false};
  bool reset{false};
  bool quit{false};
  bool toggle_aim{false};
  bool step_once{false};
};

class Input {
public:
  InputFrame poll(SDL_Window* window);
  [[nodiscard]] static physics::CueStrike to_strike(const InputFrame& frame);

private:
  float aim_angle_{0.0F};
  float power_{0.5F};
  float side_spin_{0.0F};
  float draw_spin_{0.0F};
  float mouse_x_{0.0F};
  float mouse_y_{0.0F};
  bool mouse_valid_{false};
};

} // namespace pool::app
