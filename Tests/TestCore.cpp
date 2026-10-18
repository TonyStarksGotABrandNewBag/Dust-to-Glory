#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace dtg::sim;

DTG_TEST(JsonParsesAndRoundTrips)
{
	const JsonParseResult Result = ParseJson("// comment\n{ \"a\": [1, 2.5, -3e2], \"b\": {\"c\": \"x\\ny\\u00e9\"}, \"d\": true, \"e\": null }");
	EXPECT(Result.bOk);
	EXPECT(Result.Value.Get("a").Size() == 3);
	EXPECT_NEAR(Result.Value.Get("a").At(2).AsNumber(), -300.0, 1e-9);
	EXPECT(Result.Value.Get("b").GetString("c") == "x\ny\xC3\xA9");
	EXPECT(Result.Value.GetBool("d"));
	EXPECT(Result.Value.Get("e").IsNull());
	const JsonParseResult Again = ParseJson(Result.Value.Dump(true));
	EXPECT(Again.bOk);
	EXPECT(Again.Value.Dump() == Result.Value.Dump());
}

DTG_TEST(JsonReportsErrorLocation)
{
	const JsonParseResult Result = ParseJson("{\n  \"a\": 1,\n  \"b\": oops\n}");
	EXPECT(!Result.bOk);
	EXPECT(Result.Line == 3);
}

DTG_TEST(MoneyFormatting)
{
	EXPECT(FormatMoney(Dollars(1234567)) == "$1,234,567");
	EXPECT(FormatMoney(12345) == "$123.45");
	EXPECT(FormatMoney(-Dollars(40)) == "-$40");
	EXPECT(FormatMoney(0) == "$0");
}

DTG_TEST(RngIsDeterministic)
{
	Rng A(99);
	Rng B(99);
	for (int Index = 0; Index < 100; ++Index)
	{
		EXPECT(A.NextU32() == B.NextU32());
	}
	Rng C(1);
	for (int Index = 0; Index < 1000; ++Index)
	{
		const int Value = C.RangeInt(3, 7);
		EXPECT(Value >= 3 && Value <= 7);
	}
}

DTG_TEST(WalletSpendsOnlyWhatItHas)
{
	Wallet Funds(Dollars(100));
	EXPECT(!Funds.TrySpend(Dollars(150), MoneyCategory::Other, "too much", 0));
	EXPECT(Funds.Balance() == Dollars(100));
	EXPECT(Funds.TrySpend(Dollars(40), MoneyCategory::Vehicle, "parts", 0));
	Funds.Deposit(Dollars(10), MoneyCategory::Job, "pay", 1);
	EXPECT(Funds.Balance() == Dollars(70));
	EXPECT(Funds.Charge(Dollars(500), MoneyCategory::Fine, "fine", 2) == Dollars(70));
	EXPECT(Funds.Balance() == 0);
	EXPECT(Funds.LifetimeEarned(MoneyCategory::Job) == Dollars(10));
}

DTG_TEST(ClockAndWeather)
{
	GameClock Clock;
	EXPECT(Clock.Hour() == 8);
	EXPECT(Clock.Advance(120.0) == 60); // two real minutes = one game hour at default pace
	EXPECT(Clock.Hour() == 9);
	Clock.Skip(GameClock::MinutesPerDay * 6);
	EXPECT(Clock.DayOfWeek() == 6);
	WeatherSystem Weather;
	Rng Random(5);
	int Changes = 0;
	for (std::int64_t Minute = 0; Minute < 60 * 24 * 10; ++Minute)
	{
		Changes += Weather.Update(Minute, Random) ? 1 : 0;
	}
	EXPECT(Changes > 5);
}

DTG_TEST(CareersRequireEducationAndPromote)
{
	GameState State = dtg_test::FreshGame();
	EXPECT(!State.ApplyForCareer("doctor").bOk);
	EXPECT(State.ApplyForCareer("retail-associate").bOk);
	const Money Before = State.Funds.Balance();
	const ShiftResult Shift = State.WorkShift("retail-associate", 1.0);
	EXPECT(Shift.bValid);
	EXPECT(State.Funds.Balance() > Before);
	bool bPromoted = false;
	for (int Index = 0; Index < 20 && !bPromoted; ++Index)
	{
		bPromoted = State.WorkShift("retail-associate", 1.0).bPromoted;
	}
	EXPECT(bPromoted);
}

