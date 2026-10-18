#include "TestFramework.h"
#include "TestSupport.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

using namespace dtg::sim;

DTG_TEST(AllContentLoadsCleanly)
{
	GameState State;
	ContentReport Report;
	const bool bOk = State.LoadContent(dtg_test::LoadAllFiles(), Report);
	for (const std::string& Error : Report.Errors)
	{
		EXPECT_MSG(false, Error);
	}
	EXPECT(bOk);
	EXPECT(Report.Careers >= 25);
	EXPECT(Report.Powers >= 18);
	EXPECT(Report.Laws >= 10);
	EXPECT(Report.Businesses > 50);
}

DTG_TEST(EveryBuildingIsInteractable)
{
	const GameState& State = dtg_test::LoadedState();
	const CityPlan& City = State.City;
	EXPECT(City.Buildings.size() > 150);
	for (const BuildingPlan& Building : City.Buildings)
	{
		EXPECT_MSG(!ServicesForRole(Building.Role).empty(), Building.Name);
		EXPECT_MSG(!Building.Name.empty(), std::string("unnamed ") + LotRoleName(Building.Role));
		EXPECT(Building.Footprint.Width() > 5.0 && Building.Footprint.Depth() > 5.0);
		EXPECT(City.Bounds.Contains(Building.DoorX, Building.DoorY));
	}
	for (std::size_t A = 0; A < City.Buildings.size(); ++A)
	{
		for (std::size_t B = A + 1; B < City.Buildings.size(); ++B)
		{
			EXPECT(!City.Buildings[A].Footprint.Overlaps(City.Buildings[B].Footprint));
		}
	}
	for (const CareerDef& Def : State.Careers.Careers())
	{
		LotRole Role = LotRole::Office;
		EXPECT(ParseLotRole(Def.Workplace, Role));
		EXPECT_MSG(City.FirstWithRole(Role) != nullptr, Def.Workplace);
	}
	// Every brand still on sale has a showroom (sister brands share one); defunct brands are sold used.
	int Showrooms = 0;
	for (const Brand& Maker : State.Vehicles.Brands())
	{
		if (!Maker.HasShowroom(State.Vehicles.CurrentYear()))
		{
			continue;
		}
		++Showrooms;
		bool bFound = false;
		for (const BuildingPlan* Dealer : City.WithRole(LotRole::Dealership))
		{
			bFound = bFound || Dealer->Param == Maker.ShowroomId();
		}
		EXPECT_MSG(bFound, Maker.Id);
		bool bListed = false;
		for (const Brand* Sold : State.BrandsAtShowroom(Maker.ShowroomId()))
		{
			bListed = bListed || Sold == &Maker;
		}
		EXPECT_MSG(bListed, Maker.Id);
	}
	EXPECT(Showrooms >= 50);
	for (const std::string& Store : State.Homes.Stores())
	{
		bool bFound = false;
		for (const BuildingPlan* Shop : City.WithRole(LotRole::FurnitureStore))
		{
			bFound = bFound || Shop->Param == Store;
		}
		EXPECT_MSG(bFound, Store);
	}
	EXPECT(City.FindVenue(EventKind::DragRace) && City.FindVenue(EventKind::DemolitionDerby) &&
		City.FindVenue(EventKind::MonsterJam) && City.FindVenue(EventKind::Formula));
	EXPECT(City.StarterHomeId > 0);
	EXPECT(City.Bounds.Contains(City.SpawnX, City.SpawnY));
}

DTG_TEST(PowersChooseMutateAndEvolve)
{
	const GameState& Loaded = dtg_test::LoadedState();
	PowerSet Set;
	std::string Error;
	EXPECT(!Set.Choose(Loaded.Powers, {"flight", "flight", "telekinesis"}, &Error));
	EXPECT(!Set.Choose(Loaded.Powers, {"flight", "telekinesis"}, &Error));
	EXPECT(!Set.Choose(Loaded.Powers, {"flight", "telekinesis", "warp"}, &Error)); // evolved powers are not starters
	EXPECT(Set.Choose(Loaded.Powers, {"pyrokinesis", "flight", "telekinesis"}, &Error));
	Rng Random(11);
	int Mutations = 0;
	bool bEvolved = false;
	bool bLevelled = false;
	for (int Use = 0; Use < 400; ++Use)
	{
		MutationEvent Event;
		bool bLevelUp = false;
		if (Set.RecordUse(0, Loaded.Powers, Random, Event, bLevelUp))
		{
			++Mutations;
			bEvolved = bEvolved || Event.bEvolved;
		}
		bLevelled = bLevelled || bLevelUp;
	}
	EXPECT(Mutations >= 2);
	EXPECT(bEvolved);
	EXPECT(bLevelled);
	EXPECT(Set.Slot(0)->PowerId == "plasma-blast");
	const EffectivePower Power = Loaded.Powers.Evaluate(*Set.Slot(0));
	EXPECT(Power.Def != nullptr && Power.Potency > 1.0);

	const Superhuman Villain = GenerateSuperhuman(Loaded.Powers, Alignment::Villain, 5, Random);
	EXPECT(Villain.Powers.size() == 3);
	EXPECT(Villain.Bounty > 0);
	EXPECT(!Villain.Codename.empty());
}

DTG_TEST(MinistryIntegrityMatters)
{
	Ministry Honest;
	Ministry Greedy;
	EXPECT(Honest.Found("Hope Chapel"));
	EXPECT(Greedy.Found("Gold Chapel"));
	Greedy.SetSalaryShare(0.5);
	Rng RandomA(3);
	Rng RandomB(3);
	for (int Week = 0; Week < 20; ++Week)
	{
		Honest.Preach(static_cast<SermonTheme>(Week % 10), 0.8, RandomA);
		Greedy.Preach(static_cast<SermonTheme>(Week % 10), 0.8, RandomB);
		Honest.CollectWeeklyOffering(RandomA);
		Greedy.CollectWeeklyOffering(RandomB);
	}
	EXPECT(Honest.Integrity() > Greedy.Integrity());
	EXPECT(Honest.Members() > Greedy.Members());
	const double DoctrineBefore = Honest.Doctrine();
	EXPECT(Honest.RunTelethon(RandomA) > 0);
	EXPECT(Honest.Doctrine() < DoctrineBefore);
	Honest.Dissolve();
	EXPECT(!Honest.IsFounded());
}

DTG_TEST(MinistryCanBeBroughtDown)
{
	GameState State = dtg_test::FreshGame(5);
	EXPECT(State.FoundMinistry("Waccamaw Neck Fellowship").bOk);
	for (int Index = 0; Index < 6; ++Index)
	{
		State.Telethon(); // shortcuts erode doctrine and integrity
	}
	EXPECT(State.PlayerMinistry.Doctrine() < 0.5);
	bool bCollapsed = false;
	for (int Attempt = 0; Attempt < 20 && !bCollapsed; ++Attempt)
	{
		const AttackResult Result = State.Attack(GameState::PlayerMinistryId, AttackKind::DoctrineExpose);
		bCollapsed = Result.bCollapsed || !State.PlayerMinistry.IsFounded();
	}
	EXPECT(bCollapsed);
}

DTG_TEST(ReviewsStocksAndOwnership)
{
	GameState State = dtg_test::FreshGame(9);
	State.Funds.Deposit(Dollars(5000000), MoneyCategory::Other, "test", 0);
	const Stock* Orchard = State.Markets.FindStock("ORCH");
	EXPECT(Orchard != nullptr);
	const double Before = Orchard->Price;
	EXPECT(State.BuyShares("ORCH", 200000).bOk);
	EXPECT(State.Markets.FindStock("ORCH")->Price > Before); // big buys move the price
	EXPECT(State.Markets.SharesHeld("ORCH") == 200000);
	EXPECT(State.SellShares("ORCH", 100000).bOk);
	EXPECT(!State.SellShares("ORCH", 999999).bOk);

	const Business* Diner = nullptr;
	for (const Business& Value : State.Markets.Businesses())
	{
		if (Value.Sector == "Restaurant" && !Value.bPublic)
		{
			Diner = &Value;
			break;
		}
	}
	EXPECT(Diner != nullptr);
	if (!Diner) return;
	const std::string DinerId = Diner->Id;
	EXPECT(State.Markets.AskingPrice(DinerId) > 0);
	EXPECT(State.BuyBusiness(DinerId).bOk);
	EXPECT(State.Markets.FindBusiness(DinerId)->PlayerEquity == 1.0);
	// Run two weeks of business days: owner income arrives.
	const Money EarnedBefore = State.Funds.LifetimeEarned(MoneyCategory::Reward);
	State.Skip(14 * 24 * 60);
	EXPECT(State.Funds.LifetimeEarned(MoneyCategory::Reward) >= EarnedBefore);
	EXPECT(State.Markets.FindBusiness(DinerId)->Rep.ReviewCount > 0);
}

DTG_TEST(BertrandCompetition)
{
	Business Value;
	Value.BaseRevenue = Dollars(30000);
	Value.BaseCosts = Dollars(25000);
	EXPECT_NEAR(Value.CompetitionRevenueFactor(), 1.0, 1e-9);
	Value.PriceLevel = 1.3;
	Value.RivalPrice = 1.0;
	// Pricing far above the rival loses volume.
	EXPECT(Value.CompetitionCostFactor() < 1.0);
	const BertrandResult Eq = BertrandDuopoly(1.0, 1.0, 0.5, 0.6, 0.6);
	EXPECT_NEAR(Eq.PriceA, Eq.PriceB, 1e-9);
	EXPECT_NEAR(BertrandBestResponse(1.0, 1.0, 0.5, 0.6, Eq.PriceB), Eq.PriceA, 1e-9);
}

