// Street furniture: stable as the terrain streams, on the sidewalk rather than in the street, clear of
// the carriageway, and barriers only where a car could actually go over the edge.
#include "Sim/StreetFurniture.h"

#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace detail_furniture_test
{
	Rect Around(double X, double Y, double Half)
	{
		return Rect{X - Half, Y - Half, X + Half, Y + Half};
	}

	std::string Key(const Prop& Value)
	{
		auto Round = [](double V) { return std::to_string(static_cast<long long>(std::llround(V * 1000.0))); };
		return Round(Value.X) + "," + Round(Value.Y) + "," + Round(Value.Z) + "," + std::to_string(static_cast<int>(Value.Kind)) + "," +
			std::to_string(Value.Variant);
	}

	std::map<std::string, int> Census(const std::vector<Prop>& Props)
	{
		std::map<std::string, int> Out;
		for (const Prop& Value : Props)
		{
			++Out[Key(Value)];
		}
		return Out;
	}

	std::vector<Prop> OfKind(const std::vector<Prop>& Props, PropKind Kind)
	{
		std::vector<Prop> Out;
		std::copy_if(Props.begin(), Props.end(), std::back_inserter(Out), [Kind](const Prop& Value) { return Value.Kind == Kind; });
		return Out;
	}

	/** Distance from a point to a rectangle: zero inside it. */
	double ToRect(const Rect& Area, double X, double Y)
	{
		const double Dx = std::max({Area.MinX - X, 0.0, X - Area.MaxX});
		const double Dy = std::max({Area.MinY - Y, 0.0, Y - Area.MaxY});
		return std::hypot(Dx, Dy);
	}

	double ToSegment(double X, double Y, double Ax, double Ay, double Bx, double By)
	{
		const double Lx = Bx - Ax;
		const double Ly = By - Ay;
		const double Len2 = Lx * Lx + Ly * Ly;
		const double T = Len2 > 1e-12 ? std::clamp(((X - Ax) * Lx + (Y - Ay) * Ly) / Len2, 0.0, 1.0) : 0.0;
		return std::hypot(X - (Ax + Lx * T), Y - (Ay + Ly * T));
	}

	/** Somewhere along the highways whose deck stands well clear of the ground, if the state has one. */
	bool FindRaisedDeck(const GameState& State, double& OutX, double& OutY)
	{
		const FurnitureRules Rules;
		for (const HighwayPlan& Road : State.City.Highways)
		{
			const double Length = Road.LengthM();
			for (double Along = 0.0; Along < Length; Along += 25.0)
			{
				const std::array<double, 3> Here = Road.At(Along);
				if (!Road.DeckM.empty() && Here[2] - State.Map.NaturalHeight(Here[0], Here[1]) > Rules.ElevatedM + 1.0)
				{
					OutX = Here[0];
					OutY = Here[1];
					return true;
				}
			}
		}
		return false;
	}
}

// The same ground gives the same props however it is asked for -- whole, or as two chunks -- so a
// streamed chunk boundary never shows as a seam and a lamp never hops when its chunk reloads.
DTG_TEST(FurnitureIsStableInWorldSpace)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	const double X = State.City.SpawnX;
	const double Y = State.City.SpawnY;
	FurnitureRules Rules;
	Rules.Limit = 1000000;
	const Rect Whole = Around(X, Y, 900.0);
	const std::vector<Prop> All = FurnitureIn(State.Map, State.City, Whole, Rules);
	EXPECT_MSG(All.size() > 30, std::to_string(All.size()) + " props around the start");
	EXPECT(Census(All) == Census(FurnitureIn(State.Map, State.City, Whole, Rules)));

	const double Split = X + 37.3; // not on any spacing grid
	std::vector<Prop> Halves = FurnitureIn(State.Map, State.City, Rect{Whole.MinX, Whole.MinY, Split, Whole.MaxY}, Rules);
	const std::vector<Prop> Right = FurnitureIn(State.Map, State.City, Rect{Split, Whole.MinY, Whole.MaxX, Whole.MaxY}, Rules);
	Halves.insert(Halves.end(), Right.begin(), Right.end());
	EXPECT_MSG(Census(Halves) == Census(All), std::to_string(Halves.size()) + " in halves vs " + std::to_string(All.size()) + " whole");
}

