// The South Carolina state map: real places in the right order, a road network that reaches every town,
// and terrain that behaves (sea offshore, mountains upstate, level towns, dry and drivable highways).
#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <chrono>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace
{
	const StatePlan& Map()
	{
		return dtg_test::LoadedState().Map;
	}

	double Distance(const StateCity& A, const StateCity& B)
	{
		return std::hypot(A.X - B.X, A.Y - B.Y);
	}

	/** Every intersection reachable from a starting one through the road graph. */
	std::set<int> Reachable(const CityPlan& World, int From)
	{
		std::set<int> Seen;
		std::vector<int> Stack{From};
		while (!Stack.empty())
		{
			const int Id = Stack.back();
			Stack.pop_back();
			if (!Seen.insert(Id).second)
			{
				continue;
			}
			if (const Intersection* Node = World.FindIntersection(Id))
			{
				Stack.insert(Stack.end(), Node->Links.begin(), Node->Links.end());
			}
		}
		return Seen;
	}
}

DTG_TEST(StateMapIsSouthCarolinaStartingInLongs)
{
	const GameState& State = dtg_test::LoadedState();
	EXPECT(State.bStateMap);
	EXPECT(Map().Name == "South Carolina");
	EXPECT(Map().Cities.size() >= 20);
	EXPECT(Map().StartCity() && Map().StartCity()->Name == "Longs");
	// A new game wakes up in Longs, in a home that's actually in Longs.
	const CityInfo* Spawn = State.City.CityAt(State.City.SpawnX, State.City.SpawnY);
	EXPECT(Spawn && Spawn->Name == "Longs");
	const BuildingPlan* Home = State.City.FindBuilding(State.City.StarterHomeId);
	EXPECT(Home != nullptr);
	if (Home)
	{
		const CityInfo* HomeTown = State.City.CityAt(Home->DoorX, Home->DoorY);
		EXPECT(HomeTown && HomeTown->Name == "Longs");
		EXPECT(Home->Address.find("Longs") != std::string::npos);
	}
	// Real towns, each built at its own size, with the landmarks the state file gives them.
	for (const char* Name : {"Myrtle Beach", "North Myrtle Beach", "Little River", "Conway", "Loris", "Charleston", "Columbia", "Greenville",
			 "Hilton Head Island"})
	{
		EXPECT_MSG(State.City.FindCity(Name) != nullptr, Name);
	}
	const CityInfo* Hub = State.City.FindCity("Myrtle Beach");
	EXPECT(Hub && Hub->Style == CityStyle::Hub);
	int HubDealers = 0;
	for (const BuildingPlan* Dealer : State.City.WithRole(LotRole::Dealership))
	{
		HubDealers += (Hub && State.City.CityAt(Dealer->DoorX, Dealer->DoorY) == Hub) ? 1 : 0;
	}
	// The motor mile: every new-car showroom (one per dealer group) is in the hub.
	EXPECT(HubDealers >= 30 && HubDealers == static_cast<int>(State.City.WithRole(LotRole::Dealership).size()));
	bool bConwayJail = false;
	for (const BuildingPlan* Jail : State.City.WithRole(LotRole::Jail))
	{
		const CityInfo* Town = State.City.CityAt(Jail->DoorX, Jail->DoorY);
		bConwayJail = bConwayJail || (Town && Town->Name == "Conway");
	}
	EXPECT(bConwayJail);
	// A fresh game greets the player in Longs.
	GameState Fresh;
	ContentReport Report;
	Fresh.LoadContent(dtg_test::LoadAllFiles(), Report);
	Fresh.NewGame(11, "Tester");
	bool bGreeted = false;
	for (const std::string& Note : Fresh.TakeNotifications())
	{
		bGreeted = bGreeted || Note.find("Welcome to Longs, SC") != std::string::npos;
	}
	EXPECT(bGreeted);
}

DTG_TEST(CitiesKeepTheirPlaceWithoutOverlapping)
{
	const StatePlan& State = Map();
	for (std::size_t A = 0; A < State.Cities.size(); ++A)
	{
		const StateCity& City = State.Cities[A];
		EXPECT_MSG(State.InsideState(City.Bounds.MinX, City.Bounds.MinY) && State.InsideState(City.Bounds.MaxX, City.Bounds.MinY) &&
				State.InsideState(City.Bounds.MinX, City.Bounds.MaxY) && State.InsideState(City.Bounds.MaxX, City.Bounds.MaxY),
			City.Name + " pokes outside the state");
		for (std::size_t B = A + 1; B < State.Cities.size(); ++B)
		{
			EXPECT_MSG(!City.Bounds.Overlaps(State.Cities[B].Bounds), City.Name + " overlaps " + State.Cities[B].Name);
		}
	}
	const StateCity* Longs = State.FindCity("Longs");
	const StateCity* MyrtleBeach = State.FindCity("Myrtle Beach");
	const StateCity* Charleston = State.FindCity("Charleston");
	const StateCity* Columbia = State.FindCity("Columbia");
	const StateCity* Greenville = State.FindCity("Greenville");
	EXPECT(Longs && MyrtleBeach && Charleston && Columbia && Greenville);
	if (!(Longs && MyrtleBeach && Charleston && Columbia && Greenville))
	{
		return;
	}
	// Longs stays where it really is; the world's origin.
	EXPECT(std::hypot(Longs->X, Longs->Y) < 100.0);
	// Compass directions survive (X east, Y south): Charleston is south-west of Myrtle Beach, Greenville north-west of Columbia.
	EXPECT(Charleston->X < MyrtleBeach->X && Charleston->Y > MyrtleBeach->Y);
	EXPECT(Greenville->X < Columbia->X && Greenville->Y < Columbia->Y);
	EXPECT(MyrtleBeach->Y > Longs->Y);
	// Distances are the real ones divided by the map scale (Columbia to Charleston is about 160 km).
	EXPECT_NEAR(Distance(*Columbia, *Charleston) * State.Scale / 1000.0, 160.0, 25.0);
	EXPECT_NEAR(Distance(*Columbia, *Greenville) * State.Scale / 1000.0, 155.0, 25.0);
	// Venues sit clear of every town.
	for (const VenuePlan& Venue : State.VenueLayouts)
	{
		for (const StateCity& City : State.Cities)
		{
			EXPECT_MSG(!Venue.Area.Overlaps(City.Bounds), Venue.Name + " overlaps " + City.Name);
		}
	}
	EXPECT(State.VenueLayouts.size() == 4);
}

