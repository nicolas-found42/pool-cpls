#include "pool/app/renderer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using pool::app::fit_viewport;

TEST_CASE("the viewport preserves the table aspect ratio", "[app]") {
  const auto view = fit_viewport(1280.0F, 720.0F);
  REQUIRE(view.width / view.height == Catch::Approx(2.0F).margin(1.0e-3F));
}

TEST_CASE("the viewport is centred in the window", "[app]") {
  const auto view = fit_viewport(1000.0F, 600.0F);
  REQUIRE(view.x + view.width * 0.5F == Catch::Approx(500.0F).margin(1.0e-3F));
  REQUIRE(view.y + view.height * 0.5F == Catch::Approx(300.0F).margin(1.0e-3F));
}

TEST_CASE("the limiting axis decides the scale", "[app]") {
  // 2000x400 leaves ~296 px of height after margins, so height limits the scale.
  const auto wide = fit_viewport(2000.0F, 400.0F);
  REQUIRE(wide.height == Catch::Approx(296.0F).margin(1.0F));
  REQUIRE(wide.scale == Catch::Approx(296.0F / 1.27F).margin(1.0F));

  // 400x2000 leaves ~296 px of width, so width limits it.
  const auto tall = fit_viewport(400.0F, 2000.0F);
  REQUIRE(tall.width == Catch::Approx(296.0F).margin(1.0F));
  REQUIRE(tall.scale == Catch::Approx(296.0F / 2.54F).margin(1.0F));
}

TEST_CASE("degenerate window sizes do not produce negative scales", "[app]") {
  const auto view = fit_viewport(0.0F, 0.0F);
  REQUIRE(view.scale > 0.0F);
  REQUIRE(view.width > 0.0F);
  REQUIRE(view.height > 0.0F);
}
