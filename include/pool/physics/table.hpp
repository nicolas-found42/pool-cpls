#pragma once

#include "pool/physics/constants.hpp"
#include "pool/physics/world.hpp"

namespace pool::physics {

// Six-pocket layout for a rectangular table: four corners plus one side pocket
// per long rail.
[[nodiscard]] std::vector<Pocket> make_pockets(const TableSpec& table);

// Bounds with the balls' radius inset so a resting ball touches the cushion
// nose without overlapping it.
[[nodiscard]] Bounds make_bounds(const TableSpec& table, double radius);

// Classic 8-ball triangle rack, apex on the foot spot, eight ball in the middle
// of the third row, one solid and one stripe in the back corners.
void rack_eight_ball(World& world, const TableSpec& table, double radius, double gap = 0.0);

} // namespace pool::physics