DTG_TEST(HighwaysJoinEveryTown)
{
	const GameState& Game = dtg_test::LoadedState();
	const CityPlan& World = Game.City;
	std::set<std::string> Names;
	for (const HighwayPlan& Road : World.Highways)
	{
		Names.insert(Road.Name);
		EXPECT(Road.Points.size() >= 2);
	}
	for (const char* Name : {"I-95", "I-26", "I-20", "I-85", "I-77", "US-17", "US-501", "SC-9", "SC-31"})
	{
		EXPECT_MSG(Names.count(Name) == 1, Name);
	}
	const CityInfo* Longs = World.FindCity("Longs");
	EXPECT(Longs != nullptr);
	if (!Longs)
	{
		return;
	}
	const std::set<int> FromLongs = Reachable(World, World.NearestIntersection(Longs->Bounds.CenterX(), Longs->Bounds.CenterY()));
	for (const CityInfo& City : World.Cities)
	{
		EXPECT_MSG(FromLongs.count(World.NearestIntersection(City.Bounds.CenterX(), City.Bounds.CenterY())) == 1, "can't drive from Longs to " + City.Name);
	}
	for (const VenuePlan& Venue : World.Venues)
	{
		const std::array<double, 2> Gate = VenueEntrance(Venue);
		EXPECT_MSG(FromLongs.count(World.NearestIntersection(Gate[0], Gate[1])) == 1, "can't drive to " + Venue.Name);
	}
	// Highways don't plough through towns they don't serve.
	const StatePlan& State = Game.Map;
	for (const HighwayPlan& Road : State.Highways)
	{
		for (std::size_t Index = 0; Index + 1 < Road.Points.size(); ++Index)
		{
			for (int Step = 1; Step < 20; ++Step)
			{
				const double T = Step / 20.0;
				const double X = Road.Points[Index][0] + (Road.Points[Index + 1][0] - Road.Points[Index][0]) * T;
				const double Y = Road.Points[Index][1] + (Road.Points[Index + 1][1] - Road.Points[Index][1]) * T;
				for (const StateCity& City : State.Cities)
				{
					const Rect Inner{City.Bounds.MinX + 20.0, City.Bounds.MinY + 20.0, City.Bounds.MaxX - 20.0, City.Bounds.MaxY - 20.0};
					EXPECT_MSG(!Inner.Contains(X, Y), Road.Name + " runs through " + City.Name);
				}
			}
		}
	}
}

DTG_TEST(TerrainFollowsTheGeography)
{
	const StatePlan& State = Map();
	// Offshore from Myrtle Beach is the Atlantic: below sea level, and water.
	const Point2 Offshore = State.ToWorld(-78.75, 33.60);
	EXPECT(State.IsOcean(Offshore[0], Offshore[1]));
	const SurfaceInfo Sea = State.SurfaceAt(Offshore[0], Offshore[1]);
	EXPECT(Sea.bWater && Sea.bOcean && Sea.GroundM < 0.0);
	// Across the borders is land (North Carolina, Georgia), not sea.
	const Point2 NorthCarolina = State.ToWorld(-80.0, 35.3);
	const Point2 Georgia = State.ToWorld(-82.0, 33.0);
	EXPECT(!State.InsideState(NorthCarolina[0], NorthCarolina[1]) && !State.IsOcean(NorthCarolina[0], NorthCarolina[1]));
	EXPECT(!State.InsideState(Georgia[0], Georgia[1]) && !State.IsOcean(Georgia[0], Georgia[1]));
	// The upstate mountains rise well above the coastal plain.
	double Peak = 0.0;
	for (double Lon = -83.0; Lon < -82.3; Lon += 0.02)
	{
		for (double Lat = 34.85; Lat < 35.15; Lat += 0.02)
		{
			const Point2 Where = State.ToWorld(Lon, Lat);
			if (State.InsideState(Where[0], Where[1]))
			{
				Peak = std::max(Peak, State.NaturalHeight(Where[0], Where[1]));
			}
		}
	}
	EXPECT(Peak > 80.0);
	EXPECT(State.FindCity("Greenville")->LevelM > State.FindCity("Columbia")->LevelM);
	EXPECT(State.FindCity("Columbia")->LevelM > State.FindCity("Charleston")->LevelM);
	EXPECT(State.FindCity("Charleston")->LevelM < 5.0);
	// Every town is built on perfectly level ground.
	for (const StateCity& City : State.Cities)
	{
		double Low = 1e9;
		double High = -1e9;
		for (int I = 0; I <= 12; ++I)
		{
			for (int J = 0; J <= 12; ++J)
			{
				const double Height = State.HeightAt(City.Bounds.MinX + City.Bounds.Width() * I / 12.0, City.Bounds.MinY + City.Bounds.Depth() * J / 12.0);
				Low = std::min(Low, Height);
				High = std::max(High, Height);
				EXPECT(!State.SurfaceAt(City.Bounds.MinX + City.Bounds.Width() * I / 12.0, City.Bounds.MinY + City.Bounds.Depth() * J / 12.0).bWater);
			}
		}
		EXPECT_MSG(High - Low < 0.001, City.Name + " isn't level");
		EXPECT_NEAR(Low, City.LevelM, 0.001);
	}
	// Lakes hold water above their beds.
	for (const StateLake& Lake : State.Lakes)
	{
		const SurfaceInfo Info = State.SurfaceAt(Lake.X, Lake.Y);
		EXPECT_MSG(Info.bWater && !Info.bOcean && Info.WaterM > Info.GroundM + 1.0, Lake.Name);
	}
}