// Lamps stand on a block's sidewalk, just in from the kerb of the street they light -- not out in a
// cross street at a junction, and not on a lawn.
DTG_TEST(LampsStandOnSidewalksBesideStreets)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	const FurnitureRules Rules;
	const std::vector<Prop> Lamps = OfKind(FurnitureIn(State.Map, State.City, Around(State.City.FindCity("Myrtle Beach")->Bounds.CenterX(), State.City.FindCity("Myrtle Beach")->Bounds.CenterY(), 900.0)),
		PropKind::StreetLamp);
	// Sampled round the hub, not the start town: a crossroads community lights its junctions only, and at
	// ten real metres to the game metre nothing else is within 900 m of the spawn.
	EXPECT_MSG(Lamps.size() > 10, std::to_string(Lamps.size()) + " lamps in Myrtle Beach");
	for (const Prop& Lamp : Lamps)
	{
		double Nearest = 1e18;
		bool bOnStreet = false;
		for (const RoadPlan& Road : State.City.Roads)
		{
			Nearest = std::min(Nearest, ToRect(Road.Area, Lamp.X, Lamp.Y));
			bOnStreet = bOnStreet || Road.Area.Contains(Lamp.X, Lamp.Y);
		}
		bool bOnBlock = false;
		for (const Rect& Block : State.City.Blocks)
		{
			bOnBlock = bOnBlock || Block.Contains(Lamp.X, Lamp.Y);
		}
		EXPECT_MSG(!bOnStreet, "lamp in the street at " + Key(Lamp));
		EXPECT_MSG(bOnBlock, "lamp off the blocks at " + Key(Lamp));
		EXPECT_MSG(std::fabs(Nearest - Rules.LampSetbackM) < 0.01, "lamp " + std::to_string(Nearest) + " m from the kerb");
		EXPECT_NEAR(Lamp.Z, State.Map.HeightAt(Lamp.X, Lamp.Y) + Rules.KerbM, 1e-6);
	}
	// Both sides of the street are lit, on streets running both ways: four facings. Lamps on one side
	// quietly dropped (the mutation test found this gap) would leave two.
	std::map<long long, int> Facings;
	for (const Prop& Lamp : Lamps)
	{
		++Facings[std::llround(Lamp.Yaw * 100.0)];
	}
	EXPECT_MSG(Facings.size() == 4, std::to_string(Facings.size()) + " lamp facings");
}

// One hydrant to a block, on a corner, just in from the kerb.
DTG_TEST(HydrantsOnePerBlockOnACorner)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	const FurnitureRules Rules;
	const std::vector<Prop> Hydrants = OfKind(FurnitureIn(State.Map, State.City, Around(State.City.FindCity("Myrtle Beach")->Bounds.CenterX(), State.City.FindCity("Myrtle Beach")->Bounds.CenterY(), 900.0)),
		PropKind::Hydrant);
	EXPECT(!Hydrants.empty());
	std::map<std::size_t, int> PerBlock;
	for (const Prop& Hydrant : Hydrants)
	{
		bool bFound = false;
		for (std::size_t Index = 0; Index < State.City.Blocks.size(); ++Index)
		{
			const Rect& Block = State.City.Blocks[Index];
			if (!Block.Contains(Hydrant.X, Hydrant.Y))
			{
				continue;
			}
			bFound = true;
			++PerBlock[Index];
			const double FromX = std::min(Hydrant.X - Block.MinX, Block.MaxX - Hydrant.X);
			const double FromY = std::min(Hydrant.Y - Block.MinY, Block.MaxY - Hydrant.Y);
			EXPECT_MSG(std::fabs(FromX - Rules.LampSetbackM) < 0.01 && std::fabs(FromY - Rules.LampSetbackM) < 0.01,
				"hydrant not on a corner at " + Key(Hydrant));
		}
		EXPECT_MSG(bFound, "hydrant off the blocks at " + Key(Hydrant));
	}
	for (const auto& [Block, Count] : PerBlock)
	{
		EXPECT_MSG(Count == 1, std::to_string(Count) + " hydrants on block " + std::to_string(Block));
	}
}