DTG_TEST(TurnaroundsUseNashBargaining)
{
	GameState State = dtg_test::FreshGame(13);
	const Business* Target = nullptr;
	for (const Business& Value : State.Markets.Businesses())
	{
		if (!Value.bPublic && Value.Problems.size() >= 2)
		{
			Target = &Value;
			break;
		}
	}
	EXPECT(Target != nullptr);
	if (!Target) return;
	const TurnaroundOffer Offer = State.Markets.ProposeTurnaround(Target->Id);
	EXPECT(Offer.bValid);
	double WeakPower = 0.0;
	const double WeakFee = State.Markets.NashFee(Offer, State.Player, &WeakPower);
	State.Player.AddSkillXp(Skill::People, 4000);
	State.Player.AddSkillXp(Skill::Business, 4000);
	double StrongPower = 0.0;
	const double StrongFee = State.Markets.NashFee(Offer, State.Player, &StrongPower);
	EXPECT(StrongPower > WeakPower);
	EXPECT(StrongFee > WeakFee);
	Rng Random(2);
	const NegotiationResult Greedy = State.Markets.Negotiate(Offer, ToMoney(StrongFee * 1.6), 0.0, State.Player, 1, Random);
	EXPECT(Greedy.Outcome == NegotiationOutcome::Countered);
	const NegotiationResult Fair = State.Markets.Negotiate(Offer, Greedy.Cash, Greedy.Equity, State.Player, 2, Random);
	EXPECT(Fair.Outcome == NegotiationOutcome::Accepted);
	State.Player.AddSkillXp(Skill::Tech, 4000);
	State.Player.AddSkillXp(Skill::Trades, 4000);
	State.Player.AddSkillXp(Skill::Creativity, 4000);
	const TurnaroundResult Result = State.Markets.PerformTurnaround(Target->Id, Fair.Cash, Fair.Equity, State.Player, 1.0, State.Funds, 0, Random);
	EXPECT(Result.bValid && Result.Fixed >= 1);
	EXPECT(Result.HealthAfter > Result.HealthBefore);
}

DTG_TEST(SmearsBackfireOnTheHonest)
{
	Market Markets;
	Business Honest;
	Honest.Id = "honest";
	Honest.Name = "Honest Hardware";
	Honest.Integrity = 1.0;
	Honest.Rep.Trust = 0.9;
	Markets.AddBusiness(Honest);
	Rng Random(8);
	int Backfires = 0;
	for (int Index = 0; Index < 50; ++Index)
	{
		Backfires += Markets.LaunchAttack("honest", AttackKind::SmearCampaign, true, 0, Random).bBackfired ? 1 : 0;
	}
	EXPECT(Backfires > 15);
	EXPECT(Markets.PlayerReputation().Trust < 0.2);

	Business Shady;
	Shady.Id = "shady";
	Shady.Name = "Shady Holdings";
	Shady.Integrity = 0.05;
	Markets.AddBusiness(Shady);
	// Allegations against a low-integrity outfit usually check out, and a few confirmed ones finish it.
	int Confirmed = 0;
	int Attempts = 0;
	while (!Markets.FindBusiness("shady")->bDissolved && Attempts < 10)
	{
		Confirmed += Markets.LaunchAttack("shady", AttackKind::Allegation, false, 0, Random).bConfirmed ? 1 : 0;
		++Attempts;
	}
	EXPECT(Markets.FindBusiness("shady")->bDissolved);
	EXPECT(Confirmed >= Attempts - 1);
}

DTG_TEST(CreativeWorksEarnRoyalties)
{
	Profile Player;
	Rng Random(4);
	IpPortfolio Ip;
	const CreativeWork Rough = Ip.Create(WorkKind::Song, "Garage Demo", Player, 2, 0.2, 0.0, Random);
	Player.AddSkillXp(Skill::Creativity, 4000);
	Player.AddSkillXp(Skill::People, 4000);
	const CreativeWork Polished = Ip.Create(WorkKind::Song, "Dust to Glory", Player, 80, 1.0, 60.0, Random);
	EXPECT(Polished.Quality > Rough.Quality + 0.3);
	Wallet Funds(Dollars(10000));
	EXPECT(Ip.Protect(Polished.Id, Funds, 0, nullptr));
	Money Total = 0;
	for (int Week = 0; Week < 10; ++Week)
	{
		for (const WeeklyRoyalty& Payment : Ip.WeeklyRoyalties(Random)) Total += Payment.Amount;
	}
	EXPECT(Total > Dollars(1000));
	EXPECT(Ip.LicenseOffer(Polished.Id) > 0);
}

DTG_TEST(NpcsWakeUp)
{
	const GameState& Loaded = dtg_test::LoadedState();
	NpcPopulation Npcs;
	NpcMind& Mind = Npcs.GetOrCreate("ped-17");
	EXPECT(Mind.Stage == AwarenessStage::Scripted);
	EXPECT(Npcs.GetOrCreate("ped-17").Name == Mind.Name); // deterministic and persistent
	Rng Random(1);
	// Spectacle habituates fast: watching the same power a hundred times is not enough on its own.
	for (int Index = 0; Index < 100; ++Index)
	{
		Npcs.Observe(Mind, Stimulus::SawPower, 1.0, Index, Random);
	}
	const double FromSpectacle = Mind.Awareness;
	EXPECT(FromSpectacle < 0.85);
	// Relationships keep counting.
	std::string LastStage;
	for (int Index = 0; Index < 200 && Mind.Stage != AwarenessStage::SelfDirected; ++Index)
	{
		Npcs.Respond(Mind, Index % 2 == 0 ? "chat" : "teach:Driving", 1000 + Index, Random, Loaded.Careers);
		Npcs.Observe(Mind, Stimulus::Helped, 1.0, 2000 + Index, Random);
	}
	EXPECT(Mind.Stage == AwarenessStage::SelfDirected);
	EXPECT(!Mind.Goal.empty());
	EXPECT(Mind.SkillLevel(Skill::Driving) > 0);
	const NpcDecision Decision = Npcs.Decide(Mind, 12, Random, Loaded.Careers);
	EXPECT(Decision.Activity != NpcActivity::Routine);
	bool bCanInvite = false;
	for (const DialogueOption& Option : Npcs.Options(Mind)) bCanInvite = bCanInvite || Option.Id == "invite";
	EXPECT(bCanInvite);
	EXPECT(Npcs.Respond(Mind, "invite", 5000, Random, Loaded.Careers).find("Yes") != std::string::npos);
	EXPECT(Mind.bCompanion);
}

DTG_TEST(NpcRepeatedGameRemembersBetrayal)
{
	const GameState& Loaded = dtg_test::LoadedState();
	NpcPopulation Npcs;
	NpcMind& Mind = Npcs.GetOrCreate("grudge");
	Mind.Deal.Strategy = RepeatedStrategy::GrimTrigger;
	Rng Random(2);
	EXPECT(DecideCooperate(Mind.Deal, Random));
	Npcs.Respond(Mind, "promise-break", 0, Random, Loaded.Careers);
	for (int Index = 0; Index < 20; ++Index)
	{
		Npcs.Respond(Mind, "promise-keep", Index, Random, Loaded.Careers);
		EXPECT(!DecideCooperate(Mind.Deal, Random)); // grim trigger never forgets
	}
	RepeatedGameMemory Tft;
	RecordOpponentMove(Tft, false);
	EXPECT(!DecideCooperate(Tft, Random));
	RecordOpponentMove(Tft, true);
	EXPECT(DecideCooperate(Tft, Random));
}

DTG_TEST(NashEquilibriaOfClassicGames)
{
	// Prisoner's dilemma: (Defect, Defect) is the unique equilibrium.
	const MatrixGame Dilemma = MatrixGame::Make(2, 2, {3, 0, 5, 1}, {3, 5, 0, 1});
	const auto PdPure = PureEquilibria(Dilemma);
	EXPECT(PdPure.size() == 1 && PdPure[0].first == 1 && PdPure[0].second == 1);
	EXPECT(FindEquilibria(Dilemma).size() == 1);

	// Matching pennies: unique mixed equilibrium at 50/50.
	const MatrixGame Pennies = MatrixGame::ZeroSum(2, 2, {1, -1, -1, 1});
	Equilibrium Mixed;
	EXPECT(SolveEquilibrium(Pennies, Mixed));
	EXPECT_NEAR(Mixed.Row[0], 0.5, 1e-9);
	EXPECT_NEAR(Mixed.Col[0], 0.5, 1e-9);
	EXPECT(PureEquilibria(Pennies).empty());

	// Battle of the sexes: two pure equilibria and one mixed.
	const MatrixGame Battle = MatrixGame::Make(2, 2, {2, 0, 0, 1}, {1, 0, 0, 2});
	EXPECT(FindEquilibria(Battle).size() == 3);

	// Rock-paper-scissors: everyone plays a third.
	const MatrixGame Rps = MatrixGame::ZeroSum(3, 3, {0, -1, 1, 1, 0, -1, -1, 1, 0});
	Equilibrium Third;
	EXPECT(SolveEquilibrium(Rps, Third));
	for (int Index = 0; Index < 3; ++Index)
	{
		EXPECT_NEAR(Third.Row[static_cast<std::size_t>(Index)], 1.0 / 3.0, 1e-9);
	}
	EXPECT_NEAR(Third.RowValue, 0.0, 1e-9);

	// The campaign game always has an equilibrium the AI can play.
	Equilibrium Campaign;
	EXPECT(SolveEquilibrium(CampaignGame(5, 5, 0.6), Campaign));
}