DTG_TEST(RoadsStayDryAndDrivable)
{
	const StatePlan& State = Map();
	int Bridged = 0;
	for (const HighwayPlan& Road : State.Highways)
	{
		EXPECT(!Road.DeckM.empty());
		const double Length = Road.LengthM();
		double Previous = Road.At(0.0)[2];
		for (double Along = 5.0; Along <= Length; Along += 5.0)
		{
			const std::array<double, 3> Here = Road.At(Along);
			const SurfaceInfo Info = State.SurfaceAt(Here[0], Here[1]);
			if (Info.bWater)
			{
				++Bridged;
				EXPECT_MSG(Here[2] >= Info.WaterM + 2.5, Road.Name + " dips into the water");
			}
			else
			{
				EXPECT_MSG(Here[2] >= Info.GroundM - 0.01, Road.Name + " is buried");
			}
			// No steps or walls anywhere: every deck is tapered to a 6% ramp, bridges included.
			EXPECT_MSG(std::fabs(Here[2] - Previous) / 5.0 <= 0.0601, Road.Name + " is too steep");
			EXPECT_NEAR(State.RoadHeightAt(Here[0], Here[1]), Here[2], 0.25);
			Previous = Here[2];
		}
	}
	EXPECT(Bridged > 0); // rivers and lakes are crossed on bridges
}

DTG_TEST(FloridaShipsAsASecondState)
{
	// The generator takes a state file, so a second state is data rather than code. This is Florida's.
	GameState Sunshine;
	ContentReport Report;
	const bool bOk = Sunshine.LoadContent(dtg_test::LoadAllFiles("state-florida.json"), Report);
	for (const std::string& Error : Report.Errors)
	{
		EXPECT_MSG(false, Error);
	}
	EXPECT(bOk);
	Sunshine.NewGame(5, "Tester");

	const StatePlan& Map = Sunshine.Map;
	EXPECT(Map.Name == "Florida");
	EXPECT(Map.Cities.size() >= 20);
	EXPECT(Map.StartCity() && Map.StartCity()->Name == "Jacksonville");
	for (const char* Name : {"Miami", "Tampa", "Orlando", "Key West", "Tallahassee", "Pensacola", "Naples"})
	{
		EXPECT_MSG(Sunshine.City.FindCity(Name) != nullptr, Name);
	}
	// Towns stay on land and off each other, as in any state.
	for (std::size_t A = 0; A < Map.Cities.size(); ++A)
	{
		const StateCity& City = Map.Cities[A];
		EXPECT_MSG(Map.InsideState(City.Bounds.MinX, City.Bounds.MinY) && Map.InsideState(City.Bounds.MaxX, City.Bounds.MaxY) &&
				Map.InsideState(City.Bounds.MinX, City.Bounds.MaxY) && Map.InsideState(City.Bounds.MaxX, City.Bounds.MinY),
			City.Name + " is in the sea");
		for (std::size_t B = A + 1; B < Map.Cities.size(); ++B)
		{
			EXPECT_MSG(!City.Bounds.Overlaps(Map.Cities[B].Bounds), City.Name + " overlaps " + Map.Cities[B].Name);
		}
	}
	// A peninsula has sea on both sides and land only to the north -- which is what tells the two apart.
	const Point2 Atlantic = Map.ToWorld(-79.80, 27.50);
	const Point2 Gulf = Map.ToWorld(-84.50, 27.50);
	const Point2 Georgia = Map.ToWorld(-83.00, 31.60);
	EXPECT(Map.IsOcean(Atlantic[0], Atlantic[1]));
	EXPECT(Map.IsOcean(Gulf[0], Gulf[1]));
	EXPECT(!Map.IsOcean(Georgia[0], Georgia[1]) && !Map.InsideState(Georgia[0], Georgia[1]));
	// Florida is flat: nowhere in it is a mountain.
	for (const StateCity& City : Map.Cities)
	{
		EXPECT_MSG(City.LevelM < 30.0, City.Name + " is improbably high for Florida");
	}
	// And you can still drive everywhere.
	const CityInfo* Start = Sunshine.City.FindCity("Jacksonville");
	EXPECT(Start != nullptr);
	if (!Start)
	{
		return;
	}
	const std::set<int> Reached = Reachable(Sunshine.City, Sunshine.City.NearestIntersection(Start->Bounds.CenterX(), Start->Bounds.CenterY()));
	for (const CityInfo& City : Sunshine.City.Cities)
	{
		EXPECT_MSG(Reached.count(Sunshine.City.NearestIntersection(City.Bounds.CenterX(), City.Bounds.CenterY())) == 1,
			"can't drive from Jacksonville to " + City.Name);
	}
	// US-1 to Key West really does run over open water, so it has to be a causeway rather than a road
	// that disappears into the Atlantic.
	int Causeway = 0;
	for (const HighwayPlan& Road : Map.Highways)
	{
		EXPECT(!Road.DeckM.empty());
		const double Length = Road.LengthM();
		for (double Along = 5.0; Along <= Length; Along += 20.0)
		{
			const std::array<double, 3> Here = Road.At(Along);
			const SurfaceInfo Info = Map.SurfaceAt(Here[0], Here[1]);
			if (Info.bWater)
			{
				Causeway += Info.bOcean ? 1 : 0;
				EXPECT_MSG(Here[2] >= Info.WaterM + 2.5, Road.Name + " dips into the water");
			}
			else
			{
				EXPECT_MSG(Here[2] >= Info.GroundM - 0.01, Road.Name + " is buried");
			}
		}
	}
	EXPECT(Causeway > 0);
}

