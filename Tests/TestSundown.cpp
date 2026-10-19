// Cheats, and the mode behind the last one.
//
// The test that matters most here is the daylight invariant. Sundown's entire design rests on one
// claim -- that the open ground is safe while the sun is up and nowhere is safe once it is not --
// and if that claim is false anywhere in the day, the mode has no shape. So it is checked across
// every minute of the day rather than at a few sampled hours.
#include "Sim/Cheats.h"
#include "Sim/Sundown.h"

#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace
{
	constexpr std::int64_t MinutesPerDay = 24 * 60;
}

// Every cheat is reachable by its word and by its number, and no two collide.
DTG_TEST(CheatsAreReachableBothWays)
{
	const std::vector<CheatDef>& Cheats = AllCheats();
	EXPECT_MSG(Cheats.size() == static_cast<std::size_t>(Cheat::Count),
		"the cheat table has " + std::to_string(Cheats.size()) + " entries for " +
			std::to_string(static_cast<int>(Cheat::Count)) + " cheats");

	std::set<std::string> Codes;
	std::set<std::string> Dials;
	for (std::size_t Index = 0; Index < Cheats.size(); ++Index)
	{
		const CheatDef& Def = Cheats[Index];
		// The table must be in enum order, or FindCheatDef and the save indices disagree.
		EXPECT_MSG(static_cast<std::size_t>(Def.Which) == Index,
			std::string(Def.Code) + " is out of order in the cheat table");
		EXPECT_MSG(Def.Code[0] != '\0', "a cheat has no code");
		EXPECT_MSG(Def.Title[0] != '\0', std::string(Def.Code) + " has no title");
		EXPECT_MSG(Def.Effect[0] != '\0', std::string(Def.Code) + " does not say what it does");

		EXPECT_MSG(Codes.insert(Def.Code).second, std::string(Def.Code) + " is listed twice");
		const std::string Dial = DialFor(Def.Code);
		EXPECT_MSG(!Dial.empty(), std::string(Def.Code) + " has no dialled form");
		EXPECT_MSG(Dials.insert(Dial).second,
			std::string(Def.Code) + " dials the same number as another cheat (" + Dial + ")");

		// Both forms, and the sloppy ways a person actually types them.
		EXPECT_MSG(FindCheat(Def.Code) == &Def, std::string(Def.Code) + " is not found by its word");
		EXPECT_MSG(FindCheat(Dial) == &Def, std::string(Def.Code) + " is not found by its number");
		std::string Lower = Def.Code;
		std::transform(Lower.begin(), Lower.end(), Lower.begin(),
			[](char Letter) { return static_cast<char>(Letter >= 'A' && Letter <= 'Z' ? Letter + 32 : Letter); });
		EXPECT_MSG(FindCheat(Lower) == &Def, std::string(Def.Code) + " is case sensitive");
		EXPECT_MSG(FindCheat(" " + std::string(Def.Code) + " ") == &Def,
			std::string(Def.Code) + " cannot cope with spaces");
	}
	// The keypad mapping itself, on a known word.
	EXPECT_MSG(DialFor("PAYDAY") == "1-999-729329", "PAYDAY dials " + DialFor("PAYDAY"));
	EXPECT(FindCheat("not a cheat") == nullptr);
	EXPECT(FindCheat("") == nullptr);
}

