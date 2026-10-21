// Vegetation: it has to be stable, it has to stay off the road, and it has to belong to the place.
//
// Stability is the one that would actually ruin a drive. Terrain streams in chunks that are built,
// thrown away and built again as the player moves, so if the trees depended on which chunk asked for
// them, a pine would hop two metres sideways every time you drove past it.
#include "Sim/Vegetation.h"

#include "Sim/LandCover.h"

#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace
{
	Rect Around(double X, double Y, double Half)
	{
		return Rect{X - Half, Y - Half, X + Half, Y + Half};
	}

	std::string Key(const Plant& Value)
	{
		// Enough precision to catch a plant that moved at all, not so much that it is noise.
		auto Round = [](double V) { return std::to_string(static_cast<long long>(std::llround(V * 1000.0))); };
		return Round(Value.X) + "," + Round(Value.Y) + "," + Round(Value.HeightM) + "," +
			Round(Value.RadiusM) + "," + std::to_string(static_cast<int>(Value.Kind));
	}
}

// The same ground gives the same trees, however it is asked for: twice over, and in overlapping
// pieces. A chunk boundary must not be visible as a seam in the planting, and a chunk rebuilt after
// being released must come back identical.
DTG_TEST(VegetationIsStableInWorldSpace)
{
	const GameState& State = dtg_test::LoadedState();
	// Somewhere that actually grows things, found rather than assumed: the start town is Longs,
	// which is ten miles from the North Carolina line, so a fixed offset can easily land in the
	// next state or out at sea.
	double X = State.City.SpawnX;
	double Y = State.City.SpawnY;
	bool bFound = false;
	for (int Ring = 1; Ring <= 12 && !bFound; ++Ring)
	{
		for (int Step = 0; Step < 8 && !bFound; ++Step)
		{
			const double Angle = Step * 0.785398;
			const double TryX = State.City.SpawnX + Ring * 600.0 * std::cos(Angle);
			const double TryY = State.City.SpawnY + Ring * 600.0 * std::sin(Angle);
			if (State.Map.InsideState(TryX, TryY) &&
				PlantsIn(State.Map, State.City, Around(TryX, TryY, 300.0)).size() > 20)
			{
				X = TryX;
				Y = TryY;
				bFound = true;
			}
		}
	}
	EXPECT_MSG(bFound, "no patch of ground within seven kilometres of the start grows anything");

	const std::vector<Plant> Once = PlantsIn(State.Map, State.City, Around(X, Y, 300.0));
	const std::vector<Plant> Twice = PlantsIn(State.Map, State.City, Around(X, Y, 300.0));
	EXPECT_MSG(Once.size() == Twice.size(), "asking twice gave a different number of plants");
	EXPECT_MSG(!Once.empty(), "the patch that grew plants a moment ago is now empty");
	for (std::size_t Index = 0; Index < Once.size() && Index < Twice.size(); ++Index)
	{
		EXPECT_MSG(Key(Once[Index]) == Key(Twice[Index]),
			"plant " + std::to_string(Index) + " moved between two identical requests");
	}

	// Now the same ground in four pieces, as four neighbouring chunks would ask for it. Every plant
	// in the whole must appear in exactly one piece, unchanged.
	std::map<std::string, int> Whole;
	for (const Plant& Value : Once)
	{
		++Whole[Key(Value)];
	}
	std::map<std::string, int> Pieces;
	const double Half = 150.0;
	for (const double Dx : {-Half, Half})
	{
		for (const double Dy : {-Half, Half})
		{
			for (const Plant& Value : PlantsIn(State.Map, State.City, Around(X + Dx, Y + Dy, Half)))
			{
				++Pieces[Key(Value)];
			}
		}
	}
	std::size_t Missing = 0;
	std::size_t Doubled = 0;
	for (const auto& Entry : Whole)
	{
		const auto Found = Pieces.find(Entry.first);
		if (Found == Pieces.end())
		{
			++Missing;
		}
		else if (Found->second > 1)
		{
			++Doubled;
		}
	}
	// A plant exactly on a piece boundary can legitimately fall in two pieces, since the rectangles
	// share an edge; what must not happen is a plant vanishing or changing.
	EXPECT_MSG(Missing == 0, std::to_string(Missing) + " of " + std::to_string(Whole.size()) +
		" plants disappear when the same ground is asked for in four pieces -- a seam at every chunk edge");
	EXPECT_MSG(Doubled * 50 < Whole.size() + 50, std::to_string(Doubled) +
		" plants are duplicated across piece boundaries, which is more than edge cases");
}