DTG_TEST(NorthCarolinaIsTheHardOne)
{
	// A third state, and the one that asks the most of the generator: sea level on the Outer Banks to
	// 2,037 m on Mount Mitchell, with interstates that have to cross the Blue Ridge and still be
	// drivable. South Carolina tops out at 1,085 m and Florida is flat, so this is where the terrain
	// and the highway grader are actually tested.
	GameState Tarheel;
	ContentReport Report;
	const bool bOk = Tarheel.LoadContent(dtg_test::LoadAllFiles("state-northcarolina.json"), Report);
	for (const std::string& Error : Report.Errors)
	{
		EXPECT_MSG(false, Error);
	}
	EXPECT(bOk);
	Tarheel.NewGame(9, "Tester");

	const StatePlan& Map = Tarheel.Map;
	EXPECT(Map.Name == "North Carolina");
	EXPECT(Map.Cities.size() >= 24);
	// You start in the mountains this time, which is a different opening entirely.
	EXPECT(Map.StartCity() && Map.StartCity()->Name == "Boone");
	for (const char* Name : {"Charlotte", "Raleigh", "Asheville", "Wilmington", "Nags Head", "Cherokee"})
	{
		EXPECT_MSG(Tarheel.City.FindCity(Name) != nullptr, Name);
	}

	// Towns on land and off each other, as in any state.
	for (std::size_t A = 0; A < Map.Cities.size(); ++A)
	{
		const StateCity& City = Map.Cities[A];
		EXPECT_MSG(Map.InsideState(City.Bounds.MinX, City.Bounds.MinY) && Map.InsideState(City.Bounds.MaxX, City.Bounds.MaxY) &&
				Map.InsideState(City.Bounds.MinX, City.Bounds.MaxY) && Map.InsideState(City.Bounds.MaxX, City.Bounds.MinY),
			City.Name + " is in the sea");
		for (std::size_t B = A + 1; B < Map.Cities.size(); ++B)
		{
			EXPECT_MSG(!City.Bounds.Overlaps(Map.Cities[B].Bounds), City.Name + " overlaps " + Map.Cities[B].Name);
		}
	}

	// The state actually rises. Boone and Asheville are mountain towns and the coast is at sea level,
	// which is the whole reason this file exists.
	const CityInfo* Mountain = Tarheel.City.FindCity("Boone");
	const CityInfo* Coast = Tarheel.City.FindCity("Nags Head");
	EXPECT(Mountain != nullptr && Coast != nullptr);
	if (Mountain && Coast)
	{
		const StateCity* High = Map.FindCity("Boone");
		const StateCity* Low = Map.FindCity("Nags Head");
		EXPECT(High && Low);
		if (High && Low)
		{
			// World metres: 1:25 with a 3x vertical exaggeration, so Mount Mitchell's 2,037 m is
			// about 244 of the ground actually driven on.
			EXPECT_MSG(High->LevelM > 25.0, "Boone is not in the mountains");
			EXPECT_MSG(Low->LevelM < 10.0, "Nags Head is not at the sea");
			EXPECT(High->LevelM > Low->LevelM + 20.0);
		}
	}
	// And somewhere in it is genuinely high ground.
	double Highest = 0.0;
	for (double Lon = -84.2; Lon <= -81.0; Lon += 0.1)
	{
		for (double Lat = 35.1; Lat <= 36.5; Lat += 0.1)
		{
			const Point2 At = Map.ToWorld(Lon, Lat);
			Highest = std::max(Highest, Map.NaturalHeight(At[0], At[1]));
		}
	}
	// World metres again: 2,037 m of Mount Mitchell at 1:25 with this state's 1.5x exaggeration is
	// about 122 on the map, and the ridges around it a good deal less.
	EXPECT_MSG(Highest > 100.0, "the Blue Ridge is missing");

	// The Atlantic is east of the Outer Banks, and Tennessee is land rather than sea.
	const Point2 Atlantic = Map.ToWorld(-75.1, 35.6);
	const Point2 Tennessee = Map.ToWorld(-84.8, 35.8);
	EXPECT(Map.IsOcean(Atlantic[0], Atlantic[1]));
	EXPECT(!Map.IsOcean(Tennessee[0], Tennessee[1]) && !Map.InsideState(Tennessee[0], Tennessee[1]));

	// You can still drive everywhere, mountains and all.
	const CityInfo* Start = Tarheel.City.FindCity("Boone");
	EXPECT(Start != nullptr);
	if (!Start)
	{
		return;
	}
	const std::set<int> Reached = Reachable(Tarheel.City, Tarheel.City.NearestIntersection(Start->Bounds.CenterX(), Start->Bounds.CenterY()));
	for (const CityInfo& City : Tarheel.City.Cities)
	{
		EXPECT_MSG(Reached.count(Tarheel.City.NearestIntersection(City.Bounds.CenterX(), City.Bounds.CenterY())) == 1,
			"can't drive from Boone to " + City.Name);
	}

	// Every road stays above the ground it crosses and out of the water, and -- the point of this
	// state -- no grade is steeper than a road can be built.
	double Steepest = 0.0;
	std::string Worst;
	for (const HighwayPlan& Road : Map.Highways)
	{
		EXPECT(!Road.DeckM.empty());
		const double Length = Road.LengthM();
		double LastZ = Road.At(5.0)[2];
		for (double Along = 25.0; Along <= Length; Along += 20.0)
		{
			const std::array<double, 3> Here = Road.At(Along);
			const SurfaceInfo Info = Map.SurfaceAt(Here[0], Here[1]);
			if (Info.bWater)
			{
				EXPECT_MSG(Here[2] >= Info.WaterM + 2.5, Road.Name + " dips into the water");
			}
			else
			{
				EXPECT_MSG(Here[2] >= Info.GroundM - 0.01, Road.Name + " is buried");
			}
			const double Grade = std::fabs(Here[2] - LastZ) / 20.0;
			if (Grade > Steepest)
			{
				Steepest = Grade;
				Worst = Road.Name;
			}
			LastZ = Here[2];
		}
	}
	// Every road in every state comes out at or under the 6% ramp the decks are tapered to. Before
	// the roads were graded at all this read 56%, and before crossings were treated as overpasses
	// rather than averaged into the hillside, 244%.
	EXPECT_MSG(Steepest <= 0.0601, Worst + " climbs at " + std::to_string(Steepest * 100.0) + "%");
}