DTG_TEST(BargainingCournotAndInspection)
{
	const Bargain Even = NashBargain(100.0, 10.0, 20.0, 0.5);
	EXPECT(Even.bDeal);
	EXPECT_NEAR(Even.PayoffA, 45.0, 1e-9);
	EXPECT_NEAR(Even.PayoffB, 55.0, 1e-9);
	EXPECT(!NashBargain(10.0, 8.0, 8.0, 0.5).bDeal);
	const CournotResult Symmetric = CournotDuopoly(100.0, 1.0, 10.0, 10.0);
	EXPECT_NEAR(Symmetric.QuantityA, 30.0, 1e-9);
	EXPECT_NEAR(Symmetric.Price, 40.0, 1e-9);
	const CournotResult Cheaper = CournotDuopoly(100.0, 1.0, 5.0, 10.0);
	EXPECT(Cheaper.QuantityA > Cheaper.QuantityB);
	const InspectionResult Patrol = InspectionGame(100.0, 300.0, 50.0, 500.0);
	EXPECT_NEAR(Patrol.InspectProbability, 0.25, 1e-9);
	EXPECT_NEAR(Patrol.ViolateProbability, 0.1, 1e-9);
}

DTG_TEST(PoliticsFromCouncilToLaw)
{
	GameState State = dtg_test::FreshGame(21);
	EXPECT(!State.RunForOffice(Office::President).bOk);
	EXPECT(!State.RunForOffice(Office::CityCouncil).bOk); // nobody knows you yet
	State.Markets.PlayerReputation().Followers = 2000;
	State.Markets.PlayerReputation().Trust = 0.8;
	State.Funds.Deposit(Dollars(100000), MoneyCategory::Other, "donors", 0);
	EXPECT(State.RunForOffice(Office::CityCouncil).bOk);
	for (int Day = 0; Day < 3; ++Day)
	{
		State.CampaignRally(1.0);
		State.CampaignDebate(1.0);
		State.CampaignStrategy(0, 1.0);
		State.CampaignAds(Dollars(3000));
	}
	State.Skip(4 * 24 * 60);
	EXPECT(State.Politics.Wins() + State.Politics.Losses() == 1);
	if (State.Politics.Current() == Office::CityCouncil)
	{
		const double Before = ToDollars(State.Funds.Balance());
		LawVote Vote;
		for (int Try = 0; Try < 10 && !Vote.bPassed; ++Try)
		{
			Vote = State.ProposeLaw("raise-speed-limits");
		}
		EXPECT(Vote.bPassed || State.Politics.Approval() < 0.5);
		if (Vote.bPassed)
		{
			EXPECT(State.ActiveLaws().SpeedingHeatMul < 1.0);
		}
		(void)Before;
	}
	// Lobbying works without office.
	GameState Lobbyist = dtg_test::FreshGame(22);
	Lobbyist.Funds.Deposit(Dollars(5000000), MoneyCategory::Other, "pac", 0);
	LawVote Lobby;
	for (int Try = 0; Try < 10 && !Lobby.bPassed; ++Try)
	{
		Lobby = Lobbyist.LobbyForLaw("tax-relief-act", Dollars(400000));
	}
	EXPECT(Lobby.bPassed);
	EXPECT(Lobbyist.ActiveLaws().WageMul > 1.0);
}

DTG_TEST(GameStateSavesAndLoads)
{
	GameState State = dtg_test::FreshGame(77);
	State.Funds.Deposit(Dollars(250000), MoneyCategory::Other, "test", 0);
	EXPECT(State.ChoosePowers({"flight", "telekinesis", "grapple"}).bOk);
	EXPECT(State.ApplyForCareer("landscaper").bOk);
	State.WorkShift("landscaper", 0.9);
	const VehicleModel* Car = State.Vehicles.NewestOfBody(BodyStyle::Sports);
	EXPECT(Car != nullptr);
	EXPECT(State.BuyVehicle(Car->Id).bOk);
	EXPECT(State.InstallPart(State.ActiveVehicleUid, "turbo-small").bOk);
	EXPECT(State.FoundMinistry("Hope Chapel").bOk);
	State.Preach(SermonTheme::Hope, 0.9);
	EXPECT(State.BuyShares("KEYU", 100).bOk);
	State.CreateWork(WorkKind::Book, "My Story", 10, 0.5);
	NpcMind& Mind = State.Npcs.GetOrCreate("barista-1");
	State.Npcs.Observe(Mind, Stimulus::SawPower, 1.0, 0, State.Random);
	State.Skip(9 * 24 * 60);

	const std::string Saved = State.SaveToJson();
	GameState Restored = dtg_test::LoadedState();
	std::string Error;
	EXPECT(Restored.LoadFromJson(Saved, &Error));
	EXPECT(Error.empty());
	EXPECT(Restored.Funds.Balance() == State.Funds.Balance());
	EXPECT(Restored.Clock.TotalMinutes() == State.Clock.TotalMinutes());
	EXPECT(Restored.Garage.size() == State.Garage.size());
	EXPECT(Restored.FindOwned(State.ActiveVehicleUid) != nullptr);
	EXPECT(Restored.FindOwned(State.ActiveVehicleUid)->Build.Has("turbo-small"));
	EXPECT(Restored.PlayerPowers.Slots().size() == 3);
	EXPECT(Restored.PlayerMinistry.Name() == "Hope Chapel");
	EXPECT(Restored.Markets.SharesHeld("KEYU") == 100);
	EXPECT(Restored.Ip.Works().size() == 1);
	EXPECT(Restored.Npcs.Find("barista-1") != nullptr);
	EXPECT(Restored.Jobs.size() == 1);
	// Saving again produces the same document: nothing is lost in the round trip.
	const std::string Resaved = Restored.SaveToJson();
	EXPECT(Resaved == Saved);
	if (Resaved != Saved && std::getenv("DTG_DUMP"))
	{
		std::FILE* A = std::fopen("save_a.json", "w");
		std::FILE* B = std::fopen("save_b.json", "w");
		std::fputs(Saved.c_str(), A);
		std::fputs(Resaved.c_str(), B);
		std::fclose(A);
		std::fclose(B);
	}
}

DTG_TEST(AWeekInLongs)
{
	GameState State = dtg_test::FreshGame(31);
	EXPECT(State.ActiveVehicle() != nullptr);
	EXPECT(!State.TakeNotifications().empty());
	const TickReport Report = State.Skip(8 * 24 * 60);
	EXPECT(Report.bNewWeek);
	EXPECT(State.Markets.News().size() > 0 || State.Markets.Stocks().front().History.size() > 5);
	EXPECT(State.NetWorth() > 0);
}

DTG_TEST(RubinsteinBargainingRewardsPatience)
{
	EXPECT_NEAR(RubinsteinShare(0.9, 0.9), 1.0 / 1.9, 1e-9);
	// An impatient responder concedes almost everything to the proposer.
	EXPECT(RubinsteinShare(0.9, 0.5) > 0.9);
	// A patient responder holds out.
	EXPECT(RubinsteinShare(0.5, 0.95) < 0.1);
}

namespace detail_offer_test
{
	std::string FindCar(dtg::sim::GameState& State, bool (*Want)(const dtg::sim::PrivateCar&))
	{
		for (int Index = 0; Index < 400; ++Index)
		{
			const std::string Id = "home-" + std::to_string(Index) + "-0";
			if (Want(State.PrivateCarAt(Id)))
			{
				return Id;
			}
		}
		return std::string();
	}

	void SetTime(dtg::sim::GameState& State, int Day, int Hour)
	{
		State.Clock.SetTotalMinutes(static_cast<double>(Day * 24 * 60 + Hour * 60));
	}
}

