// What grows where. Scatters trees and scrub over a state deterministically, so the engine layer
// only has to turn a list of plants into geometry.
//
// This lives in the simulation rather than the renderer for two reasons. It is testable here -- a
// forest that wanders onto the interstate is a unit test, not a bug report -- and it has to be
// *stable*: terrain is streamed in chunks that are built, released and built again as the player
// drives around, and a tree that moves when its chunk reloads is worse than no tree at all. So
// nothing here depends on which chunk asked, only on where the plant stands. Ask for the same patch
// of ground twice and you get the same trees, in the same order.
//
// Species follow the land, because that is what makes a drive read as the Carolinas or as Florida
// rather than as green noise: longleaf pine on the sandhills and the inland flats, live oak in the
// towns and the older country, cabbage palm and palmetto along the coast, bald cypress standing in
// the swamp edges and river bottoms, and scrub where the soil is thin or the elevation is against
// trees.
#pragma once

#include "Sim/CityLayout.h"
#include "Sim/StatePlan.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dtg::sim
{
	enum class PlantKind : int
	{
		Pine,     // longleaf and loblolly: tall, narrow, bare trunk most of the way up
		Oak,      // live oak: short trunk, wide spreading crown
		Palm,     // cabbage palm: bare curved trunk, a burst of fronds
		Palmetto, // low fan palmetto, no real trunk
		Cypress,  // bald cypress: flared base, standing in or beside water
		Scrub,    // bushes and brush
		Count
	};
	const char* PlantKindName(PlantKind Kind);

	/** One plant, in world metres. */
	struct Plant
	{
		double X = 0.0;
		double Y = 0.0;
		double GroundM = 0.0;   // the surface it stands on
		double HeightM = 8.0;   // trunk base to crown top
		double RadiusM = 2.0;   // crown radius
		double Lean = 0.0;      // radians away from vertical (wind-shaped coastal palms lean)
		double LeanAim = 0.0;   // which way it leans, radians
		double Spin = 0.0;      // rotation about its own axis, so identical crowns do not line up
		PlantKind Kind = PlantKind::Pine;
		/** 0..1 variation within the species, for the renderer to tint foliage by. */
		double Tint = 0.5;
	};

	/** How thickly things grow, and how far the scatter reaches. */
	struct PlantingRules
	{
		/** Candidate grid spacing in metres: one plant at most per cell. Six metres is a pine stand's pitch. */
		double SpacingM = 6.0;
		/** Clear of a highway carriageway and its verge by this much. */
		double RoadClearM = 11.0;
		/** Kept out of town pads, so streets and lots are not overgrown. Parks are planted anyway. */
		bool bAvoidTowns = true;
		/** The woods round a crossroads community reach this far out from its edge whatever the fields beyond do (LandCoverRules has the same figure). */
		double CrossroadsWoodsM = 220.0;
		/** Understory: shrubs and ferns on a finer grid under the canopy, this close together. Zero for none. */
		double UnderstorySpacingM = 4.5;
		/** How thickly the woods grow: the share of candidate cells in woodland (Sim/LandCover) that get a tree. */
		double WoodlandDensity = 0.6;
		/** A yard is kept in front of the house, this far in from the street's edge, and wooded behind. */
		double YardKeptM = 24.0;
		double YardFrontDensity = 0.05;
		double YardBackDensity = 0.45;
		/** The odd tree on a store's lot and along a road's verge. */
		double CommercialDensity = 0.02;
		double RoadsideDensity = 0.05;
		/** Nothing above this height: the upstate balds and rock. */
		double TreeLineM = 1500.0;
		/** Slopes steeper than this grow scrub at most. */
		double MaxTreeSlope = 0.62;
		/** Hard cap on what one request may return, so a huge area cannot blow the frame budget. */
		std::size_t Limit = 4000;
	};

	/**
	 * Every plant standing in Area. Deterministic in world space: the same rectangle, or any
	 * rectangle covering the same ground, yields the same plants. Cheap enough to call per streamed
	 * terrain chunk -- it samples the surface once per candidate cell and the candidates are a grid.
	 */
	std::vector<Plant> PlantsIn(const StatePlan& Map, const CityPlan& World, const Rect& Area,
		const PlantingRules& Rules = PlantingRules());

	/**
	 * One triangle of a plant's geometry, in world metres, with a flat colour. Plain doubles so the
	 * shape of a tree is decided once, here, and both the engine and the offline renderer used to
	 * check it draw the same thing. Two implementations of a pine would mean verifying one and
	 * shipping the other.
	 */
	struct PlantTri
	{
		double P[3][3] = {};   // three corners, counter-clockwise seen from outside
		double Rgb[3] = {};    // 0..1 linear
	};

	/** Appends the triangles for one plant. Roughly 30-60 of them, depending on the species. */
	void BuildPlant(const Plant& Value, std::vector<PlantTri>& Out);

	/**
	 * Whether a plant could stand at a point at all: on land, off the road, out of the towns. Public
	 * because street furniture and parked cars want the same question answered.
	 */
	bool CanPlantAt(const StatePlan& Map, const CityPlan& World, double X, double Y,
		const PlantingRules& Rules, SurfaceInfo* OutSurface = nullptr);
}