// The lookup grid is a speed trick and nothing more: SurfaceAt and RoadHeightAt must return exactly
// what they returned when they walked every road and town in the state for every single sample. A
// cell that misses a segment reaching into it would show up here as a seam in the terrain or a car
// dropping through a bridge, so every state is checked across its whole extent -- and, more densely,
// through its towns and along its roads, where the answers actually vary.
DTG_TEST(StateLookupMatchesExhaustiveSearch)
{
	const char* States[] = {"state.json", "state-florida.json", "state-northcarolina.json"};
	for (const char* Which : States)
	{
		GameState Indexed;
		ContentReport Report;
		Indexed.LoadContent(dtg_test::LoadAllFiles(Which), Report);
		EXPECT_MSG(Report.Errors.empty(), std::string(Which) + " failed to load");

		GameState Exact = Indexed;
		Exact.Map.ForgetLookup();

		std::vector<std::array<double, 2>> Samples;
		const Rect& Bounds = Indexed.Map.Bounds;
		for (int Row = 0; Row <= 40; ++Row)
		{
			for (int Column = 0; Column <= 40; ++Column)
			{
				Samples.push_back({Bounds.MinX + Bounds.Width() * Column / 40.0,
					Bounds.MinY + Bounds.Depth() * Row / 40.0});
			}
		}
		// Through the towns, where the pads are.
		for (const StateCity& City : Indexed.Map.Cities)
		{
			for (int Row = -6; Row <= 6; ++Row)
			{
				for (int Column = -6; Column <= 6; ++Column)
				{
					Samples.push_back({City.X + Column * 180.0, City.Y + Row * 180.0});
				}
			}
		}
		// And along the roads, including off to either side, where the decks and shoulders are.
		for (const HighwayPlan& Road : Indexed.Map.Highways)
		{
			const double Length = Road.LengthM();
			for (double Along = 0.0; Along <= Length; Along += 37.0)
			{
				const std::array<double, 3> Here = Road.At(Along);
				for (const double Offset : {-30.0, -12.0, 0.0, 12.0, 30.0})
				{
					Samples.push_back({Here[0] + Offset, Here[1]});
					Samples.push_back({Here[0], Here[1] + Offset});
				}
			}
		}

		std::size_t Mismatches = 0;
		std::string First;
		for (const std::array<double, 2>& Where : Samples)
		{
			const SurfaceInfo Fast = Indexed.Map.SurfaceAt(Where[0], Where[1]);
			const SurfaceInfo Slow = Exact.Map.SurfaceAt(Where[0], Where[1]);
			const double Road = Indexed.Map.RoadHeightAt(Where[0], Where[1]);
			const double RoadSlow = Exact.Map.RoadHeightAt(Where[0], Where[1]);
			const bool bSame = std::fabs(Fast.GroundM - Slow.GroundM) < 1e-9 &&
				std::fabs(Fast.WaterM - Slow.WaterM) < 1e-9 && Fast.bWater == Slow.bWater &&
				std::fabs(Road - RoadSlow) < 1e-9;
			if (!bSame)
			{
				if (Mismatches == 0)
				{
					First = std::string(Which) + " at " + std::to_string(Where[0]) + "," +
						std::to_string(Where[1]) + ": ground " + std::to_string(Fast.GroundM) + " vs " +
						std::to_string(Slow.GroundM) + ", road " + std::to_string(Road) + " vs " +
						std::to_string(RoadSlow);
				}
				++Mismatches;
			}
		}
		EXPECT_MSG(Mismatches == 0, "lookup grid disagrees with exhaustive search in " +
			std::to_string(Mismatches) + " of " + std::to_string(Samples.size()) + " samples -- " + First);
	}
}