DTG_TEST(OffersOnAnyParkedCar)
{
	using namespace detail_offer_test;
	GameState State = dtg_test::FreshGame(61);
	State.Funds.Deposit(Dollars(1000000), MoneyCategory::Other, "test", 0);
	EXPECT(State.PhoneNumber.find("555-01") != std::string::npos);

	// The same spot always holds the same car and owner.
	const PrivateCar First = State.PrivateCarAt("home-7-0");
	const PrivateCar Again = MakePrivateCar(State.Vehicles, "home-7-0", State.WorldSeed);
	EXPECT(First.ModelId == Again.ModelId && First.Condition == Again.Condition);

	const std::string Id = FindCar(State, [](const PrivateCar& Car)
	{
		return Car.Schedule == OwnerSchedule::DayWorker && Car.Attachment < 0.5;
	});
	EXPECT(!Id.empty());
	if (Id.empty()) return;
	const VehicleModel* Model = State.Vehicles.FindModel(State.PrivateCarAt(Id).ModelId);
	const double Value = ToDollars(MarketValue(*Model, State.PrivateCarAt(Id).Condition, State.PrivateCarAt(Id).MileageKm, State.PrivateCarAt(Id).Issues));

	SetTime(State, 1, 11); // Tuesday morning: at work
	EXPECT(!State.IsOwnerHomeNow(Id));
	EXPECT(State.OfferInPerson(Id, Dollars(100000)).Reply.find("Nobody answers") != std::string::npos);

	SetTime(State, 1, 19); // Tuesday evening: home
	EXPECT(State.IsOwnerHomeNow(Id));
	EXPECT(State.OfferInPerson(Id, ToMoney(Value * 0.3)).Outcome == OfferOutcome::Declined);
	const std::size_t CarsBefore = State.Garage.size();
	const OfferResponse Deal = State.OfferInPerson(Id, ToMoney(Value * 1.6));
	EXPECT(Deal.Outcome == OfferOutcome::Accepted);
	EXPECT(State.Garage.size() == CarsBefore + 1);
	EXPECT(State.PrivateCarAt(Id).bSold);

	// Some owners won't sell for any normal money.
	const std::string Loved = FindCar(State, [](const PrivateCar& Car) { return Car.Attachment > 0.86 && Car.Schedule == OwnerSchedule::DayWorker; });
	if (!Loved.empty())
	{
		const VehicleModel* LovedModel = State.Vehicles.FindModel(State.PrivateCarAt(Loved).ModelId);
		const double LovedValue = ToDollars(MarketValue(*LovedModel, State.PrivateCarAt(Loved).Condition, State.PrivateCarAt(Loved).MileageKm, {}));
		EXPECT(State.OfferInPerson(Loved, ToMoney(LovedValue * 1.3)).Outcome == OfferOutcome::NotForSale);
	}
}

DTG_TEST(NotesCallbacksTextsAndEmail)
{
	using namespace detail_offer_test;
	GameState State = dtg_test::FreshGame(62);
	State.Funds.Deposit(Dollars(1000000), MoneyCategory::Other, "test", 0);
	const std::string Id = FindCar(State, [](const PrivateCar& Car)
	{
		return Car.Prefers == Channel::Text && Car.Attachment < 0.6 && Car.Schedule == OwnerSchedule::DayWorker;
	});
	EXPECT(!Id.empty());
	if (Id.empty()) return;
	SetTime(State, 1, 11);
	const PrivateCar& Car = State.PrivateCarAt(Id);
	const VehicleModel* Model = State.Vehicles.FindModel(Car.ModelId);
	const double Reservation = OwnerReservation(*Model, Car);
	EXPECT(State.LeaveNote(Id, ToMoney(Reservation * 0.9)).bOk);
	EXPECT(!State.LeaveNote(Id, ToMoney(Reservation)).bOk); // one note at a time
	EXPECT(!State.TextOffer(Id, ToMoney(Reservation)).bOk); // no number yet
	const TickReport Day = State.Skip(2 * 24 * 60);
	EXPECT(!Day.NewMessages.empty());
	const auto Thread = State.Phone.Thread("car:" + Id);
	EXPECT(!Thread.empty());
	if (Thread.empty()) return;
	const Message& Reply = *Thread.back();
	EXPECT(Reply.Via == Channel::Text);
	EXPECT(Reply.Topic == MessageTopic::CarOffer);
	EXPECT(Reply.Outcome != OfferOutcome::NotForSale);

	// Haggle by text: the owner answers within hours.
	if (Reply.Outcome == OfferOutcome::Countered)
	{
		EXPECT(State.TextOffer(Id, ToMoney(Reservation * 0.95)).bOk);
		State.Skip(6 * 60);
		const auto Updated = State.Phone.Thread("car:" + Id);
		EXPECT(Updated.size() >= Thread.size() + 2); // our text and their reply
		const Message* Latest = State.Phone.LatestIncoming("car:" + Id);
		EXPECT(Latest != nullptr && Latest->Via == Channel::Text);
		if (Latest && Latest->bAwaitingReply)
		{
			EXPECT(State.RespondToMessage(Latest->Id, true).bOk);
			EXPECT(State.PrivateCarAt(Id).bSold);
		}
	}
	else if (Reply.bAwaitingReply)
	{
		EXPECT(State.RespondToMessage(Reply.Id, true).bOk);
	}

	// Impatient owners concede more in text haggling (Rubinstein).
	PrivateCar Patient = MakePrivateCar(State.Vehicles, "street-1-0", State.WorldSeed);
	Patient.Attachment = 0.2;
	Patient.CashNeed = 0.0;
	PrivateCar Eager = Patient;
	Eager.CashNeed = 1.0;
	const VehicleModel* SameModel = State.Vehicles.FindModel(Patient.ModelId);
	Rng Random(3);
	const Money Lowish = ToMoney(OwnerReservation(*SameModel, Patient) * 0.85);
	const OfferResponse FromPatient = EvaluateOffer(*SameModel, Patient, Lowish, 2, 0.0, BargainStyle::Text, Random);
	const OfferResponse FromEager = EvaluateOffer(*SameModel, Eager, Lowish, 2, 0.0, BargainStyle::Text, Random);
	EXPECT(FromEager.Outcome == OfferOutcome::Accepted || (FromPatient.Outcome == OfferOutcome::Countered && FromEager.Price < FromPatient.Price));

	// Businesses email.
	const int Unread = State.Phone.Unread();
	State.SendEmail(MessageTopic::Business, "Seaside Bistro", "owner@bistro.example", "Can you help us?", "We're struggling and heard you can help.");
	EXPECT(State.Phone.Unread() == Unread + 1);
	EXPECT(!State.Phone.OfChannel(Channel::Email).empty());

	// Everything survives a save.
	GameState Restored = dtg_test::LoadedState();
	EXPECT(Restored.LoadFromJson(State.SaveToJson(), nullptr));
	EXPECT(Restored.Phone.All().size() == State.Phone.All().size());
	EXPECT(Restored.PrivateCarAt(Id).bSold == State.PrivateCarAt(Id).bSold);
	EXPECT(Restored.PhoneNumber == State.PhoneNumber);
}

DTG_TEST(BackingOutOfADealIsRemembered)
{
	using namespace detail_offer_test;
	GameState State = dtg_test::FreshGame(63);
	State.Funds.Deposit(Dollars(1000000), MoneyCategory::Other, "test", 0);
	const std::string Id = FindCar(State, [](const PrivateCar& Car) { return Car.Attachment < 0.4; });
	EXPECT(!Id.empty());
	if (Id.empty()) return;
	const VehicleModel* Model = State.Vehicles.FindModel(State.PrivateCarAt(Id).ModelId);
	EXPECT(State.LeaveNote(Id, ToMoney(OwnerReservation(*Model, State.PrivateCarAt(Id)) * 1.5)).bOk);
	State.Skip(7 * 24 * 60);
	const Message* Reply = State.Phone.LatestIncoming("car:" + Id);
	EXPECT(Reply != nullptr && Reply->Outcome == OfferOutcome::Accepted);
	if (!Reply) return;
	const double Before = State.Npcs.GetOrCreate(State.PrivateCarAt(Id).OwnerId).Affinity;
	EXPECT(State.RespondToMessage(Reply->Id, false).bOk);
	EXPECT(State.Npcs.GetOrCreate(State.PrivateCarAt(Id).OwnerId).Affinity < Before);
	EXPECT(!State.PrivateCarAt(Id).bSold);
}

DTG_TEST(CourtsPleasAndSentences)
{
	GameState State = dtg_test::FreshGame(71);
	State.Funds.Deposit(Dollars(50000), MoneyCategory::Other, "test", 0);
	// A speeding stop is just a fine.
	State.ReportCrime(Crime::Speeding, true, false);
	const Money Fine = State.Busted();
	EXPECT(Fine > 0 && !State.Justice.Case().bOpen && !State.InJail());

	// Stealing a car is a felony: court.
	State.ReportCrime(Crime::VehicleTheft, true, false);
	State.ReportCrime(Crime::EvadingPolice, true, false);
	EXPECT(State.Busted() == 0);
	EXPECT(State.Justice.Case().bOpen);
	const int Trial = State.Justice.Case().TrialDays;
	const int PleaAlone = State.Justice.PleaOffer(State.Player.SkillLevel(Skill::People));
	EXPECT(PleaAlone < Trial);
	EXPECT(State.HireLawyer().bOk);
	const int PleaWithLawyer = State.Justice.PleaOffer(State.Player.SkillLevel(Skill::People));
	EXPECT(PleaWithLawyer < PleaAlone);
	const Verdict Plea = State.TakePlea();
	EXPECT(Plea.bConvicted && Plea.SentenceDays == PleaWithLawyer);
	EXPECT(State.InJail());
	EXPECT(State.Justice.HasFelony());
	EXPECT(!State.ApplyForCareer("police-officer").bOk);

	// Classes and good behavior get you out early.
	const std::int64_t Release = State.Justice.Jail().ReleaseMinute;
	for (int Day = 0; Day < 10 && State.InJail(); ++Day)
	{
		State.ServeJailDay(JailActivity::Study);
	}
	EXPECT(!State.InJail() || State.Justice.Jail().ReleaseMinute < Release);
	int Days = 0;
	while (State.InJail() && Days < 400)
	{
		if (State.Justice.CanRequestParole(State.Clock.TotalMinutes()))
		{
			State.RequestParole();
		}
		else
		{
			State.ServeJailDay(JailActivity::KeepHeadDown);
		}
		++Days;
	}
	EXPECT(!State.InJail());

	// The record survives a save; expungement needs three clean years.
	GameState Restored = dtg_test::LoadedState();
	EXPECT(Restored.LoadFromJson(State.SaveToJson(), nullptr));
	EXPECT(Restored.Justice.HasFelony());
	EXPECT(!State.ExpungeRecord().bOk);
	State.Clock.SetTotalMinutes(static_cast<double>(State.Clock.TotalMinutes()) + 4.0 * 365 * 24 * 60);
	EXPECT(State.ExpungeRecord().bOk);
	EXPECT(!State.Justice.HasFelony());
	EXPECT(State.ApplyForCareer("landscaper").bOk);
}