// Nothing grows on the carriageway, in the water, or in the middle of a town.
DTG_TEST(VegetationKeepsOffTheRoadAndOutOfTown)
{
	const char* States[] = {"state.json", "state-florida.json", "state-northcarolina.json"};
	for (const char* Which : States)
	{
		GameState State;
		ContentReport Report;
		State.LoadContent(dtg_test::LoadAllFiles(Which), Report);
		EXPECT_MSG(Report.Errors.empty(), std::string(Which) + " failed to load");

		std::size_t Checked = 0;
		std::size_t OnRoad = 0;
		std::size_t InWater = 0;
		std::size_t InTown = 0;
		// Along the highways, where a stray tree would be in the windscreen, and through the towns.
		for (const HighwayPlan& Road : State.Map.Highways)
		{
			const double Length = Road.LengthM();
			for (double Along = 0.0; Along < Length; Along += 260.0)
			{
				const std::array<double, 3> Here = Road.At(Along);
				for (const Plant& Value : PlantsIn(State.Map, State.City, Around(Here[0], Here[1], 70.0)))
				{
					++Checked;
					const SurfaceInfo Info = State.Map.SurfaceAt(Value.X, Value.Y);
					if (Info.RoadNearness > 0.0)
					{
						++OnRoad;
					}
					if (Info.bWater)
					{
						++InWater;
					}
				}
			}
		}
		for (const StateCity& City : State.Map.Cities)
		{
			for (const Plant& Value : PlantsIn(State.Map, State.City, Around(City.X, City.Y, 240.0)))
			{
				++Checked;
				const CityInfo* Town = State.City.CityAt(Value.X, Value.Y);
				if (Town == nullptr)
				{
					continue;
				}
				if (Town->Style == CityStyle::Rural)
				{
					// A crossroads community's yards are wooded (CrossroadsLotsAreWooded holds them to the
					// lots); here only its streets count.
					if (State.Map.SurfaceAt(Value.X, Value.Y).RoadNearness > 0.0)
					{
						++OnRoad;
					}
					continue;
				}
				bool bPark = false;
				for (const Rect& Park : State.City.Parks)
				{
					if (Park.Contains(Value.X, Value.Y))
					{
						bPark = true;
						break;
					}
				}
				if (!bPark)
				{
					++InTown;
				}
			}
		}
		EXPECT_MSG(Checked > 0, std::string(Which) + ": nothing grows along any road or in any town");
		EXPECT_MSG(OnRoad == 0, std::string(Which) + ": " + std::to_string(OnRoad) + " of " +
			std::to_string(Checked) + " plants stand on a road");
		EXPECT_MSG(InWater == 0, std::string(Which) + ": " + std::to_string(InWater) +
			" plants stand in open water");
		EXPECT_MSG(InTown == 0, std::string(Which) + ": " + std::to_string(InTown) +
			" plants stand on a town street or lot");
	}
}

