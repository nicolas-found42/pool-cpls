#include "pool/app/renderer.hpp"

#include "pool/physics/table.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pool::app {
namespace {

constexpr int kSegments = 64;
constexpr float kPi = 3.14159265358979323846F;

SDL_FColor fcolor(SDL_Color color, std::uint8_t alpha) {
  return {static_cast<float>(color.r) / 255.0F, static_cast<float>(color.g) / 255.0F,
          static_cast<float>(color.b) / 255.0F, static_cast<float>(alpha) / 255.0F};
}

void push_vertex(std::vector<SDL_Vertex>& out, float x, float y, const SDL_FColor& color) {
  out.push_back({{x, y}, color, {0.0F, 0.0F}});
}

// Standard pool ball colours, 1-15 (index 0 is the cue ball).
constexpr SDL_Color kBallColors[16] = {
  {245, 245, 240, 255}, {222, 186, 40, 255}, {24, 74, 168, 255}, {196, 46, 38, 255},
  {120, 60, 160, 255},  {232, 118, 30, 255}, {22, 108, 72, 255}, {124, 42, 34, 255},
  {18, 18, 18, 255},    {222, 186, 40, 255}, {24, 74, 168, 255}, {196, 46, 38, 255},
  {120, 60, 160, 255},  {232, 118, 30, 255}, {22, 108, 72, 255}, {124, 42, 34, 255},
};

SDL_Color ball_color(std::uint8_t id) {
  return kBallColors[id < 16 ? id : 0];
}

} // namespace

Viewport fit_viewport(float window_width, float window_height) {
  constexpr float kLength = 2.54F;
  constexpr float kWidth = 1.27F;
  constexpr float kMargin = 52.0F;

  const float available_w = std::max(1.0F, window_width - 2.0F * kMargin);
  const float available_h = std::max(1.0F, window_height - 2.0F * kMargin);
  const float scale = std::min(available_w / kLength, available_h / kWidth);

  Viewport view{};
  view.scale = scale;
  view.width = kLength * scale;
  view.height = kWidth * scale;
  view.x = (window_width - view.width) * 0.5F;
  view.y = (window_height - view.height) * 0.5F;
  return view;
}

Renderer::Renderer(SDL_Renderer* renderer, physics::World& world)
  : renderer_(renderer) {
  world.set_record_contacts(false);
}

SDL_FPoint Renderer::world_to_screen(const Viewport& view, physics::Vec2 p) noexcept {
  // Table frame: +y is away from the viewer, screen +y is down.
  return {view.x + static_cast<float>(p.x) * view.scale,
          view.y + (1.27F - static_cast<float>(p.y)) * view.scale};
}

physics::Vec2 Renderer::screen_to_world(const Viewport& view, SDL_FPoint p) noexcept {
  // Inverse of world_to_screen: the y axis is flipped because the table frame
  // has +y away from the viewer.
  return {(static_cast<double>(p.x) - static_cast<double>(view.x)) /
            static_cast<double>(view.scale),
          (static_cast<double>(view.y + view.height) - static_cast<double>(p.y)) /
            static_cast<double>(view.scale)};
}

void Renderer::fill_circle(float cx, float cy, float radius, SDL_Color color, std::uint8_t alpha) {
  const SDL_FColor c = fcolor(color, alpha);
  vertices_.clear();
  vertices_.reserve(static_cast<std::size_t>(kSegments) * 3U);
  // SDL_RenderGeometry without an index buffer consumes the vertices as a
  // triangle LIST, not a fan: every vertex triple must be a complete triangle.
  for (int i = 0; i < kSegments; ++i) {
    const float a0 = (static_cast<float>(i) / static_cast<float>(kSegments)) * 2.0F * kPi;
    const float a1 = (static_cast<float>(i + 1) / static_cast<float>(kSegments)) * 2.0F * kPi;
    push_vertex(vertices_, cx, cy, c);
    push_vertex(vertices_, cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, c);
    push_vertex(vertices_, cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, c);
  }
  SDL_RenderGeometry(renderer_, nullptr, vertices_.data(), static_cast<int>(vertices_.size()),
                     nullptr, 0);
}