// Entering one does something, says so, and marks the save forever.
DTG_TEST(CheatsMarkTheSaveAndCannotBeUnmarked)
{
	GameState State = dtg_test::FreshGame(11);
	EXPECT(!State.Cheats.EverUsed());

	const Money Before = State.Funds.Balance();
	const ActionResult Payday = State.EnterCheat("1-999-729329");   // PAYDAY, dialled
	EXPECT_MSG(Payday.bOk, "PAYDAY failed: " + Payday.Message);
	EXPECT_MSG(State.Funds.Balance() > Before, "PAYDAY paid nothing");
	EXPECT_MSG(!Payday.Message.empty(), "PAYDAY said nothing back");
	EXPECT_MSG(State.Cheats.EverUsed(), "using a cheat did not mark the save");

	// A toggle goes on, comes off, and the mark stays either way.
	EXPECT(State.EnterCheat("TITANIUM").bOk);
	EXPECT_MSG(State.CheatOn(Cheat::Titanium), "TITANIUM did not turn on");
	EXPECT(State.EnterCheat("titanium").bOk);
	EXPECT_MSG(!State.CheatOn(Cheat::Titanium), "TITANIUM did not turn off");
	EXPECT_MSG(State.Cheats.EverUsed(), "turning a cheat off cleared the mark");

	// And nonsense is refused rather than ignored.
	const ActionResult Nothing = State.EnterCheat("1-999-000000");
	EXPECT(!Nothing.bOk);
	EXPECT(!Nothing.Message.empty());
}

// A few that have to actually reach the systems they claim to.
DTG_TEST(CheatsReachTheirSystems)
{
	GameState State = dtg_test::FreshGame(12);

	State.ReportCrime(Crime::VehicleTheft, true, true);
	EXPECT(State.Wanted.Stars() > 0);
	EXPECT(State.EnterCheat("COLDCASE").bOk);
	EXPECT_MSG(State.Wanted.Stars() == 0, "COLDCASE left the player wanted");

	EXPECT(State.EnterCheat("MOSTWANTED").bOk);
	EXPECT_MSG(State.Wanted.Stars() == WantedSystem::MaxStars, "MOSTWANTED did not max the stars");

	const std::size_t Garage = State.Garage.size();
	EXPECT(State.EnterCheat("BARNFIND").bOk);
	EXPECT_MSG(State.Garage.size() == Garage + 1, "BARNFIND produced no car");
	EXPECT_MSG(State.Garage.back().Condition < 0.45, "the barn find arrived in good condition");

	EXPECT(State.EnterCheat("ALLTHREE").bOk);
	EXPECT_MSG(State.PlayerPowers.HasPowers(), "ALLTHREE granted no powers");
	EXPECT_MSG(State.PlayerPowers.Slots().size() == PowerSet::SlotCount,
		"ALLTHREE granted " + std::to_string(State.PlayerPowers.Slots().size()) + " powers");

	const Office Was = State.Politics.Current();
	EXPECT(State.EnterCheat("LANDSLIDE").bOk);
	EXPECT_MSG(static_cast<int>(State.Politics.Current()) > static_cast<int>(Was),
		"LANDSLIDE did not advance the office");
}

