#pragma once

#include "pool/game/match.hpp"
#include "pool/physics/world.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace pool::app {

struct Viewport {
  float x{0.0F};
  float y{0.0F};
  float width{0.0F};
  float height{0.0F};
  float scale{1.0F};
};

// World (metres) -> screen (pixels). Preserves the table's aspect ratio.
[[nodiscard]] Viewport fit_viewport(float window_width, float window_height);

struct Theme {
  SDL_Color cloth{24, 96, 62, 255};
  SDL_Color cloth_dark{16, 68, 44, 255};
  SDL_Color rail{84, 42, 18, 255};
  SDL_Color rail_dark{56, 26, 10, 255};
  SDL_Color pocket{8, 8, 8, 255};
  SDL_Color shadow{0, 0, 0, 255};
  SDL_Color aim{255, 255, 255, 190};
  SDL_Color text{240, 240, 240, 255};
};

// Immediate-mode painter. Every ball is procedural geometry: SDL3 has no circle
// primitive, so circles, rings and stripes are triangle meshes.
class Renderer {
public:
  Renderer(SDL_Renderer* renderer, physics::World& world);

  void draw(const physics::World& world, const pool::game::Match& match, const Viewport& view,
            bool show_aim, physics::Vec2 aim_direction);

  // Both transforms are pure functions of the viewport, so they are static:
  // nothing about them depends on the renderer instance.
  [[nodiscard]] static SDL_FPoint world_to_screen(const Viewport& view, physics::Vec2 p) noexcept;
  [[nodiscard]] static physics::Vec2 screen_to_world(const Viewport& view, SDL_FPoint p) noexcept;

private:
  void draw_table(const Viewport& view);
  void draw_ball(const physics::Ball& ball, const Viewport& view);
  void draw_aim(const physics::World& world, const Viewport& view, physics::Vec2 direction);
  void fill_circle(float cx, float cy, float radius, SDL_Color color, std::uint8_t alpha);
  void fill_ring(float cx, float cy, float outer, float inner, SDL_Color color, std::uint8_t alpha);
  void fill_band(float cx, float cy, float radius, float half_height, SDL_Color color,
                 std::uint8_t alpha);
  void fill_rounded_rect(const SDL_FRect& rect, float radius, SDL_Color color);

  SDL_Renderer* renderer_{nullptr};
  Theme theme_{};
  std::vector<SDL_Vertex> vertices_{};
};

} // namespace pool::app