DTG_TEST(HelpDeskPaysLikeTheNineties)
{
	GameState State = dtg_test::FreshGame();
	const CareerDef* Def = State.Careers.FindCareer("help-desk-tech");
	EXPECT(Def != nullptr);
	if (Def)
	{
		const double Hourly = ToDollars(Def->Levels.front().HourlyPay);
		EXPECT(Hourly >= 12.0 && Hourly <= 18.0);
		// A year of help-desk work buys a typical house in about three years: the 1990s ratio.
		double CheapestHome = 1e12;
		double TotalHomes = 0.0;
		int Homes = 0;
		for (const Property& Home : State.Homes.Properties())
		{
			if (Home.Kind == "House")
			{
				CheapestHome = std::min(CheapestHome, ToDollars(Home.Price));
				TotalHomes += ToDollars(Home.Price);
				++Homes;
			}
		}
		const double Average = TotalHomes / std::max(1, Homes);
		const double Annual = Hourly * 2000.0;
		EXPECT_MSG(Average / Annual < 6.0 && Average / Annual > 1.5, "average house " + std::to_string(Average));
	}
}

DTG_TEST(EducationGraduates)
{
	GameState State = dtg_test::FreshGame();
	EXPECT(State.Enroll("it-cert").bOk);
	bool bGraduated = false;
	for (int Index = 0; Index < 10 && !bGraduated; ++Index)
	{
		bGraduated = State.AttendClass(1.0, 3).bGraduated;
	}
	EXPECT(bGraduated);
	EXPECT(State.Player.HasEducation("it-cert"));
	EXPECT(State.ApplyForCareer("help-desk-tech").bOk);
}

DTG_TEST(HackingScoresLikeMastermind)
{
	const GuessFeedback Exact = HackSession::Score({1, 2, 3, 4}, {1, 2, 3, 4}, 6);
	EXPECT(Exact.Exact == 4 && Exact.Partial == 0);
	const GuessFeedback Mixed = HackSession::Score({1, 2, 3, 4}, {4, 3, 2, 1}, 6);
	EXPECT(Mixed.Exact == 0 && Mixed.Partial == 4);
	const GuessFeedback Dupes = HackSession::Score({1, 1, 2, 2}, {1, 2, 1, 5}, 6);
	EXPECT(Dupes.Exact == 1 && Dupes.Partial == 2);
}

DTG_TEST(HackingSessionBreachesAndTraces)
{
	HackConfig Config = MakeHackConfig(2, 0, HackToolkit());
	HackSession Session(Config, 1234);
	for (int Layer = 0; Layer < Session.Layers(); ++Layer)
	{
		Session.Guess(Session.SecretForLayer(Session.Layer()));
	}
	EXPECT(Session.State() == HackState::Breached);

	HackSession Doomed(Config, 99);
	std::vector<int> Wrong(static_cast<std::size_t>(Doomed.CodeLength()), 0);
	for (int Attempt = 0; Attempt < 50 && Doomed.State() == HackState::Active; ++Attempt)
	{
		if (Doomed.SecretForLayer(Doomed.Layer()) == Wrong)
		{
			Wrong[0] = 1;
		}
		Doomed.Guess(Wrong);
	}
	EXPECT(Doomed.State() == HackState::Traced);

	HackToolkit Tools;
	Tools.PatternEngine = 2;
	HackSession Assisted(MakeHackConfig(1, 0, Tools), 7);
	int Revealed = 0;
	for (int Position = 0; Position < Assisted.CodeLength(); ++Position)
	{
		Revealed += Assisted.IsRevealed(Position) ? 1 : 0;
	}
	EXPECT(Revealed == 2);
}

DTG_TEST(HackContractsMixLegalAndGrey)
{
	Rng Random(3);
	const std::vector<HackContract> Board = GenerateHackContracts(200, 5, Random);
	int Unauthorized = 0;
	for (const HackContract& Contract : Board)
	{
		Unauthorized += Contract.Legality == HackLegality::Unauthorized ? 1 : 0;
		EXPECT(Contract.Payout > 0);
		if (Contract.Legality == HackLegality::Unauthorized)
		{
			EXPECT(Contract.HeatStars >= 1);
		}
	}
	EXPECT(Unauthorized > 20 && Unauthorized < 90);
}