// THE invariant. Open ground in daylight is safe; nothing else reliably is.
DTG_TEST(SundownDaylightIsTheWholeDesign)
{
	GameState State = dtg_test::FreshGame(13);
	EXPECT(State.EnterCheat("SUNDOWN").bOk);
	EXPECT_MSG(State.Sundown.IsActive(), "SUNDOWN did not start the mode");

	int SafeMinutes = 0;
	int DarkMinutes = 0;
	for (std::int64_t Minute = 0; Minute < MinutesPerDay; ++Minute)
	{
		const double Sun = State.Sundown.SunAt(Minute);
		EXPECT_MSG(Sun >= 0.0 && Sun <= 1.0,
			"the sun is at " + std::to_string(Sun) + " at minute " + std::to_string(Minute));

		const double Outside = State.Sundown.Exposure(Minute, /*bIndoors*/ false, /*bLit*/ false, 0.0);
		const double OutsideFar = State.Sundown.Exposure(Minute, false, false, 3000.0);
		const double InsideDark = State.Sundown.Exposure(Minute, /*bIndoors*/ true, /*bLit*/ false, 0.0);

		if (Sun >= 0.55)
		{
			EXPECT_MSG(Outside == 0.0,
				"the open ground is not safe at minute " + std::to_string(Minute) +
					" with the sun at " + std::to_string(Sun));
			EXPECT_MSG(OutsideFar == 0.0, "distance from shelter matters in full daylight");
			++SafeMinutes;
		}
		else
		{
			++DarkMinutes;
		}

		// A dark interior is never safe, at any hour. This is the inversion the mode turns on.
		EXPECT_MSG(InsideDark > 0.4,
			"a dark interior is only " + std::to_string(InsideDark) + " dangerous at minute " +
				std::to_string(Minute));
		// Being far from shelter can never be safer than being near it.
		EXPECT_MSG(OutsideFar >= Outside - 1e-9,
			"being further from shelter was safer at minute " + std::to_string(Minute));
	}
	EXPECT_MSG(SafeMinutes > 400, "only " + std::to_string(SafeMinutes) + " minutes of the day are safe");
	EXPECT_MSG(DarkMinutes > 300, "only " + std::to_string(DarkMinutes) + " minutes of the day are dark");

	// Light inside is the one thing that makes an interior safe, which is what the generator buys.
	const std::int64_t Midnight = 0;
	EXPECT_MSG(State.Sundown.Exposure(Midnight, true, /*bLit*/ true, 0.0) <
		State.Sundown.Exposure(Midnight, true, false, 0.0),
		"light indoors is no safer than darkness");

	// The clock the player actually watches, and the one thing it must never do: disagree with
	// whether the ground is safe. Checked at every minute of the day, because an earlier version
	// measured the countdown against one threshold and the danger against another, and at seven in
	// the evening it said the light had run out while the rules still called it full daylight.
	for (std::int64_t Minute = 0; Minute < MinutesPerDay; ++Minute)
	{
		const bool bSafe = State.Sundown.Exposure(Minute, false, false, 0.0) == 0.0;
		const double Left = State.Sundown.MinutesOfLightLeft(Minute);
		const double Wait = State.Sundown.MinutesUntilSafe(Minute);
		EXPECT_MSG(bSafe == (Left > 0.0),
			"at minute " + std::to_string(Minute) + " the ground is " +
				(bSafe ? "safe" : "not safe") + " but the watch says " + std::to_string(Left) +
				" minutes of light");
		EXPECT_MSG(bSafe == (Wait == 0.0),
			"at minute " + std::to_string(Minute) + " the wait-for-light clock disagrees with the danger");
		EXPECT_MSG(Left >= 0.0 && Wait >= 0.0, "a light clock went negative");
	}
	// The day does get shorter as it goes.
	const double Morning = State.Sundown.MinutesOfLightLeft(9 * 60);
	const double Afternoon = State.Sundown.MinutesOfLightLeft(16 * 60);
	EXPECT_MSG(Morning > Afternoon, "the day did not get shorter between morning and afternoon");
	EXPECT_MSG(State.Sundown.MinutesOfLightLeft(23 * 60) == 0.0, "there is daylight left at eleven at night");
	EXPECT_MSG(State.Sundown.MinutesUntilSafe(3 * 60) > 0.0, "the small hours report no wait for daylight");

	// And the sun itself must be continuous: no jump anywhere, or the threshold gets crossed twice
	// in an afternoon and the mode stutters between safe and not.
	for (std::int64_t Minute = 1; Minute < MinutesPerDay; ++Minute)
	{
		const double Step = std::fabs(State.Sundown.SunAt(Minute) - State.Sundown.SunAt(Minute - 1));
		EXPECT_MSG(Step < 0.02, "the sun jumped by " + std::to_string(Step) + " at minute " +
			std::to_string(Minute));
	}
}

