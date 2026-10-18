// The South Carolina state map: real places in the right order, a road network that reaches every town,
// and terrain that behaves (sea offshore, mountains upstate, level towns, dry and drivable highways).
#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
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