// Species belong to their ground: palms on the coast and not in the mountains, cypress near water,
// and every plant a plausible size.
DTG_TEST(VegetationSuitsItsSurroundings)
{
	const char* States[] = {"state.json", "state-florida.json", "state-northcarolina.json"};
	for (const char* Which : States)
	{
		GameState State;
		ContentReport Report;
		State.LoadContent(dtg_test::LoadAllFiles(Which), Report);
		EXPECT_MSG(Report.Errors.empty(), std::string(Which) + " failed to load");

		std::size_t Counts[static_cast<std::size_t>(PlantKind::Count)] = {};
		std::size_t HighPalms = 0;
		std::size_t Total = 0;
		const Rect& Bounds = State.Map.Bounds;
		for (int Row = 0; Row < 12; ++Row)
		{
			for (int Column = 0; Column < 12; ++Column)
			{
				const double X = Bounds.MinX + Bounds.Width() * (Column + 0.5) / 12.0;
				const double Y = Bounds.MinY + Bounds.Depth() * (Row + 0.5) / 12.0;
				for (const Plant& Value : PlantsIn(State.Map, State.City, Around(X, Y, 120.0)))
				{
					++Total;
					++Counts[static_cast<std::size_t>(Value.Kind)];
					EXPECT_MSG(Value.HeightM > 0.3 && Value.HeightM < 40.0,
						std::string(Which) + ": a " + PlantKindName(Value.Kind) + " is " +
							std::to_string(Value.HeightM) + " m tall");
					EXPECT_MSG(Value.RadiusM > 0.2 && Value.RadiusM < 14.0,
						std::string(Which) + ": a " + PlantKindName(Value.Kind) + " is " +
							std::to_string(Value.RadiusM) + " m across");
					EXPECT_MSG(std::fabs(Value.GroundM - State.Map.SurfaceAt(Value.X, Value.Y).GroundM) < 0.01,
						std::string(Which) + ": a " + PlantKindName(Value.Kind) + " floats above the ground");
					if (Value.Kind == PlantKind::Palm && Value.GroundM > 400.0)
					{
						++HighPalms;
					}
				}
			}
		}
		EXPECT_MSG(Total > 200, std::string(Which) + " grew only " + std::to_string(Total) +
			" plants across the whole state");
		EXPECT_MSG(HighPalms == 0, std::string(Which) + " has " + std::to_string(HighPalms) +
			" palm trees up a mountain");
		// Pine is the backbone of both Carolinas and most of inland Florida: it should not be rare among the
		// trees. The understory (scrub on a finer grid under the canopy) is not a tree and is not counted.
		const std::size_t Trees = Total - Counts[static_cast<std::size_t>(PlantKind::Scrub)];
		EXPECT_MSG(Counts[static_cast<std::size_t>(PlantKind::Pine)] * 6 > Trees,
			std::string(Which) + " is only " +
				std::to_string(Counts[static_cast<std::size_t>(PlantKind::Pine)] * 100 / (Trees ? Trees : 1)) +
				"% pine among its trees, which is not the Southeast");
	}
}

// It has to be affordable. Vegetation is generated for every terrain chunk as it streams in, so the
// cost of one chunk's worth is paid inside a frame that is also drawing the world.
DTG_TEST(VegetationForAChunkIsAffordable)
{
	const GameState& State = dtg_test::LoadedState();
	// One chunk is 192 m square.
	const Rect Chunk = Around(State.City.SpawnX + 900.0, State.City.SpawnY + 900.0, 96.0);
	const std::vector<Plant> Grown = PlantsIn(State.Map, State.City, Chunk);
	// Enough to look like country, few enough to draw: a chunk is 36,864 square metres, and a chunk of
	// woods at a pine stand's pitch with its floor is about 1,300 plants.
	EXPECT_MSG(Grown.size() < 1800, "a single chunk grew " + std::to_string(Grown.size()) +
		" plants, which will not fit in a frame");

	// And the limit is honoured, however much ground is asked for at once.
	PlantingRules Tight;
	Tight.Limit = 50;
	const std::vector<Plant> Capped = PlantsIn(State.Map, State.City,
		Around(State.City.SpawnX, State.City.SpawnY, 4000.0), Tight);
	EXPECT_MSG(Capped.size() <= 50, "the planting limit was ignored: " +
		std::to_string(Capped.size()) + " plants");
}