// Scavenging strips a place, which is the pressure that pushes the player somewhere new.
DTG_TEST(SundownScavengingRunsPlacesDry)
{
	GameState State = dtg_test::FreshGame(14);
	EXPECT(State.EnterCheat("SUNDOWN").bOk);

	// Somewhere that is not a nest, so the haul is about depletion rather than what woke up.
	int Target = -1;
	for (const BuildingPlan& Building : State.City.Buildings)
	{
		if (!State.Sundown.IsNest(Building.Id))
		{
			Target = Building.Id;
			break;
		}
	}
	EXPECT_MSG(Target >= 0, "every building in the state is a nest");

	const std::int64_t Noon = 12 * 60;
	int First = 0;
	int Later = 0;
	for (int Run = 0; Run < 5; ++Run)
	{
		const Haul Got = State.Sundown.Scavenge(State.City, Target, Noon, State.Random);
		EXPECT_MSG(Got.Minutes > 0, "searching a building took no time");
		EXPECT_MSG(!Got.Story.empty(), "a scavenging run said nothing");
		if (Run == 0)
		{
			First = Got.Total();
		}
		Later = Got.Total();
	}
	EXPECT_MSG(State.Sundown.Picked(Target) > 0.9,
		"five searches only stripped the place to " + std::to_string(State.Sundown.Picked(Target)));
	EXPECT_MSG(Later <= First, "the fifth search of a building beat the first");

	// A nest is a nest whatever the hour, and says so.
	int NestId = -1;
	for (const Nest& Value : State.Sundown.Nests())
	{
		if (!Value.bCleared)
		{
			NestId = Value.BuildingId;
			break;
		}
	}
	EXPECT_MSG(NestId >= 0, "the mode started with no nests at all");
	const Haul Disturbed = State.Sundown.Scavenge(State.City, NestId, Noon, State.Random);
	EXPECT_MSG(Disturbed.bNest, "searching a nest at noon did not wake it");
	EXPECT_MSG(Disturbed.bDisturbed, "a woken nest was not marked as disturbed");
}

// The Alpha learns a habit and forgets one.
DTG_TEST(SundownTheAlphaLearnsWhereYouGo)
{
	GameState State = dtg_test::FreshGame(15);
	EXPECT(State.EnterCheat("SUNDOWN").bOk);

	const double X = State.City.SpawnX;
	const double Y = State.City.SpawnY;
	EXPECT_MSG(State.Sundown.Expectation(X, Y) == 0.0, "the Alpha started out already expecting you");

	double Last = 0.0;
	for (int Day = 0; Day < 4; ++Day)
	{
		State.Sundown.Noticed(X, Y, State.Sundown.Day());
		const double Now = State.Sundown.Expectation(X, Y);
		EXPECT_MSG(Now > Last, "working the same block again taught the Alpha nothing on day " +
			std::to_string(Day));
		Last = Now;
	}
	EXPECT_MSG(Last > 0.6, "four visits to one block only reached " + std::to_string(Last));
	EXPECT_MSG(Last < 1.0, "the Alpha became certain, which leaves the player no gamble to take");

	// Somewhere it has never seen you is still clean.
	EXPECT_MSG(State.Sundown.Expectation(X + 5000.0, Y + 5000.0) == 0.0,
		"the Alpha expects you somewhere you have never been");

	// And it lets a habit go if you stop.
	for (int Day = 0; Day < 6; ++Day)
	{
		State.Sundown.NewDay(State.Random);
	}
	EXPECT_MSG(State.Sundown.Expectation(X, Y) < Last,
		"the Alpha never forgets, which means a block is ruined permanently");
}

