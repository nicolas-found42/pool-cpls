#include "pool/physics/table.hpp"

#include <cmath>

namespace pool::physics {

std::vector<Pocket> make_pockets(const TableSpec& table) {
  // Corner pockets sit on the corners; side pockets are centred on the long
  // rails. The capture radius is half the mouth, and the mouth is measured
  // between opposing cushion noses exactly as the WPA equipment specification
  // defines it (§9): 4.5 in at the corners, 5 in at the sides, so the side
  // pocket is deliberately wider than the corner pocket.
  const double corner_radius = table.corner_mouth * 0.5;
  const double side_radius = table.side_mouth * 0.5;
  const double l = table.length;
  const double w = table.width;
  return {
    {{0.0, 0.0}, corner_radius}, {{l * 0.5, -0.004}, side_radius},    {{l, 0.0}, corner_radius},
    {{0.0, w}, corner_radius},   {{l * 0.5, w + 0.004}, side_radius}, {{l, w}, corner_radius},
  };
}

Bounds make_bounds(const TableSpec& table, double radius) {
  return {radius, radius, table.length - radius, table.width - radius};
}

void rack_eight_ball(World& world, const TableSpec& table, double radius, double gap) {
  // Standard 8-ball rack: rows of 1..5, apex on the foot spot (3/4 of the way
  // down the table), eight ball centred in row three, back corners split
  // between a solid and a stripe.
  static constexpr std::uint8_t kRack[kBallCount - 1] = {1, 2, 9,  3,  8, 10, 4, 14,
                                                         7, 6, 15, 13, 5, 12, 11};

  const double spacing = 2.0 * radius + gap;
  const double row_height = spacing * std::sqrt(3.0) * 0.5;
  const Vec2 apex{table.length * 0.75, table.width * 0.5};

  std::size_t index = 0;
  for (int row = 0; row < 5; ++row) {
    const double x = apex.x + static_cast<double>(row) * row_height;
    for (int slot = 0; slot <= row; ++slot) {
      const double y =
        apex.y + (static_cast<double>(slot) - static_cast<double>(row) * 0.5) * spacing;
      world.place(kRack[index], {x, y});
      ++index;
    }
  }

  world.place(kCueBall, {table.length * 0.25, table.width * 0.5});
}

} // namespace pool::physics