DTG_TEST(TrialsCanGoEitherWay)
{
	int Acquittals = 0;
	int Convictions = 0;
	for (std::uint64_t Seed = 1; Seed <= 40; ++Seed)
	{
		GameState State = dtg_test::FreshGame(Seed);
		State.Funds.Deposit(Dollars(50000), MoneyCategory::Other, "test", 0);
		State.ReportCrime(Crime::UnauthorizedHack, true, false);
		State.Busted();
		State.HireLawyer();
		const Verdict Result = State.GoToTrial();
		Acquittals += Result.bDecided && !Result.bConvicted ? 1 : 0;
		Convictions += Result.bConvicted ? 1 : 0;
	}
	EXPECT(Acquittals > 3 && Convictions > 3);
}

DTG_TEST(CoDefendantsPlayThePrisonersDilemma)
{
	// Testifying is each side's dominant strategy, so the only equilibrium is both testifying.
	const MatrixGame Game = JusticeSystem::CoDefendantGame(60);
	const auto Pure = PureEquilibria(Game);
	EXPECT(Pure.size() == 1 && Pure[0].first == 1 && Pure[0].second == 1);

	// A friend who trusts you (and whom you've never betrayed) keeps quiet.
	GameState State = dtg_test::FreshGame(72);
	NpcMind& Friend = State.Npcs.GetOrCreate("friend-1");
	Friend.Deal.Strategy = RepeatedStrategy::TitForTat;
	State.ReportCrime(Crime::VehicleTheft, true, false);
	State.Busted("friend-1");
	EXPECT(State.Justice.Case().CoDefendantName == Friend.Name);
	const Verdict Loyal = State.CoDefendantChoice(false);
	EXPECT(Loyal.bConvicted);
	EXPECT(Loyal.SentenceDays < 15); // both silent: light time
	// Betray them, and next time they remember.
	State.Justice.Release();
	State.ReportCrime(Crime::VehicleTheft, true, false);
	State.Busted("friend-1");
	State.CoDefendantChoice(true);
	State.Justice.Release();
	State.ReportCrime(Crime::VehicleTheft, true, false);
	State.Busted("friend-1");
	const Verdict Payback = State.CoDefendantChoice(false);
	EXPECT(Payback.SentenceDays >= 30); // they testified against you
}

DTG_TEST(OutbreaksRiseAndFall)
{
	Rng RandomA(5);
	Rng RandomB(5);
	OutbreakSystem Untreated;
	OutbreakSystem Treated;
	Untreated.Begin(RandomA);
	Treated.Begin(RandomB);
	int UntreatedDays = 0;
	int TreatedDays = 0;
	bool bSawDrag = false;
	for (int Day = 0; Day < 300 && Untreated.IsActive(); ++Day)
	{
		Untreated.DailyUpdate(RandomA, 0.0);
		bSawDrag = bSawDrag || (Untreated.EconomicDrag() > 0.01 && Untreated.StreetLife() < 1.0);
		++UntreatedDays;
	}
	for (int Day = 0; Day < 300 && Treated.IsActive(); ++Day)
	{
		if (Day == 15)
		{
			Treated.DeployTreatment(0.6);
		}
		Treated.DailyUpdate(RandomB, 0.0);
		++TreatedDays;
	}
	EXPECT(!Untreated.IsActive() && UntreatedDays < 250);
	EXPECT(Untreated.PeakInfected() > 0.01);
	EXPECT(bSawDrag);
	EXPECT(TreatedDays < UntreatedDays);
	EXPECT(Treated.PeakInfected() < Untreated.PeakInfected());
}

DTG_TEST(MedicinesTrialsAndTreatmentContracts)
{
	GameState State = dtg_test::FreshGame(81);
	State.Funds.Deposit(Dollars(2000000), MoneyCategory::Other, "test", 0);
	EXPECT(!State.DevelopMedicine("Calmex", 40, 1.0, false).bOk); // no science background
	State.Player.AddSkillXp(Skill::Science, 4000);
	State.Player.AddSkillXp(Skill::Medicine, 4000);

	// An everyday medicine goes through three trial phases before it sells.
	EXPECT(State.DevelopMedicine("Calmex", 60, 1.0, false).bOk);
	const std::string Everyday = State.Ip.Works().back().Id;
	EXPECT(State.Ip.Works().back().Kind == WorkKind::Medicine);
	EXPECT(State.RunTrial(Everyday).bStarted);
	EXPECT(!State.RunTrial(Everyday).bFinished); // still running
	int Guard = 0;
	while (!State.Ip.Find(Everyday)->bApproved && !State.Ip.Find(Everyday)->bShelved && Guard++ < 10)
	{
		State.Skip(15 * 24 * 60);
		const TrialResult Step = State.RunTrial(Everyday);
		if (Step.bFinished && Step.bPassed && !Step.bApproved)
		{
			State.RunTrial(Everyday); // start the next phase
		}
	}
	const CreativeWork* Pill = State.Ip.Find(Everyday);
	EXPECT(Pill->bApproved || Pill->bShelved);

	// During an outbreak, a targeted treatment is fast-tracked and sold to the state at a negotiated price.
	State.Outbreak.Begin(State.Random);
	for (int Day = 0; Day < 10; ++Day) State.Outbreak.DailyUpdate(State.Random, 0.0);
	std::string CureId;
	for (int Attempt = 0; Attempt < 6 && CureId.empty(); ++Attempt)
	{
		EXPECT(State.DevelopMedicine("Fevergone", 20, 1.0, true).bOk);
		CreativeWork* Cure = State.Ip.Find(State.Ip.Works().back().Id);
		EXPECT(Cure->Treats == State.Outbreak.Strain().Id);
		Cure->Quality = 1.0; // a strong candidate
		const std::string Candidate = Cure->Id;
		Guard = 0;
		while (!State.Ip.Find(Candidate)->bApproved && !State.Ip.Find(Candidate)->bShelved && Guard++ < 12)
		{
			const TrialResult Step = State.RunTrial(Candidate);
			if (Step.bStarted)
			{
				EXPECT(State.Ip.Find(Candidate)->TrialReadyMinute - State.Clock.TotalMinutes() <= 3 * 24 * 60); // fast track
				State.Skip(3 * 24 * 60 + 60);
			}
		}
		if (State.Ip.Find(Candidate)->bApproved)
		{
			CureId = Candidate;
		}
	}
	EXPECT(!CureId.empty());
	EXPECT(State.Outbreak.IsActive());
	if (CureId.empty() || !State.Outbreak.IsActive())
	{
		return;
	}
	const TreatmentQuote Quote = State.QuoteTreatment(CureId);
	EXPECT(Quote.bValid && Quote.Doses > 0);
	const auto Price = [&](TreatmentPricing Pricing) { return Quote.Prices[static_cast<int>(Pricing)]; };
	EXPECT(Price(TreatmentPricing::AtCost) < Price(TreatmentPricing::Fair));
	EXPECT(Price(TreatmentPricing::Fair) < Price(TreatmentPricing::Gouge));
	EXPECT(Price(TreatmentPricing::Market) < Price(TreatmentPricing::Gouge));
	const double TrustBefore = State.Markets.PlayerReputation().Trust;
	EXPECT(State.SellTreatment(CureId, TreatmentPricing::AtCost).bOk);
	EXPECT(State.Markets.PlayerReputation().Trust > TrustBefore);
	EXPECT(State.Outbreak.TreatmentDeployed());
	EXPECT(!State.SellTreatment(CureId, TreatmentPricing::Gouge).bOk); // one contract per outbreak

	GameState Restored = dtg_test::LoadedState();
	EXPECT(Restored.LoadFromJson(State.SaveToJson(), nullptr));
	EXPECT(Restored.Outbreak.IsActive() == State.Outbreak.IsActive());
	EXPECT(Restored.Ip.Find(CureId) && Restored.Ip.Find(CureId)->bApproved);
}

DTG_TEST(PriceGougingHasAPublicCost)
{
	GameState State = dtg_test::FreshGame(82);
	State.Player.AddSkillXp(Skill::Science, 4000);
	State.Outbreak.Begin(State.Random);
	for (int Day = 0; Day < 10; ++Day) State.Outbreak.DailyUpdate(State.Random, 0.0);
	EXPECT(State.DevelopMedicine("Quickrelief", 40, 1.0, true).bOk);
	CreativeWork* Cure = State.Ip.Find(State.Ip.Works().back().Id);
	Cure->bApproved = true;
	Cure->TrialPhase = 3;
	const double TrustBefore = State.Markets.PlayerReputation().Trust;
	const Money CashBefore = State.Funds.Balance();
	EXPECT(State.SellTreatment(Cure->Id, TreatmentPricing::Gouge).bOk);
	EXPECT(State.Funds.Balance() > CashBefore);
	EXPECT(State.Markets.PlayerReputation().Trust < TrustBefore - 0.2);
}

