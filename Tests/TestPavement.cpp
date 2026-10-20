// Walking routes: an errand keeps to the pavement, crosses a road only straight over at a corner,
// never cuts through a building or over a lawn, and gets to the door without wandering.
#include "Sim/Pavement.h"

#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace detail_pavement_test
{
	using Point = std::array<double, 2>;

	std::string Where(const Point& At)
	{
		return "(" + std::to_string(At[0]) + ", " + std::to_string(At[1]) + ")";
	}

	double ToSegment(const Point& At, const Point& A, const Point& B)
	{
		const double Lx = B[0] - A[0];
		const double Ly = B[1] - A[1];
		const double Len2 = Lx * Lx + Ly * Ly;
		const double T = Len2 > 1e-12 ? std::clamp(((At[0] - A[0]) * Lx + (At[1] - A[1]) * Ly) / Len2, 0.0, 1.0) : 0.0;
		return std::hypot(At[0] - (A[0] + Lx * T), At[1] - (A[1] + Ly * T));
	}

	/** The ring worked out here from the sidewalk width, not taken from the code under test. */
	std::array<Point, 4> RingOf(const Rect& Block)
	{
		const double In = 0.5 * FurnitureRules().SidewalkM;
		return {{{Block.MinX + In, Block.MinY + In}, {Block.MaxX - In, Block.MinY + In}, {Block.MaxX - In, Block.MaxY - In},
			{Block.MinX + In, Block.MaxY - In}}};
	}

	/** What a route has to stay clear of, gathered once from the patch of town the routes cover. */
	struct Town
	{
		std::vector<Rect> Blocks;
		std::vector<Rect> Roads;
		std::vector<Rect> Footprints;
	};

	bool OnRing(const Town& Here, const Point& At)
	{
		for (const Rect& Block : Here.Blocks)
		{
			const std::array<Point, 4> Ring = RingOf(Block);
			for (std::size_t Side = 0; Side < 4; ++Side)
			{
				if (ToSegment(At, Ring[Side], Ring[(Side + 1) % 4]) < 1e-6)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IsRingCorner(const Town& Here, const Point& At)
	{
		for (const Rect& Block : Here.Blocks)
		{
			for (const Point& Corner : RingOf(Block))
			{
				if (std::hypot(At[0] - Corner[0], At[1] - Corner[1]) < 1e-6)
				{
					return true;
				}
			}
		}
		return false;
	}

	/** What is wrong with standing here, or "" when it is a fine place to walk. */
	std::string Trouble(const Town& Here, const Point& At, bool& bInRoad)
	{
		const double Sidewalk = FurnitureRules().SidewalkM;
		for (const Rect& Footprint : Here.Footprints)
		{
			// Strictly inside: a door is on the wall, and the path up to it touches the building there.
			if (At[0] > Footprint.MinX + 0.01 && At[0] < Footprint.MaxX - 0.01 && At[1] > Footprint.MinY + 0.01 && At[1] < Footprint.MaxY - 0.01)
			{
				return "inside a building at " + Where(At);
			}
		}
		for (const Rect& Block : Here.Blocks)
		{
			const double FromKerb = std::min({At[0] - Block.MinX, Block.MaxX - At[0], At[1] - Block.MinY, Block.MaxY - At[1]});
			if (Block.Contains(At[0], At[1]) && FromKerb > Sidewalk + 0.01)
			{
				return "on a lawn " + std::to_string(FromKerb) + " m in from the kerb at " + Where(At);
			}
		}
		for (const Rect& Road : Here.Roads)
		{
			bInRoad = bInRoad || Road.Contains(At[0], At[1]);
		}
		return "";
	}

	/** The first thing wrong with a route from Start to Door, or "" when there is nothing. */
	std::string Inspect(const Town& Here, const std::vector<Point>& Route, const Point& Start, const Point& Door)
	{
		const double Sidewalk = FurnitureRules().SidewalkM;
		if (Route.empty())
		{
			return "no route";
		}
		if (std::hypot(Route.back()[0] - Door[0], Route.back()[1] - Door[1]) > 1e-9)
		{
			return "ends at " + Where(Route.back()) + ", not the door";
		}
		for (std::size_t Index = 0; Index + 1 < Route.size(); ++Index)
		{
			if (!OnRing(Here, Route[Index]))
			{
				return "waypoint " + std::to_string(Index) + " off the pavement at " + Where(Route[Index]);
			}
		}
		double Walked = std::hypot(Route.front()[0] - Start[0], Route.front()[1] - Start[1]);
		for (std::size_t Index = 0; Index + 1 < Route.size(); ++Index)
		{
			const Point& A = Route[Index];
			const Point& B = Route[Index + 1];
			const double Length = std::hypot(B[0] - A[0], B[1] - A[1]);
			Walked += Length;
			const bool bFrontPath = Index + 2 == Route.size();
			bool bInRoad = false;
			const int Steps = std::max(1, static_cast<int>(std::ceil(Length / 0.5)));
			for (int Step = 0; Step <= Steps; ++Step)
			{
				const double T = static_cast<double>(Step) / static_cast<double>(Steps);
				const Point At{A[0] + (B[0] - A[0]) * T, A[1] + (B[1] - A[1]) * T};
				std::string Problem = Trouble(Here, At, bInRoad);
				// The last leg is the path up to the door, over the front yard: a lawn is allowed there.
				if (!Problem.empty() && !(bFrontPath && Problem.rfind("on a lawn", 0) == 0))
				{
					return "leg " + std::to_string(Index) + ": " + Problem;
				}
			}
			if (bFrontPath)
			{
				if (bInRoad || Length > 15.0)
				{
					return "the path to the door is " + std::to_string(Length) + " m" + (bInRoad ? " and crosses a road" : "");
				}
				continue;
			}
			// Over a road only at a corner, and straight over: not mid-block, not diagonally across a junction.
			if (bInRoad && (!IsRingCorner(Here, A) || !IsRingCorner(Here, B)))
			{
				return "leg " + std::to_string(Index) + " crosses a road away from a corner, " + Where(A) + " to " + Where(B);
			}
			if (bInRoad && Length > 18.0 + Sidewalk + 0.01)
			{
				return "leg " + std::to_string(Index) + " crosses " + std::to_string(Length) + " m of road, " + Where(A) + " to " + Where(B);
			}
		}
		// No wandering: no more than a block's width and depth of detour beyond the straight-line grid
		// distance -- round to the far corner of the first block, and back round the last.
		const double Grid = std::fabs(Door[0] - Start[0]) + std::fabs(Door[1] - Start[1]);
		if (Walked > Grid + 120.0)
		{
			return "walks " + std::to_string(Walked) + " m for " + std::to_string(Grid) + " m of grid";
		}
		return "";
	}

	/** The town with the most blocks: the hub, where an errand can cross the most streets. */
	const CityInfo* BiggestTown(const CityPlan& City)
	{
		const CityInfo* Best = nullptr;
		std::size_t Most = 0;
		for (const CityInfo& Candidate : City.Cities)
		{
			std::size_t Count = 0;
			for (const Rect& Block : City.Blocks)
			{
				Count += City.CityAt(Block.CenterX(), Block.CenterY()) == &Candidate ? 1 : 0;
			}
			if (Count > Most)
			{
				Most = Count;
				Best = &Candidate;
			}
		}
		return Best;
	}
}

// Errands across the biggest town, from random spots on the pavement to random doors: every one gets
// there, on the pavements, over the roads only at corners, through no building and over no lawn.
DTG_TEST(ErrandsKeepToThePavement)
{
	using namespace detail_pavement_test;
	const GameState& State = dtg_test::LoadedState();
	const CityPlan& City = State.City;
	const CityInfo* Home = BiggestTown(City);
	EXPECT(Home != nullptr);
	if (!Home)
	{
		return;
	}
	auto InTown = [&](double X, double Y) { return City.CityAt(X, Y) == Home; };
	// Everything a route could brush against, a street's width beyond the town's edge included.
	const Rect Around{Home->Bounds.MinX - 30.0, Home->Bounds.MinY - 30.0, Home->Bounds.MaxX + 30.0, Home->Bounds.MaxY + 30.0};

	Town Here;
	for (const Rect& Block : City.Blocks)
	{
		if (InTown(Block.CenterX(), Block.CenterY()))
		{
			Here.Blocks.push_back(Block);
		}
	}
	for (const RoadPlan& Road : City.Roads)
	{
		if (Road.Area.Overlaps(Around))
		{
			Here.Roads.push_back(Road.Area);
		}
	}
	std::vector<const BuildingPlan*> Doors;
	for (const BuildingPlan& Building : City.Buildings)
	{
		if (Building.Footprint.Overlaps(Around))
		{
			Here.Footprints.push_back(Building.Footprint);
		}
		if (InTown(Building.DoorX, Building.DoorY))
		{
			Doors.push_back(&Building);
		}
	}
	EXPECT_MSG(Here.Blocks.size() >= 16, std::to_string(Here.Blocks.size()) + " blocks in " + Home->Name);
	EXPECT_MSG(Doors.size() >= 40, std::to_string(Doors.size()) + " doors in " + Home->Name);
	if (Here.Blocks.empty() || Doors.empty())
	{
		return;
	}

	Rng Random(20261020);
	int Routed = 0;
	int Crossings = 0;
	for (int Trip = 0; Trip < 300; ++Trip)
	{
		// Anywhere on the pavement ring of a block in town.
		const std::array<Point, 4> Ring = RingOf(Here.Blocks[Random.Index(Here.Blocks.size())]);
		const std::size_t Side = Random.Index(4);
		const double T = Random.NextDouble();
		const Point Start{Lerp(Ring[Side][0], Ring[(Side + 1) % 4][0], T), Lerp(Ring[Side][1], Ring[(Side + 1) % 4][1], T)};
		const BuildingPlan& Goal = *Doors[Random.Index(Doors.size())];
		const Point Door{Goal.DoorX, Goal.DoorY};
		const std::vector<Point> Route = PavementRoute(City, Start[0], Start[1], Door[0], Door[1]);
		const std::string Problem = Inspect(Here, Route, Start, Door);
		EXPECT_MSG(Problem.empty(), "from " + Where(Start) + " to " + Goal.Name + " at " + Where(Door) + ": " + Problem);
		Routed += Problem.empty() ? 1 : 0;
		for (std::size_t Index = 0; Index + 1 < Route.size(); ++Index)
		{
			Crossings += IsRingCorner(Here, Route[Index]) && IsRingCorner(Here, Route[Index + 1]) &&
					std::hypot(Route[Index + 1][0] - Route[Index][0], Route[Index + 1][1] - Route[Index][1]) < 18.0 + FurnitureRules().SidewalkM + 0.01
				? 1
				: 0;
		}
	}
	// Most trips go to another block, so they cross roads: a test of routes that never left the block
	// they started on would prove nothing about crossings.
	EXPECT_MSG(Crossings > 300, std::to_string(Crossings) + " road crossings in " + std::to_string(Routed) + " good routes");
}

// The blocks do not reach another town: there is no walking route there, and the caller is told so
// rather than handed a straight line across the country.
DTG_TEST(NoWalkingRouteToAnotherTown)
{
	const GameState& State = dtg_test::LoadedState();
	const CityPlan& City = State.City;
	const CityInfo* Home = City.CityAt(City.SpawnX, City.SpawnY);
	const BuildingPlan* Near = nullptr;
	const BuildingPlan* Away = nullptr;
	for (const BuildingPlan& Building : City.Buildings)
	{
		const CityInfo* There = City.CityAt(Building.DoorX, Building.DoorY);
		if (There && There == Home && !Near)
		{
			Near = &Building;
		}
		if (There && There != Home && !Away)
		{
			Away = &Building;
		}
	}
	EXPECT(Home != nullptr && Near != nullptr && Away != nullptr);
	if (!Near || !Away)
	{
		return;
	}
	EXPECT(PavementRoute(City, City.SpawnX, City.SpawnY, Away->DoorX, Away->DoorY).empty());
	// And a door in the start town does have one, so the empty answer above is about the other town.
	EXPECT(!PavementRoute(City, City.SpawnX, City.SpawnY, Near->DoorX, Near->DoorY).empty());
}