// Load times and the frame budget. Terrain is streamed while the player drives, so a height sample
// is not allowed to drift back into walking the whole road network: a chunk is some four and a half
// thousand samples and a frame is 16.7 ms. The budget here is a count of segments examined rather
// than a stopwatch, so it means the same thing on a busy build machine as on a quiet one.
DTG_TEST(StateHeightQueriesStayCheap)
{
	const char* States[] = {"state.json", "state-florida.json", "state-northcarolina.json"};
	for (const char* Which : States)
	{
		GameState State;
		ContentReport Report;
		State.LoadContent(dtg_test::LoadAllFiles(Which), Report);
		EXPECT_MSG(Report.Errors.empty(), std::string(Which) + " failed to load");

		std::size_t Total = 0;
		for (const HighwayPlan& Road : State.Map.Highways)
		{
			Total += Road.Points.size() > 0 ? Road.Points.size() - 1 : 0;
		}
		EXPECT(Total > 0);

		// A chunk's worth of samples around the spawn, which is where the first frames are spent.
		std::size_t Looked = 0;
		std::size_t Worst = 0;
		const int Cells = 64;
		const double Step = 192.0 / Cells;
		for (int Row = 0; Row <= Cells; ++Row)
		{
			for (int Column = 0; Column <= Cells; ++Column)
			{
				const std::size_t Here = State.Map.RoadCandidatesAt(
					State.City.SpawnX + Column * Step, State.City.SpawnY + Row * Step);
				Looked += Here;
				Worst = std::max(Worst, Here);
			}
		}
		const double Average = static_cast<double>(Looked) /
			static_cast<double>((Cells + 1) * (Cells + 1));
		// Absolute bounds, because what costs a frame is the number of segments examined, not the
		// fraction. Today these read 14.3/18 for South Carolina (its start town is a junction of
		// several routes), 5.0/5 for Florida and 4.0/4 for North Carolina. Drop the lookup grid and
		// the average becomes the whole network -- 69, 75 and 141 -- which is what this guards.
		EXPECT_MSG(Average <= 24.0, std::string(Which) + " examines " + std::to_string(Average) +
			" road segments per height sample, of " + std::to_string(Total));
		EXPECT_MSG(Worst <= 48, std::string(Which) + " has a cell holding " + std::to_string(Worst) +
			" road segments, of " + std::to_string(Total));
		// And on a network big enough for the fraction to mean anything, it has to scale: a bigger
		// state must not cost proportionally more per sample.
		if (Total >= 100)
		{
			EXPECT_MSG(Average < 0.15 * static_cast<double>(Total),
				std::string(Which) + " examines " + std::to_string(Average) + " of " +
					std::to_string(Total) + " segments per sample -- the grid has stopped scaling");
		}
	}
}

// Startup. Generating a whole state -- terrain, towns, every building, the road network and its
// grading -- happens before the player sees anything, so it is measured here. The budget is
// deliberately loose, some thirty times what this costs today (24 ms for South Carolina, 41 for
// Florida, 46 for North Carolina), because a build machine's clock is shared and nobody should be
// chasing a red test over a busy afternoon. It is here to catch an order-of-growth mistake -- a
// generation step that quietly became quadratic in the number of buildings -- not to police
// milliseconds.
DTG_TEST(StateLoadsFastEnoughToStartPlaying)
{
	const char* States[] = {"state.json", "state-florida.json", "state-northcarolina.json"};
	for (const char* Which : States)
	{
		const dtg::sim::ContentFiles Files = dtg_test::LoadAllFiles(Which);
		const std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();
		GameState State;
		ContentReport Report;
		State.LoadContent(Files, Report);
		State.NewGame(7, "Tester");
		const double Elapsed = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - Started).count();
		EXPECT_MSG(Report.Errors.empty(), std::string(Which) + " failed to load");
		EXPECT_MSG(!State.City.Buildings.empty(), std::string(Which) + " generated no buildings");
		EXPECT_MSG(Elapsed < 1500.0, std::string(Which) + " took " + std::to_string(Elapsed) +
			" ms to become playable");
	}
}

// A crossroads community's streets are two lanes with shoulders; the hub's are five lanes wide.
DTG_TEST(RoadWidthFollowsTheTownsStyle)
{
	const GameState& State = dtg_test::LoadedState();
	auto WidthIn = [&](const std::string& Name) -> double
	{
		for (const CityInfo& Town : State.City.Cities)
		{
			if (Town.Name != Name)
			{
				continue;
			}
			for (const RoadPlan& Road : State.City.Roads)
			{
				if (Town.Bounds.Contains(Road.Area.CenterX(), Road.Area.CenterY()))
				{
					return Road.bHorizontal ? Road.Area.Depth() : Road.Area.Width();
				}
			}
		}
		return -1.0;
	};
	EXPECT_MSG(std::fabs(WidthIn("Longs") - 10.0) < 0.01, "Longs' streets are " + std::to_string(WidthIn("Longs")) + " m wide, not 10");
	EXPECT_MSG(std::fabs(WidthIn("Myrtle Beach") - 18.0) < 0.01, "Myrtle Beach's streets are " + std::to_string(WidthIn("Myrtle Beach")) + " m wide, not 18");
}