// A crossroads community is houses in the woods: its lots grow trees, off the street and clear of
// the buildings, while a proper town keeps its lots clear.
DTG_TEST(CrossroadsLotsAreWooded)
{
	const GameState& State = dtg_test::LoadedState();
	int Rural = 0;
	int InYards = 0;
	int OnRoads = 0;
	int InBuildings = 0;
	int InTownLots = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		Rural += Town.Style == CityStyle::Rural ? 1 : 0;
		int InThisTown = 0;
		for (const Plant& Value : PlantsIn(State.Map, State.City, Town.Bounds))
		{
			const CityInfo* Where = State.City.CityAt(Value.X, Value.Y);
			if (Where == nullptr || Where->Name != Town.Name)
			{
				continue;
			}
			bool bPark = false;
			for (const Rect& Park : State.City.Parks)
			{
				bPark = bPark || Park.Contains(Value.X, Value.Y);
			}
			if (bPark)
			{
				continue;
			}
			if (Town.Style != CityStyle::Rural)
			{
				++InTownLots;
				continue;
			}
			++InYards;
			++InThisTown;
			if (State.Map.SurfaceAt(Value.X, Value.Y).RoadNearness > 0.0)
			{
				++OnRoads;
			}
			for (const BuildingPlan& Building : State.City.Buildings)
			{
				if (Building.Id < Town.FirstBuildingId || Building.Id > Town.LastBuildingId)
				{
					continue;
				}
				const Rect& Lot = Building.Footprint;
				if (Value.X > Lot.MinX - 2.9 && Value.X < Lot.MaxX + 2.9 && Value.Y > Lot.MinY - 2.9 && Value.Y < Lot.MaxY + 2.9)
				{
					++InBuildings;
				}
			}
		}
		// Wooded, not a tree or two: without the density floor Longs had 53 trees in its yards, with it 205.
		if (Town.Style == CityStyle::Rural)
		{
			EXPECT_MSG(InThisTown >= 150, std::to_string(InThisTown) + " trees in the yards of " + Town.Name + ": not wooded");
		}
	}
	EXPECT_MSG(Rural > 0, "no crossroads community to check");
	EXPECT_MSG(InYards > 8, std::to_string(InYards) + " trees in crossroads yards: the lots are bare");
	EXPECT_MSG(OnRoads == 0, std::to_string(OnRoads) + " yard trees stand on a street");
	EXPECT_MSG(InBuildings == 0, std::to_string(InBuildings) + " yard trees stand in or against a building");
	EXPECT_MSG(InTownLots == 0, std::to_string(InTownLots) + " trees on the lots of towns that keep them clear");
}

namespace detail_vegetation_test
{
	const CityInfo* StartTown(const GameState& State)
	{
		for (const CityInfo& Town : State.City.Cities)
		{
			if (Town.bStart)
			{
				return &Town;
			}
		}
		return nullptr;
	}

	/** A square of one land cover, 500 to 2,500 m from the start town, found by sampling. */
	bool FindSquare(const GameState& State, LandCover Want, double Half, Rect& Out)
	{
		const CityInfo* Longs = StartTown(State);
		if (!Longs)
		{
			return false;
		}
		for (double Reach = 500.0; Reach <= 2500.0; Reach += 150.0)
		{
			for (int Step = 0; Step < 16; ++Step)
			{
				const double Angle = Step * 0.39269908;
				const double X = Longs->Bounds.CenterX() + Reach * std::cos(Angle);
				const double Y = Longs->Bounds.CenterY() + Reach * std::sin(Angle);
				const Rect Square = Around(X, Y, Half);
				const LandCoverMap Cover(State.Map, State.City, Square);
				bool bAll = true;
				for (int J = 0; J <= 4 && bAll; ++J)
				{
					for (int I = 0; I <= 4 && bAll; ++I)
					{
						bAll = Cover.At(Square.MinX + Square.Width() * I / 4.0, Square.MinY + Square.Depth() * J / 4.0) == Want;
					}
				}
				if (bAll)
				{
					Out = Square;
					return true;
				}
			}
		}
		return false;
	}
}

// The woods are thick, at a pine stand's pitch with a floor under them, and a field is open ground.
DTG_TEST(WoodsStandThickAndFieldsLieOpen)
{
	using namespace detail_vegetation_test;
	const GameState& State = dtg_test::LoadedState();
	Rect Woods;
	Rect Field;
	EXPECT_MSG(FindSquare(State, LandCover::Woodland, 60.0, Woods), "no square of woods within 2.5 km of Longs");
	EXPECT_MSG(FindSquare(State, LandCover::Field, 60.0, Field), "no square of field within 2.5 km of Longs");
	int WoodTrees = 0;
	int WoodShrubs = 0;
	for (const Plant& Value : PlantsIn(State.Map, State.City, Woods))
	{
		(Value.Kind == PlantKind::Scrub ? WoodShrubs : WoodTrees) += 1;
	}
	int FieldTrees = 0;
	std::string FieldCovers;
	const LandCoverMap FieldCover(State.Map, State.City, Field);
	for (const Plant& Value : PlantsIn(State.Map, State.City, Field))
	{
		if (Value.Kind != PlantKind::Scrub)
		{
			++FieldTrees;
			FieldCovers += std::string(" ") + LandCoverName(FieldCover.At(Value.X, Value.Y));
		}
	}
	const double Hectares = Woods.Width() * Woods.Depth() / 10000.0;
	// A loblolly stand is a tree every five or six metres: 120 per hectare and up.
	EXPECT_MSG(WoodTrees / Hectares > 100.0, std::to_string(WoodTrees) + " trees on " + std::to_string(Hectares) + " ha of woods: thin");
	EXPECT_MSG(WoodShrubs / Hectares > 80.0, std::to_string(WoodShrubs) + " shrubs on " + std::to_string(Hectares) + " ha of woods: no floor");
	EXPECT_MSG(FieldTrees / Hectares < 5.0, std::to_string(FieldTrees) + " trees on " + std::to_string(Hectares) + " ha of field: not a field; they stand on" + FieldCovers);
}