// The night responds to what you did with the day.
DTG_TEST(SundownTheNightRespondsToPreparation)
{
	GameState State = dtg_test::FreshGame(16);
	EXPECT(State.EnterCheat("SUNDOWN").bOk);

	// Nowhere to be, and out in it: this should go badly, every time.
	int Breaches = 0;
	for (int Trial = 0; Trial < 20; ++Trial)
	{
		GameState Fresh = dtg_test::FreshGame(static_cast<std::uint64_t>(900 + Trial));
		Fresh.EnterCheat("SUNDOWN");
		const NightReport Report = Fresh.Sundown.Night(/*HoursOutside*/ 6.0, Fresh.Random);
		if (Report.bBreached)
		{
			++Breaches;
		}
		EXPECT_MSG(!Report.Story.empty(), "a night passed without saying anything");
		EXPECT_MSG(Report.DreadPeak >= 0.0 && Report.DreadPeak <= 1.0,
			"dread out of range at " + std::to_string(Report.DreadPeak));
	}
	EXPECT_MSG(Breaches == 20, "standing outside all night was survivable " +
		std::to_string(20 - Breaches) + " times out of 20");

	// A boarded-up building, come in before dark: this should mostly hold.
	int Held = 0;
	for (int Trial = 0; Trial < 20; ++Trial)
	{
		GameState Fresh = dtg_test::FreshGame(static_cast<std::uint64_t>(700 + Trial));
		Fresh.EnterCheat("SUNDOWN");
		Fresh.Sundown.Supplies().Parts = 20;
		const int Shelter = Fresh.City.Buildings.front().Id;
		EXPECT_MSG(Fresh.Sundown.Fortify(Shelter, 12, false, 0.0), "boarding up a building failed");
		EXPECT_MSG(Fresh.Sundown.CurrentRefuge().Barricade >= 0.99,
			"twelve parts only reached " + std::to_string(Fresh.Sundown.CurrentRefuge().Barricade));
		const NightReport Report = Fresh.Sundown.Night(/*HoursOutside*/ 0.0, Fresh.Random);
		if (!Report.bBreached)
		{
			++Held;
		}
	}
	EXPECT_MSG(Held >= 18, "a fully boarded shelter held only " + std::to_string(Held) + " nights in 20");

	// Light is the real defence, and this isolates it: no boards at all either way, a hard night,
	// the only difference being whether the generator is running. The first version of this test
	// gave the shelter boards as well, so it passed whether or not the light was doing anything --
	// removing the guard that protects a lit shelter did not fail it.
	int LitHeld = 0;
	int DarkHeld = 0;
	for (int Trial = 0; Trial < 20; ++Trial)
	{
		GameState Lit = dtg_test::FreshGame(static_cast<std::uint64_t>(300 + Trial));
		Lit.EnterCheat("SUNDOWN");
		Lit.Sundown.Supplies().Fuel = 80;
		const int Where = Lit.City.Buildings.front().Id;
		EXPECT_MSG(Lit.Sundown.Fortify(Where, /*PartsSpent*/ 0, /*bGenerator*/ true, 60.0),
			"the generator would not go in");
		EXPECT_MSG(Lit.Sundown.CurrentRefuge().bGenerator, "the generator did not go in");
		EXPECT_MSG(Lit.Sundown.CurrentRefuge().Barricade <= 0.0, "the lit shelter got boards too");
		if (!Lit.Sundown.Night(4.0, Lit.Random).bBreached)
		{
			++LitHeld;
		}

		GameState Dark = dtg_test::FreshGame(static_cast<std::uint64_t>(300 + Trial));
		Dark.EnterCheat("SUNDOWN");
		EXPECT(Dark.Sundown.Fortify(Where, 0, /*bGenerator*/ false, 0.0));
		if (!Dark.Sundown.Night(4.0, Dark.Random).bBreached)
		{
			++DarkHeld;
		}
	}
	EXPECT_MSG(LitHeld == 20, "a lit shelter was breached " + std::to_string(20 - LitHeld) +
		" times in 20 -- light is supposed to be the thing that stops them");
	EXPECT_MSG(DarkHeld <= 4, "an unboarded, unlit shelter survived " + std::to_string(DarkHeld) +
		" nights in 20, so the generator is buying nothing");
}