// A resort strip stands its towers on the oceanfront row and nothing tall behind them, and a
// crossroads community has nothing above two floors: what Longs sees of North Myrtle Beach.
DTG_TEST(BeachTowersStandOnTheOceanfrontOnly)
{
	const GameState& State = dtg_test::LoadedState();
	int Towers = 0;
	int OffTheFront = 0;
	int TallInTheCountry = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		const StateCity* Place = nullptr;
		for (const StateCity& Candidate : State.Map.Cities)
		{
			if (Candidate.Name == Town.Name)
			{
				Place = &Candidate;
			}
		}
		const int Side = Place ? State.Map.CoastSideOf(*Place) : -1;
		for (const BuildingPlan& Building : State.City.Buildings)
		{
			if (Building.Id < Town.FirstBuildingId || Building.Id > Town.LastBuildingId)
			{
				continue;
			}
			if (Town.Style == CityStyle::Rural && Building.Floors > 2)
			{
				++TallInTheCountry;
			}
			if (Town.Style != CityStyle::Beach || Building.Floors <= 4)
			{
				continue;
			}
			++Towers;
			// One row of blocks in from the town's edge, outer road included.
			constexpr double Front = 140.0;
			const Rect& Lot = Building.Footprint;
			const bool bOnFront = (Side == 0 && Lot.MinY < Town.Bounds.MinY + Front) || (Side == 2 && Lot.MaxY > Town.Bounds.MaxY - Front)
				|| (Side == 1 && Lot.MaxX > Town.Bounds.MaxX - Front) || (Side == 3 && Lot.MinX < Town.Bounds.MinX + Front);
			if (!bOnFront)
			{
				++OffTheFront;
			}
		}
	}
	EXPECT_MSG(Towers > 0, "no beach towers at all");
	EXPECT_MSG(OffTheFront == 0, std::to_string(OffTheFront) + " of " + std::to_string(Towers) + " beach towers stand away from the sea");
	EXPECT_MSG(TallInTheCountry == 0, std::to_string(TallInTheCountry) + " buildings in crossroads communities are above two floors");
}

// A crossroads community's buildings are buildings on lots, single-storey bar the church, and never
// the lot-sized boxes the hub's downtown fills its lots with.
DTG_TEST(CrossroadsBuildingsAreSmall)
{
	const GameState& State = dtg_test::LoadedState();
	int Checked = 0;
	int TooBig = 0;
	int TooTall = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (Town.Style != CityStyle::Rural)
		{
			continue;
		}
		for (const BuildingPlan& Building : State.City.Buildings)
		{
			if (Building.Id < Town.FirstBuildingId || Building.Id > Town.LastBuildingId)
			{
				continue;
			}
			++Checked;
			const Rect& Lot = Building.Footprint;
			if (std::max(Lot.Width(), Lot.Depth()) > 26.01)
			{
				++TooBig;
			}
			const bool bTwoStoreys = Building.Role == LotRole::Church || (Building.Role == LotRole::Home && Building.HomeStyle == 3);
			if (Building.Floors > (bTwoStoreys ? 2 : 1))
			{
				++TooTall;
			}
		}
	}
	EXPECT_MSG(Checked > 0, "no crossroads buildings to check");
	EXPECT_MSG(TooBig == 0, std::to_string(TooBig) + " of " + std::to_string(Checked) + " crossroads buildings are over 26 m long");
	EXPECT_MSG(TooTall == 0, std::to_string(TooTall) + " crossroads buildings are taller than a storey");
}

// Longs is sixteen real kilometres from the sea and stands in the country. At the state's compression the
// sea and the next town's blocks must both be far enough off that neither is Longs' horizon (at 25:1 the
// sea was 640 m from its centre and North Myrtle Beach's blocks 300 m), and the beach towns must still
// have the sea at their door.
DTG_TEST(LongsIsInlandAndTheBeachesAreNot)
{
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Longs = State.City.FindCity("Longs");
	EXPECT_MSG(Longs != nullptr, "no Longs");
	if (!Longs)
	{
		return;
	}
	const double Cx = Longs->Bounds.CenterX();
	const double Cy = Longs->Bounds.CenterY();
	const double ToSea = State.Map.CoastDistance(Cx, Cy);
	EXPECT_MSG(ToSea > 1200.0, "the sea is " + std::to_string(static_cast<int>(ToSea)) + " m from Longs' centre: an ocean horizon");
	double Nearest = 1e18;
	std::string Who;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (&Town == Longs)
		{
			continue;
		}
		const double Dx = std::max({Town.Bounds.MinX - Cx, 0.0, Cx - Town.Bounds.MaxX});
		const double Dy = std::max({Town.Bounds.MinY - Cy, 0.0, Cy - Town.Bounds.MaxY});
		const double Distance = std::hypot(Dx, Dy);
		if (Distance < Nearest)
		{
			Nearest = Distance;
			Who = Town.Name;
		}
	}
	EXPECT_MSG(Nearest > 900.0, Who + "'s blocks begin " + std::to_string(static_cast<int>(Nearest)) + " m from Longs' centre");
	// Every beach town has the sea at its door, and the hub at Myrtle Beach stands by the shore.
	int Beaches = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		const bool bHub = Town.Name == "Myrtle Beach";
		if (Town.Style != CityStyle::Beach && !bHub)
		{
			continue;
		}
		Beaches += bHub ? 0 : 1;
		double Closest = 1e18;
		for (double X = Town.Bounds.MinX; X <= Town.Bounds.MaxX + 1.0; X += 50.0)
		{
			for (double Y = Town.Bounds.MinY; Y <= Town.Bounds.MaxY + 1.0; Y += 50.0)
			{
				Closest = std::min(Closest, State.Map.CoastDistance(X, Y));
			}
		}
		// Within 400 m: Hilton Head Island's centre is a real 2.6 km from its ocean beach.
		EXPECT_MSG(Closest < (bHub ? 600.0 : 400.0), Town.Name + " stands " + std::to_string(static_cast<int>(Closest)) + " m from the sea");
	}
	EXPECT_MSG(Beaches > 0, "no beach town");
}