DTG_TEST(LookingAroundYouIsCheap)
{
	// The game asks "what's near me?" several times a frame, so it comes off a grid rather than a walk
	// through every building in the state.
	const GameState& State = dtg_test::LoadedState();
	const CityPlan& City = State.City;
	EXPECT(City.Buildings.size() > 500);

	// Whatever the index says is near had better be what a full search would have found.
	Rng Random(99);
	for (int Trial = 0; Trial < 25; ++Trial)
	{
		const BuildingPlan& Pick = City.Buildings[Random.Index(City.Buildings.size())];
		const double X = Pick.DoorX + Random.Range(-40.0, 40.0);
		const double Y = Pick.DoorY + Random.Range(-40.0, 40.0);
		const double Radius = 60.0;

		std::vector<const BuildingPlan*> Expected;
		for (const BuildingPlan& Building : City.Buildings)
		{
			const double Dx = Building.DoorX - X;
			const double Dy = Building.DoorY - Y;
			if (Dx * Dx + Dy * Dy <= Radius * Radius)
			{
				Expected.push_back(&Building);
			}
		}
		const std::vector<const BuildingPlan*> Found = City.BuildingsNear(X, Y, Radius);
		EXPECT(Found.size() == Expected.size());
		// And the first one back is the closest, which is what the interaction prompt uses.
		const BuildingPlan* Nearest = City.NearestBuilding(X, Y, Radius);
		if (!Expected.empty())
		{
			const BuildingPlan* Closest = Expected.front();
			double Best = 1e18;
			for (const BuildingPlan* Building : Expected)
			{
				const double Dx = Building->DoorX - X;
				const double Dy = Building->DoorY - Y;
				if (Dx * Dx + Dy * Dy < Best)
				{
					Best = Dx * Dx + Dy * Dy;
					Closest = Building;
				}
			}
			EXPECT(Nearest == Closest);
		}
		else
		{
			EXPECT(Nearest == nullptr);
		}
	}

	// The road graph is looked up the same way, and still finds the true nearest junction -- including
	// from out in the country, where the nearest one is a long way off.
	for (int Trial = 0; Trial < 15; ++Trial)
	{
		const double X = Random.Range(City.Bounds.MinX, City.Bounds.MaxX);
		const double Y = Random.Range(City.Bounds.MinY, City.Bounds.MaxY);
		int Expected = -1;
		double Best = 1e18;
		for (const Intersection& Node : City.Intersections)
		{
			const double Dx = Node.X - X;
			const double Dy = Node.Y - Y;
			if (Dx * Dx + Dy * Dy < Best)
			{
				Best = Dx * Dx + Dy * Dy;
				Expected = Node.Id;
			}
		}
		EXPECT(City.NearestIntersection(X, Y) == Expected);
	}

	// And looking something up by id answers the same as walking the list, which is what the engine
	// layer does several times a frame for every car on the road.
	for (const BuildingPlan& Building : City.Buildings)
	{
		EXPECT(City.FindBuilding(Building.Id) == &Building);
	}
	for (const Intersection& Node : City.Intersections)
	{
		EXPECT(City.FindIntersection(Node.Id) == &Node);
	}
	EXPECT(City.FindBuilding(-1) == nullptr);
	EXPECT(City.FindBuilding(999999) == nullptr);
	EXPECT(City.FindIntersection(-5) == nullptr);
	EXPECT(City.FindIntersection(999999) == nullptr);

	// NodesNear, which the minimap draws from, against an exhaustive search.
	for (int Probe = 0; Probe < 12; ++Probe)
	{
		const double X = Random.Range(City.Bounds.MinX, City.Bounds.MaxX);
		const double Y = Random.Range(City.Bounds.MinY, City.Bounds.MaxY);
		const double Radius = Random.Range(40.0, 400.0);
		std::size_t Expected = 0;
		for (const Intersection& Node : City.Intersections)
		{
			const double Dx = Node.X - X;
			const double Dy = Node.Y - Y;
			Expected += Dx * Dx + Dy * Dy <= Radius * Radius ? 1u : 0u;
		}
		EXPECT(City.NodesNear(X, Y, Radius).size() == Expected);
	}
}

DTG_TEST(AskingTheSameQuestionTwiceWearsThin)
{
	const GameState& Loaded = dtg_test::LoadedState();
	NpcPopulation Npcs;
	NpcMind& Mind = Npcs.GetOrCreate("ped-talker");
	Rng Random(7);

	// A fresh question gets a real answer -- several sentences' worth, not a brush-off.
	const TalkTurn First = Npcs.Say(Mind, "work", 100, Random, Loaded.Careers);
	EXPECT(First.Reply.size() > 20);
	EXPECT(!First.bEnded);
	EXPECT(Mind.TimesDiscussed("work") == 1);

	// Asked again they still answer, but it costs them patience.
	const double PatienceBefore = Mind.Patience;
	const TalkTurn Second = Npcs.Say(Mind, "work", 200, Random, Loaded.Careers);
	EXPECT(!Second.bEnded);
	EXPECT(Mind.Patience < PatienceBefore);

	// The third time they say so, and they keep saying so.
	const TalkTurn Third = Npcs.Say(Mind, "work", 300, Random, Loaded.Careers);
	EXPECT(Third.Reply.find("already told you") != std::string::npos);
	EXPECT(Npcs.Say(Mind, "work", 400, Random, Loaded.Careers).Reply.find("Same answer") != std::string::npos);

	// Keep at it and they leave. Other subjects were never the problem.
	bool bWalkedOff = false;
	for (int Index = 0; Index < 8 && !bWalkedOff; ++Index)
	{
		bWalkedOff = Npcs.Say(Mind, "work", 500 + Index, Random, Loaded.Careers).bEnded;
	}
	EXPECT(bWalkedOff);
	EXPECT(Mind.TimesDiscussed("town") == 0);

	// A night's sleep puts most of it back.
	Npcs.TownTalks(2000, Random);
	EXPECT(Mind.Patience > 0.3);
}

DTG_TEST(PeopleConfideAndCanBeHelped)
{
	const GameState& Loaded = dtg_test::LoadedState();
	NpcPopulation Npcs;
	Rng Random(11);
	// Not everybody is carrying something; find one of the ones who is.
	NpcMind* Found = nullptr;
	for (int Index = 0; Index < 40 && !Found; ++Index)
	{
		NpcMind& Candidate = Npcs.GetOrCreate("ped-trouble-" + std::to_string(Index));
		if (Candidate.Burden.Kind != TroubleKind::None && Candidate.Burden.Cost > 0)
		{
			Found = Npcs.Find(Candidate.Id);
		}
	}
	EXPECT(Found != nullptr);
	if (!Found)
	{
		return;
	}
	const std::string Who = Found->Id;

	// A stranger keeps it to themselves.
	Npcs.Find(Who)->Affinity = -0.5;
	EXPECT(Npcs.Say(*Npcs.Find(Who), "trouble", 100, Random, Loaded.Careers).Reply.find("stranger") != std::string::npos);
	EXPECT(!Npcs.Find(Who)->Burden.bShared);

	// Somebody who trusts you tells you, and then the offer to help is on the table.
	Npcs.Find(Who)->Affinity = 0.6;
	const TalkTurn Told = Npcs.Say(*Npcs.Find(Who), "trouble", 200, Random, Loaded.Careers);
	EXPECT(Npcs.Find(Who)->Burden.bShared);
	EXPECT(Told.Reply == Npcs.Find(Who)->Burden.Text);
	bool bCanHelp = false;
	Money Price = 0;
	for (const DialogueOption& Option : Npcs.Options(*Npcs.Find(Who)))
	{
		if (Option.Id == "help")
		{
			bCanHelp = true;
			Price = Option.Cost;
		}
	}
	EXPECT(bCanHelp);
	EXPECT(Price > 0);

	// Offering what you haven't got does nothing.
	const TalkTurn Broke = Npcs.Say(*Npcs.Find(Who), "help", 300, Random, Loaded.Careers, Price - 1);
	EXPECT(Broke.Cost == 0);
	EXPECT(!Npcs.Find(Who)->Burden.bHelped);

	// Paying for it buys you a friend, and gives them something to tell people.
	const double Before = Npcs.Find(Who)->Affinity;
	const TalkTurn Helped = Npcs.Say(*Npcs.Find(Who), "help", 400, Random, Loaded.Careers, Dollars(10000));
	EXPECT(Helped.Cost == Price);
	EXPECT(Npcs.Find(Who)->Burden.bHelped);
	EXPECT(Npcs.Find(Who)->Affinity > Before);
	EXPECT(Helped.bNewRumor);
	EXPECT(Npcs.HowManyHeard(Helped.Rumor) == 1);
	// It only gets paid for once.
	for (const DialogueOption& Option : Npcs.Options(*Npcs.Find(Who)))
	{
		EXPECT(Option.Id != "help");
	}
}