// Clearing nests thins them out, and the cure is reachable.
DTG_TEST(SundownHasAWayOut)
{
	GameState State = dtg_test::FreshGame(18);
	EXPECT(State.EnterCheat("SUNDOWN").bOk);

	int NestId = -1;
	int Occupants = 0;
	for (const Nest& Value : State.Sundown.Nests())
	{
		if (!Value.bCleared && Value.Occupants > 0)
		{
			NestId = Value.BuildingId;
			Occupants = Value.Occupants;
			break;
		}
	}
	EXPECT(NestId >= 0);
	// Turning up without enough ammunition does not work.
	EXPECT_MSG(!State.Sundown.ClearNest(NestId, 1, State.Random),
		"a nest was cleared with a single round");
	EXPECT_MSG(State.Sundown.IsNest(NestId), "the nest cleared anyway");
	// With enough, it does.
	bool bCleared = false;
	for (int Attempt = 0; Attempt < 6 && !bCleared; ++Attempt)
	{
		bCleared = State.Sundown.ClearNest(NestId, Occupants * 3, State.Random);
	}
	EXPECT_MSG(bCleared, "a nest could not be cleared with three times the ammunition needed");
	EXPECT_MSG(!State.Sundown.IsNest(NestId), "a cleared nest is still a nest");

	// The cure: supplies and time, neither substituting for the other.
	State.Sundown.Supplies().Medical = 400;
	EXPECT_MSG(!State.Sundown.Research(0, 10.0, State.Random), "research ran on no supplies");
	EXPECT_MSG(!State.Sundown.Research(10, 0.0, State.Random), "research ran in no time");
	EXPECT_MSG(!State.Sundown.CureReady(), "the cure was ready before any work was done");
	for (int Session = 0; Session < 40 && !State.Sundown.CureReady(); ++Session)
	{
		State.Sundown.Research(14, 9.0, State.Random);
	}
	EXPECT_MSG(State.Sundown.CureReady(), "forty sessions did not produce a cure");
	EXPECT_MSG(State.Sundown.Deploy(&State.Outbreak), "the cure could not be deployed");
	EXPECT_MSG(State.Sundown.CureDeployed(), "deploying the cure did not take");
	EXPECT_MSG(!State.Sundown.Deploy(&State.Outbreak), "the cure deployed twice");
}

// Toggling the mode off puts the world back, and a save carries all of it.
DTG_TEST(SundownTogglesAndSurvivesSaving)
{
	GameState State = dtg_test::FreshGame(19);
	EXPECT(State.EnterCheat("SUNDOWN").bOk);
	EXPECT(State.Sundown.IsActive());
	State.Sundown.Supplies().Food = 17;
	State.Sundown.Supplies().Ammo = 42;
	State.Sundown.Noticed(State.City.SpawnX, State.City.SpawnY, 0);
	State.Sundown.Scavenge(State.City, State.City.Buildings.front().Id, 12 * 60, State.Random);

	const std::string Saved = State.SaveToJson();
	GameState Back;
	ContentReport Report;
	Back.LoadContent(dtg_test::LoadAllFiles(), Report);
	std::string Error;
	EXPECT_MSG(Back.LoadFromJson(Saved, &Error), "the save would not load: " + Error);

	EXPECT_MSG(Back.Sundown.IsActive(), "the mode did not survive a save");
	EXPECT_MSG(Back.Sundown.Supplies().Food == 17, "supplies did not survive a save");
	EXPECT_MSG(Back.Sundown.Supplies().Ammo == 42, "ammunition did not survive a save");
	EXPECT_MSG(Back.Sundown.Nests().size() == State.Sundown.Nests().size(), "the nests did not survive a save");
	EXPECT_MSG(Back.Sundown.Expectation(State.City.SpawnX, State.City.SpawnY) ==
		State.Sundown.Expectation(State.City.SpawnX, State.City.SpawnY),
		"what the Alpha learned did not survive a save");
	EXPECT_MSG(Back.Cheats.EverUsed(), "the cheat mark did not survive a save");
	EXPECT_MSG(Back.SaveToJson() == Saved, "a reloaded Sundown game saves differently");

	// And off again.
	EXPECT(State.EnterCheat("SUNDOWN").bOk);
	EXPECT_MSG(!State.Sundown.IsActive(), "SUNDOWN would not turn off");
	EXPECT_MSG(State.Sundown.Exposure(0, false, false, 0.0) == 0.0,
		"the world is still dangerous with the mode off");
}
