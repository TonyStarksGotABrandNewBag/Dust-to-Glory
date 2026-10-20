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
	const std::vector<Prop> Lamps = OfKind(FurnitureIn(State.Map, State.City, Around(State.City.SpawnX, State.City.SpawnY, 900.0)),
		PropKind::StreetLamp);
	EXPECT_MSG(Lamps.size() > 10, std::to_string(Lamps.size()) + " lamps in the start town");
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
	const std::vector<Prop> Hydrants = OfKind(FurnitureIn(State.Map, State.City, Around(State.City.SpawnX, State.City.SpawnY, 900.0)),
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
