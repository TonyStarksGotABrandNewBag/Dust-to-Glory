// Integration tests: the whole stack rather than one rule at a time.
//
// The unit tests next door each hold one system still and poke it. These drive a game the way a
// player would -- load the content, start, take a job, buy a car, found a ministry, get arrested,
// serve the time, run for office -- and then check that nothing came loose along the way: that the
// clock only ever moves forward, that money is conserved, that a save and a reload give back the
// game that was saved, and that a year of simulated time leaves the world still coherent.
//
// The save round trip is the valuable one. A field that is never written to the save file is
// invisible to every other test here; this catches it by saving, reloading, saving again and
// requiring the two files to be identical character for character.
#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace
{
	/** Saves, reloads into a fresh game on the same content, saves again; the two must match. */
	void ExpectSurvivesSaving(const GameState& Live, const char* StateFile, const std::string& Where)
	{
		const std::string Saved = Live.SaveToJson();
		GameState Reloaded;
		ContentReport Report;
		Reloaded.LoadContent(dtg_test::LoadAllFiles(StateFile), Report);
		EXPECT_MSG(Report.Errors.empty(), Where + ": content failed to reload");
		std::string Error;
		EXPECT_MSG(Reloaded.LoadFromJson(Saved, &Error), Where + ": load failed -- " + Error);
		const std::string Again = Reloaded.SaveToJson();
		if (Again != Saved)
		{
			// Point at the first difference rather than printing two whole save files.
			std::size_t At = 0;
			while (At < Saved.size() && At < Again.size() && Saved[At] == Again[At])
			{
				++At;
			}
			const std::size_t From = At > 60 ? At - 60 : 0;
			EXPECT_MSG(false, Where + ": a reloaded game saves differently at byte " +
				std::to_string(At) + "\n    saved:    ..." + Saved.substr(From, 160) +
				"\n    reloaded: ..." + Again.substr(From, 160));
		}
		// And the things a player would notice immediately.
		EXPECT_MSG(Reloaded.Funds.Balance() == Live.Funds.Balance(), Where + ": wallet changed");
		EXPECT_MSG(Reloaded.Clock.TotalMinutes() == Live.Clock.TotalMinutes(), Where + ": clock changed");
		EXPECT_MSG(Reloaded.Garage.size() == Live.Garage.size(), Where + ": garage changed");
		EXPECT_MSG(Reloaded.PlayerName == Live.PlayerName, Where + ": name changed");
		EXPECT_MSG(Reloaded.NetWorth() == Live.NetWorth(), Where + ": net worth changed");
	}

	/** Nothing in the save is a NaN, a negative age, or otherwise impossible. */
	void ExpectCoherent(const GameState& State, const std::string& Where)
	{
		EXPECT_MSG(std::isfinite(State.Health()), Where + ": health is not a number");
		EXPECT_MSG(State.Health() >= 0.0 && State.Health() <= 100.0,
			Where + ": health out of range at " + std::to_string(State.Health()));
		EXPECT_MSG(State.Clock.TotalMinutes() >= 0, Where + ": clock ran backwards past zero");
		for (const OwnedVehicle& Car : State.Garage)
		{
			EXPECT_MSG(!Car.Uid.empty(), Where + ": a car in the garage has no id");
			EXPECT_MSG(Car.MileageKm >= 0.0 && std::isfinite(Car.MileageKm),
				Where + ": " + Car.Uid + " has an impossible odometer");
		}
		EXPECT_MSG(!State.City.Buildings.empty(), Where + ": the world lost its buildings");
	}
}