// A trash can per block, on the sidewalk at the corner across from the hydrant: never two on a block,
// never on the hydrant's side on either axis, and BinSetbackM in from both kerbs.
DTG_TEST(BinsOnePerBlockAcrossFromTheHydrant)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	const FurnitureRules Rules;
	const std::vector<Prop> All = FurnitureIn(State.Map, State.City, Around(State.City.FindCity("Myrtle Beach")->Bounds.CenterX(), State.City.FindCity("Myrtle Beach")->Bounds.CenterY(), 900.0));
	const std::vector<Prop> Bins = OfKind(All, PropKind::Bin);
	const std::vector<Prop> Hydrants = OfKind(All, PropKind::Hydrant);
	EXPECT(!Bins.empty());
	std::map<std::size_t, int> PerBlock;
	int Compared = 0;
	for (const Prop& Bin : Bins)
	{
		bool bFound = false;
		for (std::size_t Index = 0; Index < State.City.Blocks.size(); ++Index)
		{
			const Rect& Block = State.City.Blocks[Index];
			if (!Block.Contains(Bin.X, Bin.Y))
			{
				continue;
			}
			bFound = true;
			++PerBlock[Index];
			const double FromX = std::min(Bin.X - Block.MinX, Block.MaxX - Bin.X);
			const double FromY = std::min(Bin.Y - Block.MinY, Block.MaxY - Bin.Y);
			EXPECT_MSG(std::fabs(FromX - Rules.BinSetbackM) < 0.01 && std::fabs(FromY - Rules.BinSetbackM) < 0.01,
				"bin not on a corner at " + Key(Bin));
			for (const Prop& Hydrant : Hydrants)
			{
				if (Block.Contains(Hydrant.X, Hydrant.Y))
				{
					const bool bSameX = (Hydrant.X < Block.CenterX()) == (Bin.X < Block.CenterX());
					const bool bSameY = (Hydrant.Y < Block.CenterY()) == (Bin.Y < Block.CenterY());
					EXPECT_MSG(!bSameX && !bSameY, "bin on the hydrant's side at " + Key(Bin));
					++Compared;
				}
			}
		}
		EXPECT_MSG(bFound, "bin off the blocks at " + Key(Bin));
	}
	EXPECT_MSG(Compared > 0, "no block had both a bin and a hydrant to compare");
	for (const auto& [Block, Count] : PerBlock)
	{
		EXPECT_MSG(Count == 1, std::to_string(Count) + " bins on block " + std::to_string(Block));
	}
}

// Street trees stand on the lawn strip just inside the sidewalk, round every block that has a lawn:
// never on the sidewalk itself, never inside a building, and on both axes of the block.
DTG_TEST(StreetTreesLineTheLawns)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	const FurnitureRules Rules;
	// In the hub: the start town is a crossroads community, which has no street trees at all.
	const CityInfo* Hub = nullptr;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (Town.Style == CityStyle::Hub)
		{
			Hub = &Town;
		}
	}
	EXPECT_MSG(Hub != nullptr, "no hub city");
	if (!Hub)
	{
		return;
	}
	const std::vector<Prop> Trees = OfKind(FurnitureIn(State.Map, State.City, Around(Hub->Bounds.CenterX(), Hub->Bounds.CenterY(), 400.0)),
		PropKind::StreetTree);
	EXPECT_MSG(Trees.size() > 20, std::to_string(Trees.size()) + " street trees in the hub");
	int AlongX = 0; // trees on a block's north or south edge
	int AlongY = 0; // on its east or west edge
	for (const Prop& Tree : Trees)
	{
		const Rect* Home = nullptr;
		for (const Rect& Block : State.City.Blocks)
		{
			if (Block.Contains(Tree.X, Tree.Y))
			{
				Home = &Block;
			}
		}
		EXPECT_MSG(Home != nullptr, "tree off the blocks at " + Key(Tree));
		if (!Home)
		{
			continue;
		}
		const double FromX = std::min(Tree.X - Home->MinX, Home->MaxX - Tree.X);
		const double FromY = std::min(Tree.Y - Home->MinY, Home->MaxY - Tree.Y);
		const double FromEdge = std::min(FromX, FromY);
		(FromY < FromX ? AlongX : AlongY) += 1;
		EXPECT_MSG(std::fabs(FromEdge - Rules.TreeSetbackM) < 0.01, "tree " + std::to_string(FromEdge) + " m from the kerb at " + Key(Tree));
		for (const BuildingPlan& Building : State.City.Buildings)
		{
			EXPECT_MSG(!Building.Footprint.Contains(Tree.X, Tree.Y), "tree inside " + Building.Name + " at " + Key(Tree));
		}
	}
	// Round the block, not just along two sides of it.
	EXPECT_MSG(AlongX > 0 && AlongY > 0, std::to_string(AlongX) + " along X edges, " + std::to_string(AlongY) + " along Y");
}

