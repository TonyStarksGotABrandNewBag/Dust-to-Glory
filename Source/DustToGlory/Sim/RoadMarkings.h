// The paint on the road.
//
// An empty grey ribbon reads as a placeholder no matter how good the terrain around it is; the
// markings are most of what makes a road look like a road at speed. They are generated here rather
// than in the renderer for the same reasons the trees are: so they can be tested without an engine,
// and so the offline check and the game draw the same paint.
//
// What goes where follows the US convention the rest of the project assumes, because a player who
// has driven in the Carolinas will notice immediately if it is wrong:
//   - yellow separates traffic going opposite ways, white separates traffic going the same way
//   - a two-lane road gets a double yellow centre, or a broken yellow where passing is allowed
//   - a divided highway has no yellow centre at all: white lane lines, a yellow left edge on each
//     carriageway and a white right edge
//   - edge lines are solid and sit just inside the pavement
//   - junctions get stop bars and crosswalks
#pragma once

#include "Sim/CityLayout.h"

#include <cstddef>
#include <vector>

namespace dtg::sim
{
	enum class MarkKind : int
	{
		CentreDouble,  // two solid yellow lines: no passing either way
		CentreBroken,  // broken yellow: passing allowed
		LaneBroken,    // broken white between lanes going the same way
		EdgeWhite,     // solid white right-hand edge
		EdgeYellow,    // solid yellow left edge of a divided carriageway
		StopBar,       // thick white bar across a lane at a junction
		Crosswalk,     // a ladder rung
		Count
	};
	const char* MarkKindName(MarkKind Kind);

	/** One painted stripe: a quad, given as a segment with a width. World metres. */
	struct RoadMark
	{
		double Ax = 0.0;
		double Ay = 0.0;
		double Az = 0.0;
		double Bx = 0.0;
		double By = 0.0;
		double Bz = 0.0;
		double WidthM = 0.12;
		MarkKind Kind = MarkKind::EdgeWhite;
		/** Paint colour, linear 0..1. Yellow is the warm US yellow, not a primary. */
		double Rgb[3] = {0.52, 0.50, 0.44};
	};

	struct MarkingRules
	{
		/** Standard US lane width; decides how many lanes a carriageway is marked for. */
		double LaneWidthM = 3.7;
		double LineWidthM = 0.12;
		/** Broken line: paint, then gap. */
		double DashM = 3.0;
		double GapM = 9.0;
		/** How far inside the pavement edge the edge line sits. */
		double EdgeInsetM = 0.55;
		/**
		 * Paint sits this far above the surface so it does not fight with it. Road paint really is
		 * about a millimetre thick, but a depth buffer a kilometre deep cannot tell a millimetre
		 * from nothing: at this draw distance a thinner lift flickers in and out along the road.
		 */
		double LiftM = 0.035;
		std::size_t Limit = 60000;
	};

	/**
	 * Every stripe on one highway, following its deck profile so the paint climbs its bridges with
	 * it. An interstate is marked as a divided carriageway; anything narrower as a two-lane road.
	 */
	std::vector<RoadMark> MarkHighway(const HighwayPlan& Road, const MarkingRules& Rules = MarkingRules());

	/**
	 * Stripes for a town's streets: centre lines on the through streets, and stop bars and
	 * crosswalks at the junctions. Heights come from the caller, since a town sits on a level pad.
	 */
	std::vector<RoadMark> MarkStreets(const CityPlan& World, double (*HeightAt)(double, double, const void*),
		const void* User, const MarkingRules& Rules = MarkingRules());
}