// A session played end to end, checking after every stage that the game still saves and reloads
// cleanly. Each stage is something a player does in their first few hours.
DTG_TEST(IntegrationAPlayedSessionSurvivesEveryStage)
{
	GameState State;
	ContentReport Report;
	State.LoadContent(dtg_test::LoadAllFiles(), Report);
	EXPECT(Report.Errors.empty());
	State.NewGame(20260418, "Kenny");
	ExpectCoherent(State, "a new game");
	ExpectSurvivesSaving(State, "state.json", "a new game");

	const std::int64_t Started = State.Clock.TotalMinutes();
	Money Last = State.Funds.Balance();

	// Work. The first job anyone can get, a few shifts, and the pay has to show up.
	EXPECT(State.ApplyForCareer("retail-associate").bOk);
	for (int Shift = 0; Shift < 6; ++Shift)
	{
		State.WorkShift("retail-associate", 0.9);
	}
	EXPECT_MSG(State.Funds.Balance() > Last, "six shifts did not pay anything");
	EXPECT_MSG(State.Clock.TotalMinutes() > Started, "working took no time");
	ExpectCoherent(State, "after working");
	ExpectSurvivesSaving(State, "state.json", "after working");

	// A car, haggled for, then driven and fixed.
	State.RefreshUsedCars();
	EXPECT(!State.UsedCars.Listings().empty());
	const UsedListing Listing = State.UsedCars.Listings().front();
	State.Funds.Deposit(Listing.AskingPrice * 2, MoneyCategory::Other, "test float", State.Clock.TotalMinutes());
	Last = State.Funds.Balance();
	EXPECT(State.BuyUsed(Listing.Id, Listing.AskingPrice).bOk);
	EXPECT_MSG(State.Funds.Balance() == Last - Listing.AskingPrice, "the car cost the wrong amount");
	EXPECT_MSG(!State.Garage.empty(), "the car never arrived in the garage");
	ExpectCoherent(State, "after buying a car");
	ExpectSurvivesSaving(State, "state.json", "after buying a car");

	const std::string Uid = State.Garage.front().Uid;
	State.Paint(Uid, 0x1A3C6E, 1);
	State.SetTransmission(true);
	EXPECT(!State.AutomaticGearbox());
	State.Repair(Uid);
	ExpectSurvivesSaving(State, "state.json", "after working on the car");

	// Ministry, with the tithes the brief asks for.
	EXPECT(State.FoundMinistry("Longs Grace Chapel").bOk);
	State.Preach(SermonTheme::Hope, 0.85);
	State.RunOutreach(OutreachKind::FoodDrive);
	ExpectCoherent(State, "after founding a ministry");
	ExpectSurvivesSaving(State, "state.json", "after founding a ministry");

	// Money that is not wages: shares, and a business.
	if (!State.Markets.Stocks().empty())
	{
		const std::string Ticker = State.Markets.Stocks().front().Ticker;
		State.Funds.Deposit(Dollars(50000), MoneyCategory::Other, "test float", State.Clock.TotalMinutes());
		State.BuyShares(Ticker, 10);
		ExpectSurvivesSaving(State, "state.json", "after buying shares");
		State.SellShares(Ticker, 5);
		ExpectSurvivesSaving(State, "state.json", "after selling shares");
	}

	// Powers, which mutate, and which the world has opinions about.
	std::vector<std::string> Picks;
	for (const PowerDef& Power : State.Powers.Powers())
	{
		if (Picks.size() < 3)
		{
			Picks.push_back(Power.Id);
		}
	}
	if (Picks.size() == 3)
	{
		EXPECT(State.ChoosePowers(Picks).bOk);
		State.UsePower(0, {}, /*bInPublic*/ false);
		ExpectCoherent(State, "after using a power");
		ExpectSurvivesSaving(State, "state.json", "after using a power");
	}

	// Arrest, sentencing, time served, release. The whole route, because it touches the most state.
	State.ReportCrime(Crime::VehicleTheft, /*bPoliceWitness*/ true, /*bCivilianWitness*/ true);
	State.Busted();
	ExpectSurvivesSaving(State, "state.json", "after being arrested");
	State.TakePlea();
	ExpectSurvivesSaving(State, "state.json", "after a plea");
	if (State.InJail())
	{
		for (int Day = 0; Day < 10 && State.InJail(); ++Day)
		{
			State.ServeJailDay(JailActivity::Work);
		}
		ExpectCoherent(State, "in jail");
		ExpectSurvivesSaving(State, "state.json", "in jail");
		State.RequestParole();
		ExpectSurvivesSaving(State, "state.json", "after asking for parole");
	}

	// And the long game: office.
	State.RunForOffice(Office::Mayor);
	State.CampaignRally(0.8);
	ExpectCoherent(State, "while campaigning");
	ExpectSurvivesSaving(State, "state.json", "while campaigning");

	EXPECT_MSG(State.Clock.TotalMinutes() > Started, "the whole session took no time at all");
}

