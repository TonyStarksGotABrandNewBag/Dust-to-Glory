// Walking routes: how somebody on foot gets from where they stand to a door across town.
//
// People keep to the pavement ring round each block, half a sidewalk in from the kerb, and cross a
// road only at a corner, straight over to the same corner of the next block's ring. A route walks the
// block grid one block at a time towards the goal -- along one axis until it is in line, then the
// other -- so it never cuts across a lawn or through a building, and it ends with the front path from
// the pavement up to the door. A greedy walk is enough: inside a town the blocks form a full grid
// (parks are blocks too), so a block is always there in the direction the goal lies.
#pragma once

#include "Sim/CityLayout.h"
#include "Sim/StreetFurniture.h"

#include <array>
#include <vector>

namespace dtg::sim
{
	/**
	 * The pavement ring round a block, in metres: its four corners half a sidewalk in from the kerb,
	 * anticlockwise from the south-west, so side N runs from corner N to corner N + 1.
	 */
	std::array<std::array<double, 2>, 4> PavementRing(const Rect& Block, const FurnitureRules& Rules = FurnitureRules());

	/**
	 * Waypoints from a start to a goal (a building's door), in metres: onto the ring of the block the
	 * start is on, round it to the corner facing the goal, over the road to the next block's ring, and
	 * so on to the goal's block, round to the point on its ring opposite the goal, and up to the goal,
	 * which is the last waypoint. Empty when the blocks do not reach: the goal is in another town,
	 * or either end is out in open country.
	 */
	std::vector<std::array<double, 2>> PavementRoute(const CityPlan& World, double FromX, double FromY, double ToX, double ToY,
		const FurnitureRules& Rules = FurnitureRules());
}