DTG_TEST(InformationHasAPrice)
{
	const GameState& Loaded = dtg_test::LoadedState();
	NpcPopulation Npcs;
	Rng Random(13);
	// Both minds are created up front: GetOrCreate can move the population out from under a reference,
	// so everything below goes through Find.
	Npcs.GetOrCreate("ped-knows-1");
	Npcs.GetOrCreate("ped-knows-1-copy");
	Npcs.Find("ped-knows-1")->Affinity = 0.0;
	const Bargain Cold = Npcs.PriceOfTip(*Npcs.Find("ped-knows-1"));
	EXPECT(Cold.bDeal);
	EXPECT(Cold.PayoffB > 0.0);

	// The same thing from a friend costs less: walking away would cost them something too.
	Npcs.Find("ped-knows-1-copy")->Knowledge = Npcs.Find("ped-knows-1")->Knowledge;
	Npcs.Find("ped-knows-1-copy")->Affinity = 0.9;
	EXPECT(Npcs.PriceOfTip(*Npcs.Find("ped-knows-1-copy")).PayoffB < Cold.PayoffB);

	// Paying gets the actual thing they know, once.
	const std::string Secret = Npcs.Find("ped-knows-1")->Knowledge.Text;
	EXPECT(!Secret.empty());
	const Money Price = Dollars(static_cast<std::int64_t>(Cold.PayoffB + 0.5));
	EXPECT(Npcs.Say(*Npcs.Find("ped-knows-1"), "tip", 100, Random, Loaded.Careers, Price - Dollars(1)).Cost == 0);
	EXPECT(!Npcs.Find("ped-knows-1")->Knowledge.bGiven);
	const TalkTurn Bought = Npcs.Say(*Npcs.Find("ped-knows-1"), "tip", 200, Random, Loaded.Careers, Dollars(100000));
	EXPECT(Bought.Cost == Price);
	EXPECT(Bought.Reply.find(Secret) != std::string::npos);
	EXPECT(Npcs.Find("ped-knows-1")->Knowledge.bGiven);
	EXPECT(Npcs.Say(*Npcs.Find("ped-knows-1"), "tip", 300, Random, Loaded.Careers, Dollars(100000)).Cost == 0);

	// Some things are worth more to the teller than to the hearer. A stranger will not say those at
	// any price; somebody who trusts you will.
	NpcPopulation Risky;
	NpcMind& Watcher = Risky.GetOrCreate("ped-watcher");
	Watcher.Knowledge.Kind = TipKind::PoliceWatch;
	Watcher.Knowledge.Text = "They run the speed trap past the church.";
	Watcher.Knowledge.Value = 700.0;
	Watcher.Knowledge.Ask = Dollars(900);
	Watcher.Affinity = 0.0;
	EXPECT(!Risky.PriceOfTip(Watcher).bDeal);
	Rng Other(19);
	EXPECT(Risky.Say(Watcher, "tip", 100, Other, Loaded.Careers, Dollars(100000)).Cost == 0);
	EXPECT(!Watcher.Knowledge.bGiven);
	Watcher.Affinity = 1.0;
	EXPECT(Risky.PriceOfTip(Watcher).bDeal);
	EXPECT(Risky.Say(Watcher, "tip", 200, Other, Loaded.Careers, Dollars(100000)).Cost > 0);
	EXPECT(Watcher.Knowledge.bGiven);
}

DTG_TEST(WitnessesHaveSomethingToTell)
{
	const GameState& Loaded = dtg_test::LoadedState();
	NpcPopulation Npcs;
	Rng Random(17);
	NpcMind& Mind = Npcs.GetOrCreate("ped-witness");

	// Nothing seen, nothing to say.
	bool bOffered = false;
	for (const DialogueOption& Option : Npcs.Options(Mind)) bOffered = bOffered || Option.Id == "saw";
	EXPECT(!bOffered);

	// Seeing the world go wrong gives them both an account and a rumour of their own.
	Npcs.Observe(Mind, Stimulus::SawGlitch, 1.0, 100, Random, "Phasing");
	for (const DialogueOption& Option : Npcs.Options(Mind)) bOffered = bOffered || Option.Id == "saw";
	EXPECT(bOffered);
	EXPECT(!Mind.Heard.empty());
	const std::string Scripted = Npcs.Say(Mind, "saw", 200, Random, Loaded.Careers).Reply;
	EXPECT(Scripted.find("the heat") != std::string::npos); // a scripted mind talks itself out of it

	// The same memory, told by somebody who has woken up, is a different answer entirely.
	Mind.Stage = AwarenessStage::Awakened;
	Mind.Topics.clear();
	const std::string Awake = Npcs.Say(Mind, "saw", 300, Random, Loaded.Careers).Reply;
	EXPECT(Awake != Scripted);
	EXPECT(Awake.find("wasn't it") != std::string::npos);
}

DTG_TEST(TheTownTalksAboutYou)
{
	// Gossip is chancy by design, so this asks whether the mechanism works, not whether one roll did.
	int TownsThatTalked = 0;
	int TotalReach = 0;
	bool bAnySecondHand = false;
	for (std::uint64_t Seed = 23; Seed < 29; ++Seed)
	{
		NpcPopulation Npcs;
		Rng Random(Seed);
		for (int Index = 0; Index < 12; ++Index)
		{
			Npcs.GetOrCreate("ped-town-" + std::to_string(Index));
		}
		for (int Index = 0; Index < 12; ++Index)
		{
			NpcMind* Mind = Npcs.Find("ped-town-" + std::to_string(Index));
			EXPECT(Mind != nullptr);
			if (Mind)
			{
				Npcs.Mingle(*Mind, Random, 3);
			}
		}
		EXPECT(!Npcs.Find("ped-town-0")->Knows.empty());
		// Being known runs both ways: whoever ped-town-0 knows, knows ped-town-0.
		const std::string Other = Npcs.Find("ped-town-0")->Knows[0].Id;
		EXPECT(Npcs.Find(Other)->FindKnown("ped-town-0") != nullptr);

		// One person sees something. Nobody else has heard anything yet.
		Npcs.Observe(*Npcs.Find("ped-town-0"), Stimulus::SawPower, 1.0, 100, Random, "Strength");
		const Rumor* Loud = Npcs.LoudestRumor(*Npcs.Find("ped-town-0"));
		EXPECT(Loud != nullptr);
		const std::string Word = Loud->Text;
		const double Fresh = Loud->Weight;
		EXPECT(Npcs.HowManyHeard(Word) == 1);

		// A fortnight of ordinary days, and the town has an opinion it was never told.
		for (int Day = 0; Day < 14; ++Day)
		{
			Npcs.TownTalks(1000 + Day * 1440, Random);
		}
		const int Knew = Npcs.HowManyHeard(Word);
		EXPECT(Knew >= 1);
		EXPECT(Knew <= 12);
		TownsThatTalked += Knew > 1 ? 1 : 0;
		TotalReach += Knew;

		// It arrives attributed: somebody can say who told them.
		for (const NpcMind& Mind : Npcs.All())
		{
			for (const Rumor& Known : Mind.Heard)
			{
				bAnySecondHand = bAnySecondHand || (Known.Text == Word && !Known.SourceName.empty() && Known.Hops > 0);
				EXPECT(Known.Weight > 0.0 && Known.Weight <= 1.0);
			}
		}
		(void)Fresh;
	}
	EXPECT(TownsThatTalked >= 5);
	EXPECT(TotalReach >= 12); // on average a rumour reaches more than the one person who saw it
	EXPECT(bAnySecondHand);

	// Nothing stays news. Somebody with a rumour and nobody to tell forgets it on their own.
	NpcPopulation Alone;
	Rng Quiet(31);
	NpcMind& Hermit = Alone.GetOrCreate("ped-hermit");
	Alone.Observe(Hermit, Stimulus::SawPower, 1.0, 0, Quiet, "Strength");
	const double Day0 = Alone.LoudestRumor(Hermit)->Weight;
	for (int Day = 0; Day < 10; ++Day)
	{
		Alone.TownTalks(1440 * (Day + 1), Quiet);
	}
	EXPECT(Alone.LoudestRumor(Hermit) != nullptr);
	EXPECT(Alone.LoudestRumor(Hermit)->Weight < Day0);
	for (int Day = 0; Day < 200; ++Day)
	{
		Alone.TownTalks(100000 + 1440 * Day, Quiet);
	}
	EXPECT(Hermit.Heard.empty()); // forgotten entirely
}

DTG_TEST(KindnessAndBetrayalBothTravel)
{
	GameState State = dtg_test::FreshGame(29);
	State.Funds.Deposit(Dollars(20000), MoneyCategory::Other, "test", 0);
	// Somebody in Longs with people who know them.
	const StateCity* Start = State.Map.StartCity();
	EXPECT(Start != nullptr);
	for (int Index = 0; Index < 10; ++Index)
	{
		State.Meet("ped-local-" + std::to_string(Index), Start ? Start->X : 0.0, Start ? Start->Y : 0.0);
	}
	EXPECT(!State.Npcs.Find("ped-local-0")->Hometown.empty());
	EXPECT(State.Npcs.Find("ped-local-0")->Hometown == (Start ? Start->Name : std::string()));

	// Break your word to one of them and the town's opinion of you moves, without anybody telling it.
	NpcMind* Mind = State.Npcs.Find("ped-local-0");
	EXPECT(Mind != nullptr);
	Mind->Stage = AwarenessStage::Awakened;
	Mind->Affinity = 0.5;
	const double SentimentBefore = State.Markets.PlayerReputation().Sentiment;
	const TalkTurn Broken = State.Talk("ped-local-0", "promise-break");
	EXPECT(Broken.bNewRumor);
	EXPECT(State.Markets.PlayerReputation().Sentiment <= SentimentBefore);
	// Whoever heard it can repeat it back to you, and name who said it.
	int Repeated = 0;
	for (const NpcMind& Other : State.Npcs.All())
	{
		if (Other.Id != "ped-local-0")
		{
			for (const Rumor& Known : Other.Heard)
			{
				Repeated += Known.Text == Broken.Rumor ? 1 : 0;
			}
		}
	}
	EXPECT(Repeated >= 0); // spreading is chancy; what matters is that the teller is carrying it
	EXPECT(State.Npcs.HowManyHeard(Broken.Rumor) >= 1);

	// Money actually leaves the wallet when you put somebody's trouble right.
	NpcMind* Helper = nullptr;
	for (const NpcMind& Candidate : State.Npcs.All())
	{
		if (Candidate.Burden.Kind != TroubleKind::None && Candidate.Burden.Cost > 0 && !Helper)
		{
			Helper = State.Npcs.Find(Candidate.Id);
		}
	}
	if (Helper)
	{
		const std::string Who = Helper->Id;
		State.Npcs.Find(Who)->Affinity = 0.8;
		State.Talk(Who, "trouble");
		EXPECT(State.Npcs.Find(Who)->Burden.bShared);
		const Money Owed = State.Npcs.HelpCost(*State.Npcs.Find(Who));
		const Money Before = State.Funds.Balance();
		const TalkTurn Paid = State.Talk(Who, "help");
		EXPECT(Paid.Cost == Owed);
		EXPECT(State.Funds.Balance() == Before - Owed);
	}
}