// The lines run pole top to pole top, one spacing at a time, and stop where a pole is missing.
DTG_TEST(WiresFollowThePoles)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	FurnitureRules Rules;
	Rules.Limit = 1000000;
	// Somewhere with a run of poles: the first place out from the start that has a few.
	Rect Where = Around(State.City.SpawnX, State.City.SpawnY, 400.0);
	bool bFound = false;
	for (int Ring = 1; Ring <= 8 && !bFound; ++Ring)
	{
		for (int Step = 0; Step < 8 && !bFound; ++Step)
		{
			const double Angle = Step * 0.785398;
			const Rect Try = Around(State.City.SpawnX + Ring * 2000.0 * std::cos(Angle), State.City.SpawnY + Ring * 2000.0 * std::sin(Angle), 400.0);
			if (OfKind(FurnitureIn(State.Map, State.City, Try, Rules), PropKind::UtilityPole).size() >= 6)
			{
				Where = Try;
				bFound = true;
			}
		}
	}
	EXPECT(bFound);
	const std::vector<WireSpan> Spans = WiresIn(State.Map, State.City, Where, Rules);
	// Poles from a wider area, so the far end of a span at the edge can be matched.
	const Rect Wider{Where.MinX - 100.0, Where.MinY - 100.0, Where.MaxX + 100.0, Where.MaxY + 100.0};
	const std::vector<Prop> Poles = OfKind(FurnitureIn(State.Map, State.City, Wider, Rules), PropKind::UtilityPole);
	EXPECT_MSG(Spans.size() >= 4, std::to_string(Spans.size()) + " spans among " + std::to_string(Poles.size()) + " poles");
	auto OnAPole = [&](double X, double Y, double Z)
	{
		for (const Prop& Pole : Poles)
		{
			if (std::hypot(Pole.X - X, Pole.Y - Y) < 0.05 && std::fabs(Pole.Z + Rules.PoleHeightM - Z) < 0.05)
			{
				return true;
			}
		}
		return false;
	};
	for (const WireSpan& Span : Spans)
	{
		EXPECT_MSG(OnAPole(Span.Ax, Span.Ay, Span.Az), "span starts off a pole at " + std::to_string(Span.Ax) + "," + std::to_string(Span.Ay));
		EXPECT_MSG(OnAPole(Span.Bx, Span.By, Span.Bz), "span ends off a pole at " + std::to_string(Span.Bx) + "," + std::to_string(Span.By));
		const double Run = std::hypot(Span.Bx - Span.Ax, Span.By - Span.Ay);
		EXPECT_MSG(Run > 0.5 * Rules.PoleSpacingM && Run < Rules.PoleSpacingM + 0.5, "span of " + std::to_string(Run) + " m");
	}
}