// A yard is kept in front of the house and wooded behind, and nothing grows on the drive, the pad, the
// walk or the shed: the vegetation respects the parcel.
DTG_TEST(YardsAreKeptInFrontAndWoodedBehind)
{
	using namespace detail_vegetation_test;
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Longs = StartTown(State);
	EXPECT_MSG(Longs != nullptr, "no start town");
	if (!Longs)
	{
		return;
	}
	const LandCoverMap Cover(State.Map, State.City, Longs->Bounds);
	const PlantingRules Rules;
	int Front = 0;
	int Back = 0;
	int OnWorks = 0;
	int Yards = 0;
	for (const Plant& Value : PlantsIn(State.Map, State.City, Longs->Bounds, Rules))
	{
		if (Value.Kind == PlantKind::Scrub)
		{
			continue;
		}
		const LotFeatures* Lot = Cover.LotAt(Value.X, Value.Y);
		if (!Lot || Cover.At(Value.X, Value.Y) != LandCover::Yard)
		{
			continue;
		}
		++Yards;
		(State.Map.StreetEdgeDistance(Value.X, Value.Y) < Rules.YardKeptM ? Front : Back) += 1;
		if (LandCoverMap::OnLotWorks(*Lot, Value.X, Value.Y, 0.3))
		{
			++OnWorks;
		}
	}
	EXPECT_MSG(Yards > 60, std::to_string(Yards) + " trees in Longs' yards");
	EXPECT_MSG(Front * 3 < Back, std::to_string(Front) + " trees in front of the houses against " + std::to_string(Back) + " behind: the front yards are not kept");
	EXPECT_MSG(OnWorks == 0, std::to_string(OnWorks) + " trees stand on a drive, a pad, a walk or a shed");
}