DTG_TEST(FurnitureDeliversDuringBusinessHours)
{
	for (std::int64_t Now = 0; Now < 24 * 60 * 3; Now += 37)
	{
		const std::int64_t Standard = ScheduleDelivery(Now, DeliverySpeed::Standard);
		const std::int64_t TimeOfDay = Standard % (24 * 60);
		EXPECT(TimeOfDay >= 9 * 60 && TimeOfDay <= 18 * 60);
		EXPECT(Standard > Now);
	}
	GameState State = dtg_test::FreshGame();
	const BuildingPlan* Starter = State.City.FindBuilding(State.City.StarterHomeId);
	EXPECT(Starter != nullptr);
	State.Funds.Deposit(Dollars(10000), MoneyCategory::Other, "test", 0);
	EXPECT(State.OrderFurniture({"sofa-plaid", "tv-27"}, State.City.StarterHomeId, DeliverySpeed::Express).bOk);
	State.Skip(8 * 60);
	const std::vector<std::string> Waiting = State.Homes.Unplaced(State.PropertyIdFor(State.City.StarterHomeId));
	EXPECT(Waiting.size() == 2);
	int Uid = 0;
	EXPECT(State.Homes.Place(State.PropertyIdFor(State.City.StarterHomeId), "sofa-plaid", 2.0, 3.0, 90.0, Uid));
	EXPECT(State.Homes.PickUp(Uid));
	EXPECT(State.Homes.Unplaced(State.PropertyIdFor(State.City.StarterHomeId)).size() == 2);
}

DTG_TEST(DragTreeFoulsAndReactions)
{
	DragTree Tree(TreeType::Pro);
	Tree.Start(1.0);
	Tree.Tick(1.2);
	EXPECT(Tree.AmbersLit() == 3);
	EXPECT(Tree.Launch() < 0.0);
	EXPECT(Tree.IsFoul());
	DragTree Clean(TreeType::Sportsman);
	Clean.Start(1.0);
	Clean.Tick(2.0);
	EXPECT(Clean.AmbersLit() == 3);
	Clean.Tick(0.7);
	EXPECT(Clean.IsGreen());
	EXPECT_NEAR(Clean.Launch(), 0.2, 1e-9);
	DragResult Quick{0.2, 11.0, 200.0, false};
	DragResult Slow{0.1, 11.5, 190.0, false};
	DragResult Foul{-0.1, 9.0, 220.0, true};
	EXPECT(DragWinner(Quick, Slow));
	EXPECT(DragWinner(Slow, Foul));
}

DTG_TEST(RaceTrackerCountsLapsAndStandings)
{
	RaceTracker Race(4, 2, 3);
	double Time = 0.0;
	for (int Pass = 0; Pass <= 8; ++Pass)
	{
		Time += 10.0;
		EXPECT(Race.PassCheckpoint(0, Pass % 4, Time));
	}
	EXPECT(Race.Finished(0));
	EXPECT(Race.Lap(1) == 0);
	EXPECT(!Race.PassCheckpoint(1, 2, 5.0)); // must start at checkpoint 0
	Race.PassCheckpoint(1, 0, 1.0);
	Race.PassCheckpoint(2, 0, 1.0);
	Race.PassCheckpoint(2, 1, 9.0);
	const std::vector<int> Order = Race.Standings({0.0, 50.0, 50.0});
	EXPECT(Order[0] == 0 && Order[1] == 2 && Order[2] == 1);
	EXPECT(FormulaPoints(1) == 25 && FormulaPoints(10) == 1 && FormulaPoints(11) == 0);
	EXPECT(Race.BestLap(0) > 0.0);
}

DTG_TEST(DerbyAndFreestyleScoring)
{
	EXPECT(DerbyHitPoints(5.0) == 0);
	EXPECT(DerbyHitPoints(40.0) > DerbyHitPoints(20.0));
	std::vector<DerbyScore> Scores(3);
	Scores[0].Points = 50;
	Scores[0].bSurvived = false;
	Scores[1].Points = 10;
	Scores[2].Points = 20;
	const std::vector<int> Ranking = DerbyRanking(Scores);
	EXPECT(Ranking[0] == 2 && Ranking[1] == 1 && Ranking[2] == 0);
	FreestyleRun Run;
	Run.Backflips = 1;
	Run.AirtimeS = 6.0;
	Run.Donuts = 3;
	const double Score = ScoreFreestyle(Run);
	EXPECT(Score > 4.0 && Score <= 10.0);
	Run.bCrashed = true;
	EXPECT(ScoreFreestyle(Run) < Score);
}