// Grass grows on the lawns: inside the sidewalk ring, outside the buildings, and the same clumps however
// the ground is asked for.
DTG_TEST(GrassStaysOnTheLawns)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	FurnitureRules Rules;
	Rules.Limit = 1000000; // the cap is per streamed chunk; this asks for a whole neighbourhood
	const Rect Whole = Around(State.City.SpawnX, State.City.SpawnY, 300.0);
	const std::vector<Prop> Clumps = GroundCoverIn(State.Map, State.City, Whole, Rules);
	EXPECT_MSG(Clumps.size() > 100, std::to_string(Clumps.size()) + " clumps near the start");
	for (const Prop& Clump : Clumps)
	{
		const Rect* Home = nullptr;
		for (const Rect& Block : State.City.Blocks)
		{
			if (Block.Contains(Clump.X, Clump.Y))
			{
				Home = &Block;
			}
		}
		EXPECT_MSG(Home != nullptr, "grass off the blocks at " + Key(Clump));
		if (Home)
		{
			const double FromEdge = std::min({Clump.X - Home->MinX, Home->MaxX - Clump.X, Clump.Y - Home->MinY, Home->MaxY - Clump.Y});
			EXPECT_MSG(FromEdge >= Rules.SidewalkM - 0.01, "grass on the sidewalk at " + Key(Clump));
		}
		for (const BuildingPlan& Building : State.City.Buildings)
		{
			EXPECT_MSG(!Building.Footprint.Contains(Clump.X, Clump.Y), "grass inside " + Building.Name + " at " + Key(Clump));
		}
	}
	const double Split = State.City.SpawnX + 13.7;
	std::vector<Prop> Halves = GroundCoverIn(State.Map, State.City, Rect{Whole.MinX, Whole.MinY, Split, Whole.MaxY}, Rules);
	const std::vector<Prop> Right = GroundCoverIn(State.Map, State.City, Rect{Split, Whole.MinY, Whole.MaxX, Whole.MaxY}, Rules);
	Halves.insert(Halves.end(), Right.begin(), Right.end());
	EXPECT_MSG(Census(Halves) == Census(Clumps), std::to_string(Halves.size()) + " in halves vs " + std::to_string(Clumps.size()) + " whole");
}

// Utility poles stand on the verge: never on a carriageway, never in a town, never in the water.
DTG_TEST(PolesStayOffTheCarriageway)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	FurnitureRules Rules;
	Rules.Limit = 1000000;
	std::vector<Prop> Poles;
	// Searched for in every direction: east of the start town is mostly the coast.
	for (int Ring = 1; Ring <= 8 && Poles.size() < 20; ++Ring)
	{
		for (int Step = 0; Step < 8 && Poles.size() < 20; ++Step)
		{
			const double Angle = Step * 0.785398;
			const std::vector<Prop> Found = OfKind(FurnitureIn(State.Map, State.City,
				Around(State.City.SpawnX + Ring * 2000.0 * std::cos(Angle), State.City.SpawnY + Ring * 2000.0 * std::sin(Angle), 1000.0),
				Rules), PropKind::UtilityPole);
			Poles.insert(Poles.end(), Found.begin(), Found.end());
		}
	}
	EXPECT_MSG(Poles.size() >= 20, std::to_string(Poles.size()) + " poles found");
	for (const Prop& Pole : Poles)
	{
		EXPECT_MSG(State.City.CityAt(Pole.X, Pole.Y) == nullptr, "pole in a town at " + Key(Pole));
		EXPECT_MSG(!State.Map.SurfaceAt(Pole.X, Pole.Y).bWater, "pole in the water at " + Key(Pole));
		// On a verge at the setback from some highway's edge -- not anywhere that merely happens to be clear.
		double BestMiss = 1e18;
		for (const HighwayPlan& Road : State.City.Highways)
		{
			for (std::size_t Index = 0; Index + 1 < Road.Points.size(); ++Index)
			{
				const double Gap = ToSegment(Pole.X, Pole.Y, Road.Points[Index][0], Road.Points[Index][1], Road.Points[Index + 1][0],
					Road.Points[Index + 1][1]);
				EXPECT_MSG(Gap > 0.5 * Road.WidthM + 1.0, "pole " + std::to_string(Gap) + " m from " + Road.Name + "'s centreline");
				BestMiss = std::min(BestMiss, std::fabs(Gap - (0.5 * Road.WidthM + Rules.PoleSetbackM)));
			}
		}
		EXPECT_MSG(BestMiss < 0.5, "pole " + std::to_string(BestMiss) + " m off its setback at " + Key(Pole));
	}
}