// From a crossroads community's streets the woods are the horizon: a line of sight at eye level out
// of town runs into tree crowns within 450 m in every direction but straight down a road.
DTG_TEST(CrossroadsHorizonIsWooded)
{
	using namespace detail_vegetation_test;
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Longs = StartTown(State);
	EXPECT_MSG(Longs != nullptr, "no start town");
	if (!Longs)
	{
		return;
	}
	const double Cx = Longs->Bounds.CenterX();
	const double Cy = Longs->Bounds.CenterY();
	const double Reach = 450.0;
	PlantingRules Rules;
	Rules.Limit = 400000;
	std::vector<Plant> Trees;
	for (const Plant& Value : PlantsIn(State.Map, State.City, Around(Cx, Cy, Reach + 20.0), Rules))
	{
		if (Value.Kind != PlantKind::Scrub && Value.HeightM > 3.0)
		{
			Trees.push_back(Value);
		}
	}
	int Open = 0;
	int Rays = 0;
	int Explained = 0;
	std::string OpenRays;
	const LandCoverMap Cover(State.Map, State.City, Around(Cx, Cy, Reach + 20.0));
	for (int Step = 0; Step < 36; ++Step)
	{
		const double Angle = Step * 0.1745329252;
		// Straight down a street there is nothing to stop the eye, as there is not in Longs.
		const double Axis = std::fmod(Angle, 1.5707963268);
		if (Axis < 0.11 || Axis > 1.5707963268 - 0.11)
		{
			continue;
		}
		const double Dx = std::cos(Angle);
		const double Dy = std::sin(Angle);
		// Up to where the line of sight leaves open country. The next town's blocks, or water, are what the
		// eye sees then; at the state's compression North Myrtle Beach's blocks begin within 300 m of
		// Longs' centre, and that is the map's doing, not the woods'.
		double Stop = Reach;
		int OnRoads = 0;
		for (double Along = 20.0; Along < Reach; Along += 10.0)
		{
			const LandCover Here = Cover.At(Cx + Along * Dx, Cy + Along * Dy);
			if (Here == LandCover::Town || Here == LandCover::Water)
			{
				Stop = Along;
				break;
			}
			OnRoads += Here == LandCover::Road ? 1 : 0;
		}
		// Down a highway leaving town there is nothing to stop the eye either.
		if (Stop < 300.0 || OnRoads >= 6)
		{
			++Explained;
			continue;
		}
		++Rays;
		int Crowns = 0;
		for (const Plant& Tree : Trees)
		{
			const double Along = (Tree.X - Cx) * Dx + (Tree.Y - Cy) * Dy;
			if (Along < 0.0 || Along > Stop)
			{
				continue;
			}
			const double Across = std::fabs(-(Tree.X - Cx) * Dy + (Tree.Y - Cy) * Dx);
			Crowns += Across < Tree.RadiusM ? 1 : 0;
		}
		if (Crowns < 3)
		{
			++Open;
			// What the line of sight runs over, for the message.
			OpenRays += " " + std::to_string(Step * 10) + " deg (" + std::to_string(Crowns) + " crowns):";
			for (double Along = 60.0; Along <= Reach; Along += 60.0)
			{
				OpenRays += std::string(" ") + LandCoverName(Cover.At(Cx + Along * Dx, Cy + Along * Dy));
			}
			OpenRays += ";";
		}
	}
	// At the state's compression most lines of sight out of Longs meet the next town, water or a highway
	// within 300 m (21 of 32 when this was written); the rest must meet the woods.
	EXPECT_MSG(Rays >= 8, std::to_string(Rays) + " rays checked, " + std::to_string(Explained) + " run into the next town, water or down a highway");
	EXPECT_MSG(Open == 0, std::to_string(Open) + " of " + std::to_string(Rays) + " lines of sight out of Longs cross fewer than three tree crowns:" + OpenRays);
}

// The country round a crossroads community is woods, and the woods have a floor: shrubs under the
// canopy, few in the open.
DTG_TEST(CrossroadsStandInTheWoodsWithAnUnderstory)
{
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Longs = nullptr;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (Town.bStart)
		{
			Longs = &Town;
		}
	}
	EXPECT_MSG(Longs != nullptr, "no start town");
	if (!Longs)
	{
		return;
	}
	// A strip of country 60 to 200 m east of the town's edge, well inside the woods' reach.
	const Rect Country{Longs->Bounds.MaxX + 60.0, Longs->Bounds.MinY, Longs->Bounds.MaxX + 200.0, Longs->Bounds.MaxY};
	int Trees = 0;
	int Shrubs = 0;
	for (const Plant& Value : PlantsIn(State.Map, State.City, Country))
	{
		(Value.Kind == PlantKind::Scrub ? Shrubs : Trees) += 1;
	}
	const double Hectares = Country.Width() * Country.Depth() / 10000.0;
	EXPECT_MSG(Trees / Hectares > 35.0, std::to_string(Trees) + " trees on " + std::to_string(Hectares) + " ha beside Longs: not woods");
	EXPECT_MSG(Shrubs / Hectares > 60.0, std::to_string(Shrubs) + " shrubs on " + std::to_string(Hectares) + " ha beside Longs: no understory");
	// And without the understory pass, no shrubs at all from a finer grid.
	PlantingRules Bare;
	Bare.UnderstorySpacingM = 0.0;
	int BareShrubs = 0;
	for (const Plant& Value : PlantsIn(State.Map, State.City, Country, Bare))
	{
		BareShrubs += Value.Kind == PlantKind::Scrub ? 1 : 0;
	}
	EXPECT_MSG(BareShrubs < Shrubs / 2, "the understory rule is not what grows the shrubs");
}
