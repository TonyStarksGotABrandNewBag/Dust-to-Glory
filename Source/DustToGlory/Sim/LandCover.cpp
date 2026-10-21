#include "Sim/LandCover.h"

#include <vector>

namespace dtg::sim
{
	const char* LandCoverName(LandCover Cover)
	{
		switch (Cover)
		{
		case LandCover::Water: return "water";
		case LandCover::Road: return "road";
		case LandCover::Street: return "street";
		case LandCover::Yard: return "yard";
		case LandCover::Commercial: return "commercial";
		case LandCover::Roadside: return "roadside";
		case LandCover::Woodland: return "woodland";
		case LandCover::Field: return "field";
		case LandCover::Town: return "town";
		case LandCover::Count: break;
		}
		return "land";
	}

	bool NearCrossroads(const CityPlan& World, double X, double Y, double Reach)
	{
		for (const CityInfo& Town : World.Cities)
		{
			if (Town.Style != CityStyle::Rural)
			{
				continue;
			}
			const Rect& B = Town.Bounds;
			if (X > B.MinX - Reach && X < B.MaxX + Reach && Y > B.MinY - Reach && Y < B.MaxY + Reach)
			{
				return true;
			}
		}
		return false;
	}

	namespace detail_landcover
	{
		bool OnStreet(const CityPlan& World, double X, double Y)
		{
			for (const RoadPlan& Road : World.Roads)
			{
				if (Road.Area.Contains(X, Y))
				{
					return true;
				}
			}
			return false;
		}
	}

	LandCoverMap::LandCoverMap(const StatePlan& InMap, const CityPlan& InWorld, const Rect& Area, const LandCoverRules& InRules)
		: Map(InMap), World(InWorld), Settings(InRules)
	{
		// The lots that could claim a point in the area: a lot is at most a block across, so a block's
		// reach either side is enough.
		const double Reach = 120.0;
		const Rect Wider{Area.MinX - Reach, Area.MinY - Reach, Area.MaxX + Reach, Area.MaxY + Reach};
		Properties = LotsIn(Map, World, Wider);
	}

	const LotFeatures* LandCoverMap::LotAt(double X, double Y) const
	{
		for (const LotFeatures& Lot : Properties)
		{
			if (Lot.Lot.Contains(X, Y))
			{
				return &Lot;
			}
		}
		return nullptr;
	}

	bool LandCoverMap::OnLotWorks(const LotFeatures& Lot, double X, double Y, double MarginM)
	{
		auto Within = [&](const Rect& R)
		{
			return X > R.MinX - MarginM && X < R.MaxX + MarginM && Y > R.MinY - MarginM && Y < R.MaxY + MarginM;
		};
		return Within(Lot.Driveway) || Within(Lot.Pad) || Within(Lot.Path) || (Lot.bShed && Within(Lot.Shed));
	}

	LandCover LandCoverMap::At(double X, double Y) const
	{
		if (!Map.InsideState(X, Y))
		{
			return LandCover::Water;
		}
		const SurfaceInfo Info = Map.SurfaceAt(X, Y);
		if (Info.bWater)
		{
			return LandCover::Water;
		}
		// A town's streets, and the shoulder and ditch beside a crossroads community's, are the road's and
		// not the lot's: checked before the lots, and before the highway, which runs through a crossroads
		// community as its main street with a verge that would otherwise swallow the front of every lot
		// on it. The ditch strip is checked outside the town too, since the outer street's far edge lies
		// just beyond the town's bounds.
		const CityInfo* Town = World.CityAt(X, Y);
		if (Town != nullptr && detail_landcover::OnStreet(World, X, Y))
		{
			return LandCover::Street;
		}
		if (Map.StreetEdgeDistance(X, Y) < DitchToM + 0.4)
		{
			return LandCover::Roadside;
		}
		if (Town != nullptr)
		{
			if (Town->Style != CityStyle::Rural)
			{
				return LandCover::Town;
			}
			for (const LotFeatures& Lot : Properties)
			{
				if (!Lot.Lot.Contains(X, Y))
				{
					continue;
				}
				const BuildingPlan* Building = World.FindBuilding(Lot.BuildingId);
				return Building != nullptr && Building->Role == LotRole::Home ? LandCover::Yard : LandCover::Commercial;
			}
			return LandCover::Roadside;
		}
		// Open country. A highway, then the mown verge beside it.
		if (Info.RoadNearness > 0.0)
		{
			return LandCover::Road;
		}
		if (Settings.RoadsideM > 0.0)
		{
			const double Probe = Settings.RoadsideM;
			const double Offsets[4][2] = {{Probe, 0.0}, {-Probe, 0.0}, {0.0, Probe}, {0.0, -Probe}};
			for (const auto& Offset : Offsets)
			{
				if (Map.SurfaceAt(X + Offset[0], Y + Offset[1]).RoadNearness > 0.5)
				{
					return LandCover::Roadside;
				}
			}
		}
		// A crossroads community stands in the woods.
		if (NearCrossroads(World, X, Y, Settings.CrossroadsWoodsM))
		{
			return LandCover::Woodland;
		}
		// Beyond, fields among the woods: cultivated country, not uniform scatter. Not on the coast strip.
		if (Map.CoastDistance(X, Y) > Settings.FieldCoastM)
		{
			const double Stand = 0.5 + 0.5 * Map.Noise(X, Y, Settings.FieldWavelengthM);
			if (Stand < Settings.FieldAbove)
			{
				return LandCover::Field;
			}
		}
		return LandCover::Woodland;
	}
}
