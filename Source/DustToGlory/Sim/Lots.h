// What a property adds round its building: the things that make a lot read as somebody's place rather
// than a box on grass. A driveway from the road to a parking pad beside the house, a mailbox at the
// road's edge by the drive, a walk from the pad to the door, foundation shrubs along the front wall, a
// chain-link fence round some lots and a shed in the back corner of some, and the culvert where the
// drive crosses the roadside ditch. All of it is decided here from the plan, stable in world space
// (the same lot gets the same features whichever chunk asks), and drawn by the world builder.
//
// Only crossroads communities (CityStyle::Rural) get them for now: that is the start town, Longs, and
// the look is a Horry County yard. A town's lots stay as they were.
#pragma once

#include "Sim/CityLayout.h"
#include "Sim/StatePlan.h"
#include "Sim/Vegetation.h"

#include <cstddef>
#include <vector>

namespace dtg::sim
{
	/** A line on the ground between two points, world metres. */
	struct LotLine
	{
		double Ax = 0.0;
		double Ay = 0.0;
		double Bx = 0.0;
		double By = 0.0;
	};

	enum class LotPaving : int
	{
		Gravel,   // sandy gravel, the Horry County default
		Concrete,
	};

	struct LotFeatures
	{
		int BuildingId = 0;
		Rect Lot;           // the lot inside its block
		Rect Driveway;      // from the road's edge to the pad
		Rect Pad;           // parking beside the house, level with its front
		Rect Path;          // the front walk, from the pad to the door
		LotPaving Paving = LotPaving::Gravel;
		bool bMailbox = false;
		double MailboxX = 0.0;
		double MailboxY = 0.0;
		double MailboxYaw = 0.0;   // radians; the box faces the road
		std::vector<LotLine> Fence;
		bool bShed = false;
		Rect Shed;
		std::vector<Plant> Shrubs;  // foundation plantings along the front wall
		bool bCulvert = false;
		double CulvertX = 0.0;
		double CulvertY = 0.0;
		double CulvertYaw = 0.0;   // radians; the pipe runs with the road
	};

	struct LotRules
	{
		double DriveWidthM = 3.2;
		double PadM = 6.0;
		double PathWidthM = 1.0;
		double ShrubSpacingM = 2.2;
		/** Out from the front wall to the shrubs. */
		double ShrubOutM = 1.0;
		/** Lots fenced, in thirds: 1 of 3. */
		int FenceOneIn = 3;
		/** Lots with a shed: 1 of 2. */
		int ShedOneIn = 2;
		/** Lots paved in concrete rather than gravel: 2 of 5. */
		int ConcreteOneIn = 5;
		std::size_t Limit = 2000;
	};

	/** The properties whose lots overlap Area, in crossroads communities. */
	std::vector<LotFeatures> LotsIn(const StatePlan& Map, const CityPlan& World, const Rect& Area, const LotRules& Rules = LotRules());

	/**
	 * The ditch along a crossroads community's streets: how far below the road grade the ground lies,
	 * OffEdgeM out from the asphalt's edge. Flat for the shoulder, then a shallow swale, then the lawn.
	 */
	double DitchDepthM(double OffEdgeM);
	/** The shoulder's width before the ditch begins, and where the ditch ends; a culvert sits between. */
	constexpr double DitchFromM = 1.4;
	constexpr double DitchToM = 4.6;
}