void Renderer::fill_ring(float cx, float cy, float outer, float inner, SDL_Color color,
                         std::uint8_t alpha) {
  const SDL_FColor c = fcolor(color, alpha);
  vertices_.clear();
  vertices_.reserve(static_cast<std::size_t>(kSegments) * 6U);
  for (int i = 0; i < kSegments; ++i) {
    const float a0 = (static_cast<float>(i) / static_cast<float>(kSegments)) * 2.0F * kPi;
    const float a1 = (static_cast<float>(i + 1) / static_cast<float>(kSegments)) * 2.0F * kPi;
    const float ox0 = cx + std::cos(a0) * outer;
    const float oy0 = cy + std::sin(a0) * outer;
    const float ox1 = cx + std::cos(a1) * outer;
    const float oy1 = cy + std::sin(a1) * outer;
    const float ix0 = cx + std::cos(a0) * inner;
    const float iy0 = cy + std::sin(a0) * inner;
    const float ix1 = cx + std::cos(a1) * inner;
    const float iy1 = cy + std::sin(a1) * inner;
    push_vertex(vertices_, ox0, oy0, c);
    push_vertex(vertices_, ox1, oy1, c);
    push_vertex(vertices_, ix0, iy0, c);
    push_vertex(vertices_, ox1, oy1, c);
    push_vertex(vertices_, ix1, iy1, c);
    push_vertex(vertices_, ix0, iy0, c);
  }
  SDL_RenderGeometry(renderer_, nullptr, vertices_.data(), static_cast<int>(vertices_.size()),
                     nullptr, 0);
}

// Horizontal band clipped to a circle: the stripe of a striped ball, seen from
// above. Built as a strip of quads so the band follows the sphere's edge.
void Renderer::fill_band(float cx, float cy, float radius, float half_height, SDL_Color color,
                         std::uint8_t alpha) {
  const SDL_FColor c = fcolor(color, alpha);
  vertices_.clear();
  const float step = 2.0F * radius / static_cast<float>(kSegments);
  for (int i = 0; i < kSegments; ++i) {
    const float x0 = -radius + static_cast<float>(i) * step;
    const float x1 = x0 + step;
    const float y0 = std::min(half_height, std::sqrt(std::max(0.0F, radius * radius - x0 * x0)));
    const float y1 = std::min(half_height, std::sqrt(std::max(0.0F, radius * radius - x1 * x1)));
    push_vertex(vertices_, cx + x0, cy - y0, c);
    push_vertex(vertices_, cx + x1, cy - y1, c);
    push_vertex(vertices_, cx + x0, cy + y0, c);
    push_vertex(vertices_, cx + x1, cy - y1, c);
    push_vertex(vertices_, cx + x1, cy + y1, c);
    push_vertex(vertices_, cx + x0, cy + y0, c);
  }
  SDL_RenderGeometry(renderer_, nullptr, vertices_.data(), static_cast<int>(vertices_.size()),
                     nullptr, 0);
}

void Renderer::fill_rounded_rect(const SDL_FRect& rect, float radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, 255);
  SDL_FRect body{rect.x + radius, rect.y, rect.w - 2.0F * radius, rect.h};
  SDL_RenderFillRect(renderer_, &body);
  SDL_FRect middle{rect.x, rect.y + radius, rect.w, rect.h - 2.0F * radius};
  SDL_RenderFillRect(renderer_, &middle);
  fill_circle(rect.x + radius, rect.y + radius, radius, color, 255);
  fill_circle(rect.x + rect.w - radius, rect.y + radius, radius, color, 255);
  fill_circle(rect.x + radius, rect.y + rect.h - radius, radius, color, 255);
  fill_circle(rect.x + rect.w - radius, rect.y + rect.h - radius, radius, color, 255);
}

void Renderer::draw_table(const Viewport& view) {
  const SDL_FRect rail{view.x - 26.0F, view.y - 26.0F, view.width + 52.0F, view.height + 52.0F};

  // Rail: a dark drop shadow plus a lit bevel, both rounded, no textures.
  fill_rounded_rect({rail.x + 5.0F, rail.y + 7.0F, rail.w, rail.h}, 20.0F, theme_.rail_dark);
  fill_rounded_rect(rail, 20.0F, theme_.rail);

  const SDL_FRect cloth{view.x, view.y, view.width, view.height};
  SDL_SetRenderDrawColor(renderer_, theme_.cloth.r, theme_.cloth.g, theme_.cloth.b, 255);
  SDL_RenderFillRect(renderer_, &cloth);

  // Vignette: a few soft bands near the rails read as light falloff from the
  // table's centre. Drawn as filled strips rather than outlines so no hard line
  // shows up as a scanline.
  SDL_SetRenderDrawColor(renderer_, theme_.cloth_dark.r, theme_.cloth_dark.g, theme_.cloth_dark.b,
                         26);
  for (int i = 0; i < 4; ++i) {
    const float inset = static_cast<float>(i) * 10.0F;
    SDL_FRect band{cloth.x + inset, cloth.y + inset, cloth.w - inset * 2.0F, 10.0F};
    SDL_RenderFillRect(renderer_, &band);
    SDL_FRect lower{cloth.x + inset, cloth.y + cloth.h - inset - 10.0F, cloth.w - inset * 2.0F,
                    10.0F};
    SDL_RenderFillRect(renderer_, &lower);
  }

  for (const physics::Pocket& pocket : physics::make_pockets(physics::TableSpec{})) {
    const SDL_FPoint p = world_to_screen(view, pocket.centre);
    const float r = static_cast<float>(pocket.radius) * view.scale * 1.4F;
    fill_circle(p.x + 2.0F, p.y + 3.0F, r, theme_.shadow, 130);
    fill_circle(p.x, p.y, r, theme_.pocket, 255);
  }
}