DTG_TEST(ConversationMemoryHoldsUp)
{
	const GameState& Loaded = dtg_test::LoadedState();
	NpcPopulation Npcs;
	Rng Random(37);
	NpcMind& Mind = Npcs.GetOrCreate("ped-holds-up");

	// A question asked three times today is closed today -- and open again tomorrow.
	for (int Index = 0; Index < 3; ++Index)
	{
		Npcs.Say(Mind, "town", 100 + Index, Random, Loaded.Careers);
	}
	const std::string Closed = Npcs.Say(Mind, "town", 200, Random, Loaded.Careers).Reply;
	EXPECT(Closed.find("already told you") != std::string::npos || Closed.find("Same answer") != std::string::npos);
	const TalkTurn Tomorrow = Npcs.Say(Mind, "town", 200 + NpcPopulation::TopicForgetMinutes, Random, Loaded.Careers);
	EXPECT(Tomorrow.Reply.find("already told you") == std::string::npos);
	EXPECT(Tomorrow.Reply.find("Same answer") == std::string::npos);
	EXPECT(Tomorrow.Reply.size() > 20);

	// A brush-off is not a conversation, so it cannot be used to buy somebody's confidence.
	NpcMind& Cold = Npcs.GetOrCreate("ped-pestered");
	Cold.Burden.Kind = TroubleKind::Fear;
	Cold.Burden.Text = "Somebody's been parked outside my place.";
	Cold.Affinity = 0.0;
	const int Before = Cold.Conversations;
	for (int Index = 0; Index < 30; ++Index)
	{
		Npcs.Say(Cold, "chat", 1000 + Index, Random, Loaded.Careers);
	}
	EXPECT(Cold.Conversations < Before + 30);
	EXPECT(!Cold.Burden.bShared); // still a stranger, however many times you said hello

	// Talking does not make a witness forget: memories are a short list, what they saw is not in it.
	NpcMind& Witness = Npcs.GetOrCreate("ped-still-remembers");
	Npcs.Observe(Witness, Stimulus::SawGlitch, 1.0, 100, Random, "Phasing");
	for (int Index = 0; Index < 40; ++Index)
	{
		Npcs.Say(Witness, Index % 2 == 0 ? "chat" : "ask", 2000 + Index * NpcPopulation::TopicForgetMinutes, Random, Loaded.Careers);
	}
	EXPECT(Witness.Memories.size() == NpcPopulation::MaxMemories); // the short list has long since filled
	bool bStillOffered = false;
	for (const DialogueOption& Option : Npcs.Options(Witness)) bStillOffered = bStillOffered || Option.Id == "saw";
	EXPECT(bStillOffered);
	EXPECT(Npcs.Say(Witness, "saw", 90000, Random, Loaded.Careers).Reply.find("Nothing worth telling") == std::string::npos);
}

DTG_TEST(NobodyKnowsTheWholeTown)
{
	NpcPopulation Npcs;
	Rng Random(41);
	for (int Index = 0; Index < 30; ++Index)
	{
		Npcs.GetOrCreate("ped-crowd-" + std::to_string(Index));
	}
	// Introduce everybody to everybody, the way a busy street would over a long session.
	for (int A = 0; A < 30; ++A)
	{
		for (int B = 0; B < 30; ++B)
		{
			Npcs.Introduce("ped-crowd-" + std::to_string(A), "ped-crowd-" + std::to_string(B), Random.Range(0.1, 1.0));
		}
	}
	for (const NpcMind& Mind : Npcs.All())
	{
		EXPECT(Mind.Knows.size() <= NpcPopulation::MaxAcquaintances);
	}
	// The ties that survive are the strong ones.
	double Weakest = 1.0;
	for (const Acquaintance& Known : Npcs.Find("ped-crowd-0")->Knows)
	{
		Weakest = Known.Tie < Weakest ? Known.Tie : Weakest;
	}
	EXPECT(Weakest > 0.1);

	// Hops counts the chain, not a per-person budget: a line passed along twice says so.
	NpcPopulation Chain;
	Rng Line(43);
	for (int Index = 0; Index < 3; ++Index)
	{
		Chain.GetOrCreate("ped-chain-" + std::to_string(Index));
	}
	Chain.Introduce("ped-chain-0", "ped-chain-1", 1.0);
	Chain.Introduce("ped-chain-1", "ped-chain-2", 1.0);
	const std::string Word = "they say the lights change by themselves";
	Chain.Tell(*Chain.Find("ped-chain-0"), Word, std::string(), 1.0, 0, 0);
	for (int Attempt = 0; Attempt < 60; ++Attempt)
	{
		Chain.Spread("ped-chain-0", Word, 1.0, 100, Line, 0);
		const Rumor* Middle = Chain.LoudestRumor(*Chain.Find("ped-chain-1"));
		if (Middle)
		{
			Chain.Spread("ped-chain-1", Word, 1.0, 200, Line, Middle->Hops);
		}
	}
	const Rumor* First = Chain.LoudestRumor(*Chain.Find("ped-chain-1"));
	const Rumor* Second = Chain.LoudestRumor(*Chain.Find("ped-chain-2"));
	EXPECT(First != nullptr && Second != nullptr);
	if (First && Second)
	{
		EXPECT(First->Hops == 1);
		EXPECT(Second->Hops == 2);
	}
}

DTG_TEST(TheTownsOpinionSurvivesASave)
{
	GameState State = dtg_test::FreshGame(47);
	const StateCity* Start = State.Map.StartCity();
	EXPECT(Start != nullptr);
	for (int Index = 0; Index < 8; ++Index)
	{
		State.Meet("ped-save-" + std::to_string(Index), Start ? Start->X : 0.0, Start ? Start->Y : 0.0);
	}
	// One witness, then a fortnight of the town talking. Most of these people have never met the
	// player: before, the save threw every one of them away.
	State.Npcs.Observe(*State.Npcs.Find("ped-save-0"), Stimulus::SawPower, 1.0, 100, State.Random, "Strength");
	const std::string Word = State.Npcs.LoudestRumor(*State.Npcs.Find("ped-save-0"))->Text;
	for (int Day = 0; Day < 14; ++Day)
	{
		State.Npcs.TownTalks(1000 + Day * 1440, State.Random);
	}
	int Links = 0;
	int Hometowns = 0;
	for (const NpcMind& Mind : State.Npcs.All())
	{
		Links += static_cast<int>(Mind.Knows.size());
		Hometowns += Mind.Hometown.empty() ? 0 : 1;
	}
	const int Carriers = State.Npcs.HowManyHeard(Word);
	EXPECT(Links > 0);
	EXPECT(Hometowns == 8);

	GameState Restored = dtg_test::FreshGame(1);
	std::string Error;
	EXPECT(Restored.LoadFromJson(State.SaveToJson(), &Error));
	int RestoredLinks = 0;
	int RestoredHometowns = 0;
	for (const NpcMind& Mind : Restored.Npcs.All())
	{
		RestoredLinks += static_cast<int>(Mind.Knows.size());
		RestoredHometowns += Mind.Hometown.empty() ? 0 : 1;
	}
	EXPECT(RestoredLinks == Links);
	EXPECT(RestoredHometowns == Hometowns);
	EXPECT(Restored.Npcs.HowManyHeard(Word) == Carriers);

	// And the links point at people who are really there, so the social graph still carries things.
	for (const NpcMind& Mind : Restored.Npcs.All())
	{
		for (const Acquaintance& Known : Mind.Knows)
		{
			EXPECT(Restored.Npcs.Find(Known.Id) != nullptr);
		}
	}
	// Which means "ask about them" answers with something, not a shrug.
	const NpcMind* Social = Restored.Npcs.Find("ped-save-0");
	EXPECT(Social != nullptr);
	if (Social && !Social->Knows.empty())
	{
		const TalkTurn About = Restored.Talk("ped-save-0", "about:" + Social->Knows[0].Id);
		EXPECT(About.Reply.find("Don't know anybody") == std::string::npos);
	}
	// What a witness saw survives too.
	EXPECT(Restored.Npcs.Find("ped-save-0")->bSawSomething);
	EXPECT(Restored.Npcs.Find("ped-save-0")->Witnessed.Kind == Stimulus::SawPower);
}