// Barriers go where the deck is raised, at deck height, and nowhere on the flat.
DTG_TEST(BarriersOnlyWhereTheDeckIsRaised)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	FurnitureRules Rules;
	Rules.Limit = 1000000;
	double X = 0.0;
	double Y = 0.0;
	EXPECT_MSG(FindRaisedDeck(State, X, Y), "no raised deck anywhere in the state");
	const std::vector<Prop> Barriers = OfKind(FurnitureIn(State.Map, State.City, Around(X, Y, 120.0), Rules), PropKind::Barrier);
	EXPECT_MSG(Barriers.size() >= 4, std::to_string(Barriers.size()) + " barriers on the raised deck");
	for (const Prop& Barrier : Barriers)
	{
		EXPECT_MSG(Barrier.Z - State.Map.NaturalHeight(Barrier.X, Barrier.Y) > 0.5 * Rules.ElevatedM,
			"barrier on the flat at " + Key(Barrier));
	}
	// And the start town, which is on level ground, has none.
	EXPECT(OfKind(FurnitureIn(State.Map, State.City, Around(State.City.SpawnX, State.City.SpawnY, 400.0), Rules), PropKind::Barrier).empty());
}

// A crossroads community has no boulevard: its trees are in its yards, and Vegetation plants those.
DTG_TEST(CrossroadsTownsLightOnlyTheirJunctions)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	const FurnitureRules Rules;
	int Checked = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (Town.Style != CityStyle::Rural)
		{
			continue;
		}
		++Checked;
		const std::vector<Prop> Lamps = OfKind(FurnitureIn(State.Map, State.City, Town.Bounds), PropKind::StreetLamp);
		EXPECT_MSG(!Lamps.empty(), "no lamp at all in " + Town.Name);
		// Every lamp stands within one lamp spacing of a cross street, measured to the street's edge, so
		// each junction is lit and the blocks between are dark.
		int Stray = 0;
		for (const Prop& Lamp : Lamps)
		{
			double Nearest = 1e18;
			for (const RoadPlan& Road : State.City.Roads)
			{
				const bool bOnThisRoad = Lamp.X >= Road.Area.MinX - Rules.LampSetbackM - 0.1 && Lamp.X <= Road.Area.MaxX + Rules.LampSetbackM + 0.1 &&
										 Lamp.Y >= Road.Area.MinY - Rules.LampSetbackM - 0.1 && Lamp.Y <= Road.Area.MaxY + Rules.LampSetbackM + 0.1;
				if (bOnThisRoad)
				{
					continue;
				}
				const double Dx = std::max({Road.Area.MinX - Lamp.X, 0.0, Lamp.X - Road.Area.MaxX});
				const double Dy = std::max({Road.Area.MinY - Lamp.Y, 0.0, Lamp.Y - Road.Area.MaxY});
				Nearest = std::min(Nearest, std::hypot(Dx, Dy));
			}
			if (Nearest > Rules.LampSpacingM)
			{
				++Stray;
			}
		}
		EXPECT_MSG(Stray == 0, std::to_string(Stray) + " of " + std::to_string(Lamps.size()) + " lamps stand along a block in " + Town.Name);
	}
	EXPECT_MSG(Checked > 0, "no crossroads community to check");
}

DTG_TEST(CrossroadsTownsHaveNoStreetTrees)
{
	using namespace detail_furniture_test;
	const GameState& State = dtg_test::LoadedState();
	int Checked = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (Town.Style != CityStyle::Rural)
		{
			continue;
		}
		++Checked;
		const std::vector<Prop> Trees = OfKind(FurnitureIn(State.Map, State.City, Town.Bounds), PropKind::StreetTree);
		EXPECT_MSG(Trees.empty(), std::to_string(Trees.size()) + " street trees in " + Town.Name);
	}
	EXPECT_MSG(Checked > 0, "no crossroads community to check");
}