void Renderer::draw_ball(const physics::Ball& ball, const Viewport& view) {
  const SDL_FPoint p = world_to_screen(view, ball.position);
  const float radius = static_cast<float>(physics::BallSpec{}.radius) * view.scale;
  const SDL_Color base = ball_color(ball.id);

  // Contact shadow opposite the light source (top-left).
  fill_circle(p.x - radius * 0.16F, p.y + radius * 0.20F, radius * 1.04F, theme_.shadow, 110);

  if (physics::is_stripe(ball.id)) {
    fill_circle(p.x, p.y, radius, kBallColors[0], 255);
    fill_band(p.x, p.y, radius * 0.995F, radius * 0.42F, base, 255);
  } else {
    fill_circle(p.x, p.y, radius, base, 255);
  }

  if (ball.id != physics::kCueBall) {
    fill_circle(p.x, p.y, radius * 0.34F, kBallColors[0], 255);
    char label[4]{};
    const int digits = std::snprintf(label, sizeof(label), "%u", static_cast<unsigned>(ball.id));
    // Centre the label on the ball; SDL's debug font is a fixed 8x8 cell.
    const float text_w =
      static_cast<float>(digits) * static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    const float text_h = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 255);
    SDL_RenderDebugText(renderer_, p.x - text_w * 0.5F, p.y - text_h * 0.5F, label);
  }

  // Specular highlight + rim light sell the sphere without a texture.
  fill_circle(p.x - radius * 0.33F, p.y - radius * 0.35F, radius * 0.23F,
              SDL_Color{255, 255, 255, 255}, 190);
  fill_ring(p.x, p.y, radius, radius * 0.88F, SDL_Color{255, 255, 255, 255}, 30);
}

void Renderer::draw_aim(const physics::World& world, const Viewport& view,
                        physics::Vec2 direction) {
  const physics::Ball& cue = world.ball(physics::kCueBall);
  if (!cue.on_table) {
    return;
  }

  const physics::Vec2 aim = physics::normalized(direction);
  const SDL_FPoint start = world_to_screen(view, cue.position);
  const SDL_FPoint end = world_to_screen(view, cue.position + aim * 0.55);

  SDL_SetRenderDrawColor(renderer_, theme_.aim.r, theme_.aim.g, theme_.aim.b, theme_.aim.a);
  SDL_RenderLine(renderer_, start.x, start.y, end.x, end.y);

  const SDL_FPoint tip = world_to_screen(view, cue.position - aim * 0.085);
  fill_circle(tip.x, tip.y, 4.0F, theme_.aim, 220);
}

void Renderer::draw(const physics::World& world, const pool::game::Match& match,
                    const Viewport& view, bool show_aim, physics::Vec2 aim_direction) {
  SDL_SetRenderDrawColor(renderer_, 13, 15, 17, 255);
  SDL_RenderClear(renderer_);

  draw_table(view);
  for (const physics::Ball& ball : world.balls()) {
    if (ball.on_table) {
      draw_ball(ball, view);
    }
  }
  if (show_aim) {
    draw_aim(world, view, aim_direction);
  }

  const auto group_name = [](pool::game::Group group) {
    switch (group) {
    case pool::game::Group::solids:
      return "solids";
    case pool::game::Group::stripes:
      return "stripes";
    default:
      return "open";
    }
  };

  char hud[160]{};
  const int on_table =
    static_cast<int>(std::count_if(world.balls().begin(), world.balls().end(),
                                   [](const physics::Ball& ball) { return ball.on_table; }));

  std::snprintf(hud, sizeof(hud), "P%d %s | games %d-%d | balls %d | %s",
                static_cast<int>(match.current_player()) + 1,
                group_name(match.group_of_player(match.current_player())),
                match.games_won(pool::game::Player::one), match.games_won(pool::game::Player::two),
                on_table, match.phase() == pool::game::Phase::finished ? "game over" : "");
  SDL_SetRenderDrawColor(renderer_, theme_.text.r, theme_.text.g, theme_.text.b, 255);
  SDL_RenderDebugText(renderer_, 16.0F, 14.0F, hud);
}

} // namespace pool::app