// The shore is a slope, not a cliff: from twenty metres inland to twenty out to sea at Myrtle Beach the
// ground never steps more than a hand's width in a metre. A town's pad a metre above the sea ran to
// the water's edge, and the terrain grid drew the step as a sawtooth.
DTG_TEST(TheShoreIsASlopeNotACliff)
{
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Hub = State.City.FindCity("Myrtle Beach");
	EXPECT_MSG(Hub != nullptr, "no Myrtle Beach");
	if (!Hub)
	{
		return;
	}
	// The nearest dry sand to the hub's centre with the sea 120 m off in some direction, as the tour finds it.
	int Checked = 0;
	double Worst = 0.0;
	std::string Where;
	for (double Radius = 50.0; Radius < 4000.0 && Checked == 0; Radius += 50.0)
	{
		for (int Step = 0; Step < 36 && Checked == 0; ++Step)
		{
			const double Angle = 2.0 * 3.14159265358979 * Step / 36.0;
			const double X = Hub->Bounds.CenterX() + Radius * std::cos(Angle);
			const double Y = Hub->Bounds.CenterY() + Radius * std::sin(Angle);
			const SurfaceInfo Info = State.Map.SurfaceAt(X, Y);
			if (Info.bWater || Info.GroundM > 1.2 || State.Map.CoastDistance(X, Y) > 40.0)
			{
				continue;
			}
			for (int Look = 0; Look < 16 && Checked == 0; ++Look)
			{
				const double Heading = 2.0 * 3.14159265358979 * Look / 16.0;
				const double Dx = std::cos(Heading);
				const double Dy = std::sin(Heading);
				const SurfaceInfo Out = State.Map.SurfaceAt(X + 120.0 * Dx, Y + 120.0 * Dy);
				if (!(Out.bWater && Out.bOcean))
				{
					continue;
				}
				double Last = State.Map.SurfaceAt(X - 20.0 * Dx, Y - 20.0 * Dy).GroundM;
				for (double Along = -19.0; Along <= 20.0; Along += 1.0)
				{
					const double Here = State.Map.SurfaceAt(X + Along * Dx, Y + Along * Dy).GroundM;
					if (std::fabs(Here - Last) > Worst)
					{
						Worst = std::fabs(Here - Last);
						Where = std::to_string(static_cast<int>(Along)) + " m along from the sand at (" + std::to_string(static_cast<int>(X)) + ", " + std::to_string(static_cast<int>(Y)) + ")";
					}
					Last = Here;
					++Checked;
				}
			}
		}
	}
	EXPECT_MSG(Checked > 0, "no shore found at Myrtle Beach");
	EXPECT_MSG(Worst < 0.15, "the shore steps " + std::to_string(Worst) + " m in a metre at " + Where);
}

// A street of houses is not one house: four kinds, the double-wides long and narrow, the farmhouses two
// storeys, and never all of one kind in a crossroads community.
DTG_TEST(HomesComeInKinds)
{
	const GameState& State = dtg_test::LoadedState();
	int Kinds[4] = {0, 0, 0, 0};
	int WrongShape = 0;
	int WrongHeight = 0;
	for (const BuildingPlan& Building : State.City.Buildings)
	{
		if (Building.Role != LotRole::Home)
		{
			continue;
		}
		EXPECT_MSG(Building.HomeStyle >= 0 && Building.HomeStyle <= 3, "a home of kind " + std::to_string(Building.HomeStyle));
		if (Building.HomeStyle < 0 || Building.HomeStyle > 3)
		{
			continue;
		}
		++Kinds[Building.HomeStyle];
		const double Short = std::min(Building.Footprint.Width(), Building.Footprint.Depth());
		const double Long = std::max(Building.Footprint.Width(), Building.Footprint.Depth());
		if (Building.HomeStyle == 2 && (Short > 9.0 || Long < 17.0))
		{
			++WrongShape;
		}
		if ((Building.HomeStyle == 3) != (Building.Floors == 2))
		{
			++WrongHeight;
		}
	}
	for (int Kind = 0; Kind < 4; ++Kind)
	{
		EXPECT_MSG(Kinds[Kind] > 0, "no homes of kind " + std::to_string(Kind));
	}
	EXPECT_MSG(WrongShape == 0, std::to_string(WrongShape) + " double-wides are not long and narrow");
	EXPECT_MSG(WrongHeight == 0, std::to_string(WrongHeight) + " homes have the wrong number of storeys for their kind");
}