DTG_TEST(WantedLevelAndSearch)
{
	WantedSystem Wanted;
	Rng Random(1);
	Wanted.ReportCrime(Crime::VehicleTheft, true, false, Random);
	EXPECT(Wanted.Stars() == 1);
	Wanted.ReportCrime(Crime::HitPoliceCar, true, false, Random);
	EXPECT(Wanted.Stars() == 2);
	EXPECT(Wanted.UnitsWanted() == 5);
	for (int Step = 0; Step < 10; ++Step) Wanted.Update(1.0, true);
	EXPECT(Wanted.Stars() == 2);
	for (int Step = 0; Step < 40; ++Step) Wanted.Update(1.0, false);
	EXPECT(Wanted.Stars() == 0);
	EXPECT(Wanted.BustFine(Dollars(10000)) > 0);
}

DTG_TEST(TheOrdinaryThingsCostTimeAndMoney)
{
	GameState State = dtg_test::FreshGame(77);
	const Money Before = State.Funds.Balance();
	const std::int64_t Started = State.Clock.TotalMinutes();

	// A meal: cheap, quick, and it actually leaves the wallet and the clock different.
	const GameState::AmenityOffer Lunch = State.QuoteAmenity(GameState::Amenity::Meal);
	EXPECT(Lunch.bAvailable);
	EXPECT(Lunch.Cost > 0);
	EXPECT(State.UseAmenity(GameState::Amenity::Meal).bOk);
	EXPECT(State.Funds.Balance() == Before - Lunch.Cost);
	EXPECT(State.Clock.TotalMinutes() == Started + Lunch.Minutes);

	// An hour in the gym is an hour of fitness, and it gets harder to gain as the level climbs.
	const int FirstLevel = State.Player.SkillLevel(Skill::Fitness);
	const int FirstXp = State.Player.SkillXp(Skill::Fitness);
	EXPECT(State.UseAmenity(GameState::Amenity::Gym).bOk);
	const int Gained = State.Player.SkillXp(Skill::Fitness) - FirstXp;
	EXPECT(Gained > 0);
	for (int Hour = 0; Hour < 40; ++Hour)
	{
		State.UseAmenity(GameState::Amenity::Gym);
	}
	EXPECT(State.Player.SkillLevel(Skill::Fitness) > FirstLevel);
	const int HighXp = State.Player.SkillXp(Skill::Fitness);
	State.UseAmenity(GameState::Amenity::Gym);
	EXPECT(State.Player.SkillXp(Skill::Fitness) - HighXp < Gained); // diminishing returns

	// A doctor is only worth paying when something is wrong with you.
	EXPECT(!State.QuoteAmenity(GameState::Amenity::Treatment).bAvailable);
	State.Fights.Hurt(40.0);
	EXPECT(State.Fights.PlayerHealth() < CombatSystem::PlayerMaxHealth);
	EXPECT(State.QuoteAmenity(GameState::Amenity::Treatment).bAvailable);
	EXPECT(State.UseAmenity(GameState::Amenity::Treatment).bOk);
	EXPECT(State.Fights.PlayerHealth() == CombatSystem::PlayerMaxHealth);

	// And you cannot pay a fine you do not owe.
	EXPECT(!State.QuoteAmenity(GameState::Amenity::Fines).bAvailable);

	// Nothing is on offer with no money for it.
	GameState Broke = dtg_test::FreshGame(78);
	Broke.Funds.Charge(Broke.Funds.Balance(), MoneyCategory::Other, "test", 0);
	EXPECT(!Broke.QuoteAmenity(GameState::Amenity::Lodging).bAvailable);
	EXPECT(!Broke.UseAmenity(GameState::Amenity::Lodging).bOk);
	// Except the things that are free.
	EXPECT(Broke.QuoteAmenity(GameState::Amenity::Library).bAvailable);
	EXPECT(Broke.UseAmenity(GameState::Amenity::Library).bOk);
}