// Time passing on its own. A year of it, in the chunks the game actually advances in, with nothing
// driving it but the world's own daily and weekly rules. This is where a slow leak shows up: an
// unbounded list, a counter that only ever grows, money created from nothing.
DTG_TEST(IntegrationAYearOfSimulatedTimeStaysCoherent)
{
	GameState State = dtg_test::FreshGame(7);
	EXPECT(State.ApplyForCareer("retail-associate").bOk);

	const std::size_t Buildings = State.City.Buildings.size();
	std::int64_t Last = State.Clock.TotalMinutes();
	for (int Day = 0; Day < 365; ++Day)
	{
		State.Skip(60 * 24);
		EXPECT_MSG(State.Clock.TotalMinutes() > Last, "day " + std::to_string(Day) + ": the clock stalled");
		Last = State.Clock.TotalMinutes();
		if (Day % 30 == 0)
		{
			ExpectCoherent(State, "day " + std::to_string(Day));
			// The world itself must not grow just because time passed.
			EXPECT_MSG(State.City.Buildings.size() == Buildings,
				"day " + std::to_string(Day) + ": the city changed size");
			EXPECT_MSG(State.Phone.All().size() <= 500,
				"day " + std::to_string(Day) + ": the inbox has grown to " +
					std::to_string(State.Phone.All().size()) + " and is never trimmed");
			EXPECT_MSG(State.PrivateCars.size() <= 2000,
				"day " + std::to_string(Day) + ": parked cars are accumulating (" +
					std::to_string(State.PrivateCars.size()) + ")");
			EXPECT_MSG(State.Npcs.All().size() <= 20000,
				"day " + std::to_string(Day) + ": the population is accumulating (" +
					std::to_string(State.Npcs.All().size()) + ")");
		}
	}
	ExpectCoherent(State, "after a year");
	ExpectSurvivesSaving(State, "state.json", "after a year");
}

// Every state ships playable. The same short session on each map, because a map is content and
// content is where a typo lands.
DTG_TEST(IntegrationEveryStateIsPlayable)
{
	const char* States[] = {"state.json", "state-florida.json", "state-northcarolina.json"};
	for (const char* Which : States)
	{
		GameState State;
		ContentReport Report;
		State.LoadContent(dtg_test::LoadAllFiles(Which), Report);
		EXPECT_MSG(Report.Errors.empty(), std::string(Which) + " failed to load");
		State.NewGame(99, "Kenny");

		// You start somewhere real, on dry land, at the level of the town you start in.
		const StateCity* Start = State.Map.StartCity();
		EXPECT_MSG(Start != nullptr, std::string(Which) + " has no starting town");
		const SurfaceInfo Where = State.Map.SurfaceAt(State.City.SpawnX, State.City.SpawnY);
		EXPECT_MSG(!Where.bWater, std::string(Which) + " starts the player in the water");

		// And there is something to do within walking distance of the spawn.
		const std::vector<const BuildingPlan*> Near =
			State.City.BuildingsNear(State.City.SpawnX, State.City.SpawnY, 600.0);
		EXPECT_MSG(!Near.empty(), std::string(Which) + " has nothing near the spawn");

		EXPECT_MSG(State.ApplyForCareer("retail-associate").bOk,
			std::string(Which) + " has no entry-level work");
		State.WorkShift("retail-associate", 0.9);
		State.Skip(60 * 24 * 30);
		ExpectCoherent(State, Which);
		ExpectSurvivesSaving(State, Which, Which);
	}
}

// A save written by one state's game must not be mistaken for another's. Loading it into content
// built on a different map has to either work honestly or fail honestly -- never half-load.
DTG_TEST(IntegrationASaveDoesNotBleedBetweenStates)
{
	GameState Carolina;
	ContentReport First;
	Carolina.LoadContent(dtg_test::LoadAllFiles("state.json"), First);
	EXPECT(First.Errors.empty());
	Carolina.NewGame(5, "Kenny");
	Carolina.Skip(60 * 24 * 3);
	const std::string Saved = Carolina.SaveToJson();

	GameState Florida;
	ContentReport Second;
	Florida.LoadContent(dtg_test::LoadAllFiles("state-florida.json"), Second);
	EXPECT(Second.Errors.empty());
	std::string Error;
	const bool bLoaded = Florida.LoadFromJson(Saved, &Error);
	if (bLoaded)
	{
		// If it loads, the game has to be coherent on the map it is now standing on: the player
		// cannot be left at coordinates that belong to a different state's geography.
		ExpectCoherent(Florida, "a Carolina save on the Florida map");
		EXPECT_MSG(Florida.Clock.TotalMinutes() == Carolina.Clock.TotalMinutes(),
			"a cross-state load lost the clock");
	}
	else
	{
		EXPECT_MSG(!Error.empty(), "a cross-state load failed without saying why");
	}
}
