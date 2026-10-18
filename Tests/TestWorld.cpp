#include "TestFramework.h"
#include "TestSupport.h"

#include <cstdio>
#include <cstdlib>
#include <set>

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
	for (const Brand& Maker : State.Vehicles.Brands())
	{
		bool bFound = false;
		for (const BuildingPlan* Dealer : City.WithRole(LotRole::Dealership))
		{
			bFound = bFound || Dealer->Param == Maker.Id;
		}
		EXPECT_MSG(bFound, Maker.Id);
	}
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
	EXPECT(State.FoundMinistry("Dust Valley Community Church").bOk);
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

DTG_TEST(AWeekInDustValley)
{
	GameState State = dtg_test::FreshGame(31);
	EXPECT(State.ActiveVehicle() != nullptr);
	EXPECT(!State.TakeNotifications().empty());
	const TickReport Report = State.Skip(8 * 24 * 60);
	EXPECT(Report.bNewWeek);
	EXPECT(State.Markets.News().size() > 0 || State.Markets.Stocks().front().History.size() > 5);
	EXPECT(State.NetWorth() > 0);
}
