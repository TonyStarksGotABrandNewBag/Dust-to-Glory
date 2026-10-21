// What the ground is for. Every point of the state is one of a few kinds of land -- water, a road, a
// street, somebody's yard, a store's lot, the kept verge beside a road, woods, a field, or a town's
// paved blocks -- and everything that grows or lies on the ground follows from that one answer:
// the trees (Vegetation), the grass (StreetFurniture's ground cover), and the colour of the ground
// itself (the world builder's terrain). Deciding it once, here, is what stops the woods being thick in
// one system and thin in another, and keeps a field a field in all of them.
//
// Deterministic in world space, like everything else in the plan: the same point gives the same
// answer whichever area was asked for.
#pragma once

#include "Sim/CityLayout.h"
#include "Sim/Lots.h"
#include "Sim/StatePlan.h"

#include <vector>

namespace dtg::sim
{
	enum class LandCover : int
	{
		Water,
		Road,       // a highway's carriageway or verge
		Street,     // a town street
		Yard,       // a home's lot in a crossroads community: kept in front, wooded behind
		Commercial, // a store's or a church's lot in a crossroads community: cleared
		Roadside,   // the shoulder and ditch along a street, and the mown verge beside a highway
		Woodland,   // pine woods with an understory
		Field,      // open farmland between the woods
		Town,       // a town's paved blocks and parks (planted as they always were)
		Count
	};
	const char* LandCoverName(LandCover Cover);

	struct LandCoverRules
	{
		/** The kept verge beside a highway, out from the carriageway's edge. */
		double RoadsideM = 10.0;
		/** The noise that lays fields among the woods, and the share of it that is field. */
		double FieldWavelengthM = 480.0;
		double FieldAbove = 0.52;
		/** No fields this close to the sea: the coast strip is palms and dunes, not crops (250 m is six real kilometres at the state's compression). */
		double FieldCoastM = 250.0;
		/** Woods all round a crossroads community whatever the fields do beyond, so from its streets the woods are the horizon. */
		double CrossroadsWoodsM = 220.0;
	};

	/**
	 * The land cover of an area, ready to answer per point. Building one gathers the area's lots once,
	 * since a yard and a store's lot are decided by the plan's properties (Sim/Lots).
	 */
	class LandCoverMap
	{
	public:
		LandCoverMap(const StatePlan& Map, const CityPlan& World, const Rect& Area, const LandCoverRules& Rules = LandCoverRules());

		LandCover At(double X, double Y) const;
		/** The property whose lot holds the point, if a crossroads community's lot does. */
		const LotFeatures* LotAt(double X, double Y) const;
		/** Whether a point on a lot is paved or built on: the drive, the pad, the walk or the shed, and a little round each. */
		static bool OnLotWorks(const LotFeatures& Lot, double X, double Y, double MarginM);

		const LandCoverRules& Rules() const { return Settings; }
		const std::vector<LotFeatures>& Lots() const { return Properties; }

	private:
		const StatePlan& Map;
		const CityPlan& World;
		LandCoverRules Settings;
		std::vector<LotFeatures> Properties;
	};

	/** Whether a point lies within Reach of a crossroads community's edge, outside it. */
	bool NearCrossroads(const CityPlan& World, double X, double Y, double Reach);
}
