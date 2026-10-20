// Street furniture: what stands beside the roads.
//
// Wooden utility poles along the highways, as every Carolina back road has; lamps along the town
// streets, on the sidewalk by the kerb; a fire hydrant on a corner of each block; and concrete
// barriers along any deck raised high enough that driving off it would be a fall.
//
// It lives here for the same reasons the vegetation does: placement is testable without an engine,
// and it has to be stable while terrain streams in chunks. Every prop is laid on a grid in world
// space -- distance along a road in whole multiples of the spacing -- and a chunk takes only the props
// whose position falls in it (half-open, so a prop on a boundary belongs to exactly one chunk). Ask
// for the same ground twice, or in two halves, and you get the same props.
#pragma once

#include "Sim/CityLayout.h"
#include "Sim/StatePlan.h"

#include <cstddef>
#include <vector>

namespace dtg::sim
{
	enum class PropKind : int
	{
		UtilityPole, // the wooden pole carrying the lines along a highway
		StreetLamp,  // on the sidewalk by the kerb, its arm reaching over the street
		Hydrant,     // one per block, on a corner
		Barrier,     // a concrete barrier along the edge of a raised deck
		Bin,         // a trash can on the sidewalk, at the corner across from the hydrant
		StreetTree,  // on the lawn strip inside the sidewalk, round every block that has one
		Grass,       // a clump of grass blades on a lawn (GroundCoverIn, not FurnitureIn)
		Count
	};
	const char* PropKindName(PropKind Kind);

	struct Prop
	{
		PropKind Kind = PropKind::StreetLamp;
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;   // the surface it stands on
		double Yaw = 0.0; // radians. A lamp faces the street; a pole and a barrier run with the road
		int Variant = 0;  // 0..3, stable per prop, for the renderer to choose a model by. A street tree's species: 0 palm, 1 oak
	};

	struct FurnitureRules
	{
		double PoleSpacingM = 45.0;
		/** Out from the carriageway's edge to the pole. */
		double PoleSetbackM = 4.0;
		double LampSpacingM = 32.0;
		/** In from the kerb to the lamp. */
		double LampSetbackM = 0.7;
		/** In from both kerbs to the trash can: on the sidewalk, clear of the lamp and the corner. */
		double BinSetbackM = 1.2;
		/** The sidewalk's height above the street pad; matches the kerb the world builder draws. */
		double KerbM = 0.15;
		double BarrierLengthM = 3.0;
		double TreeSpacingM = 11.0;
		/** In from the kerb to the tree: past the sidewalk, on the lawn. */
		double TreeSetbackM = 3.3;
		/** Palms this near the coast (metres), live oaks inland. */
		double PalmCoastM = 25000.0;
		/** Pole top above the ground: where the lines hang from. Matches the model the game draws. */
		double PoleHeightM = 9.8;
		/** Grass clumps on a jittered grid this far apart, on the lawns. */
		double GrassPitchM = 1.8;
		/** The sidewalk ring round a block, which the grass stays off. Matches the world builder. */
		double SidewalkM = 2.5;
		/** A deck this far above the natural ground beneath gets barriers on both edges. */
		double ElevatedM = 2.5;
		/** Hard cap on one request, so a huge area cannot blow the frame budget. */
		std::size_t Limit = 4000;
	};

	/** Every prop standing in Area. Deterministic in world space, like PlantsIn. */
	std::vector<Prop> FurnitureIn(const StatePlan& Map, const CityPlan& World, const Rect& Area,
		const FurnitureRules& Rules = FurnitureRules());

	/** One span of the lines between two utility poles, pole top to pole top. World metres. */
	struct WireSpan
	{
		double Ax = 0.0;
		double Ay = 0.0;
		double Az = 0.0;
		double Bx = 0.0;
		double By = 0.0;
		double Bz = 0.0;
	};

	/**
	 * Every span whose first pole stands in Area. A span exists only between two consecutive poles that
	 * both stand: where a pole was skipped (water, a town, another road), the line has a gap.
	 */
	std::vector<WireSpan> WiresIn(const StatePlan& Map, const CityPlan& World, const Rect& Area,
		const FurnitureRules& Rules = FurnitureRules());

	/**
	 * Grass clumps in Area: a jittered grid over every lawn, off the sidewalk ring and out of the
	 * buildings. Kept apart from FurnitureIn because there are far more of them.
	 */
	std::vector<Prop> GroundCoverIn(const StatePlan& Map, const CityPlan& World, const Rect& Area,
		const FurnitureRules& Rules = FurnitureRules());
}
