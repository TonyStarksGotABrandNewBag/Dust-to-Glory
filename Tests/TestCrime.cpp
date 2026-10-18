// The villain routes: breaking out of county jail (and living as a fugitive), and arranging an outbreak so
// your own company can sell the cure -- both of which are supposed to be tempting, expensive and risky.
#include "TestFramework.h"
#include "TestSupport.h"

#include <cmath>

using namespace dtg::sim;

namespace
{
	/** Puts the player inside with a long sentence to tempt them. */
	GameState Jailed(std::uint64_t Seed, Money Cash = Dollars(200000))
	{
		GameState State = dtg_test::FreshGame(Seed);
		State.Funds.Deposit(Cash, MoneyCategory::Other, "test", 0);
		State.ReportCrime(Crime::VehicleTheft, true, false);
		State.ReportCrime(Crime::Assault, true, false);
		State.ReportCrime(Crime::Fraud, true, false);
		State.Busted();
		State.HireLawyer();
		State.TakePlea();
		return State;
	}
}

DTG_TEST(EveryWayOutOfJailHasAPrice)
{
	GameState State = Jailed(301);
	EXPECT(State.InJail());
	const int Sentence = State.Justice.Jail().SentenceDays;
	EXPECT(Sentence > 20);

	// Out on the street, there's nothing to plan.
	GameState Free = dtg_test::FreshGame(302);
	EXPECT(!Free.QuoteEscape(EscapePlan::Tunnel).bAvailable);

	// Inside, each plan has its own cost, prep time and odds; powers open one more.
	const EscapeOdds Tunnel = State.QuoteEscape(EscapePlan::Tunnel);
	const EscapeOdds Bribe = State.QuoteEscape(EscapePlan::BribeGuard);
	const EscapeOdds Crew = State.QuoteEscape(EscapePlan::OutsideCrew);
	const EscapeOdds Medical = State.QuoteEscape(EscapePlan::MedicalRun);
	EXPECT(Tunnel.bAvailable && Bribe.bAvailable && Crew.bAvailable && Medical.bAvailable);
	EXPECT(Tunnel.Cost == 0 && Bribe.Cost > 0 && Crew.Cost > 0);
	EXPECT(Tunnel.PrepDays > Bribe.PrepDays);        // digging takes nights; money takes a shift
	EXPECT(Tunnel.SearchChance > Crew.SearchChance); // there's something in your cell to find
	EXPECT(Bribe.GuardCut > 0.0 && Bribe.GuardCut < 1.0);
	for (const EscapeOdds& Odds : {Tunnel, Bribe, Crew, Medical})
	{
		EXPECT(Odds.Success > 0.0 && Odds.Success < 1.0);
	}
	// A guard's price is a Nash bargain: a smooth talker pays less than someone who can't negotiate.
	GameState Talker = Jailed(301);
	Talker.Player.AddSkillXp(Skill::People, 6000);
	EXPECT(Talker.QuoteEscape(EscapePlan::BribeGuard).Cost < Bribe.Cost);
	// And a longer stretch is worth more to get out of, so the guard asks for more.
	EXPECT(State.QuoteEscape(EscapePlan::BribeGuard).Cost > Dollars(4000));

	// Powers are their own way out, and only if you have them.
	EXPECT(!State.QuoteEscape(EscapePlan::PowerBreak).bAvailable);
	std::vector<std::string> Picks;
	for (const PowerDef* Def : State.Powers.StarterPowers())
	{
		if (Picks.size() < 3)
		{
			Picks.push_back(Def->Id);
		}
	}
	GameState Super = Jailed(303);
	EXPECT(Super.ChoosePowers(Picks).bOk);
	EXPECT(Super.QuoteEscape(EscapePlan::PowerBreak).bAvailable);

	// Committing to a bribe costs the money up front, and the bribe itself is a charge waiting to be found.
	const Money Before = State.Funds.Balance();
	EXPECT(State.PlanEscape(EscapePlan::BribeGuard).bOk);
	EXPECT(State.Funds.Balance() < Before);
	EXPECT(State.Justice.Escape().Plan == EscapePlan::BribeGuard);
	EXPECT(State.AbandonEscape().bOk);
	EXPECT(State.Justice.Escape().Plan == EscapePlan::None);
	// The money doesn't come back.
	EXPECT(State.Funds.Balance() < Before);
}

DTG_TEST(EscapingWorksAndSoDoesGettingCaught)
{
	int Escapes = 0;
	int Caught = 0;
	for (std::uint64_t Seed = 1; Seed <= 40; ++Seed)
	{
		GameState State = Jailed(400 + Seed);
		if (!State.PlanEscape(EscapePlan::Tunnel).bOk)
		{
			continue;
		}
		const int Need = State.QuoteEscape(EscapePlan::Tunnel).PrepDays;
		bool bFound = false;
		for (int Night = 0; Night < Need && !bFound; ++Night)
		{
			bFound = State.PrepareEscape().bCaught;
		}
		if (bFound)
		{
			++Caught;
			// Found during prep: solitary, a longer sentence, a lost plan and a reputation inside.
			EXPECT(State.Justice.Escape().Plan == EscapePlan::None);
			EXPECT(State.Justice.Escape().FailedAttempts == 1);
			EXPECT(State.Justice.Jail().GoodBehavior < 0.1);
			EXPECT(State.InJail());
			continue;
		}
		const EscapeResult Result = State.AttemptEscape();
		EXPECT(Result.bAttempted);
		if (Result.bEscaped)
		{
			++Escapes;
			EXPECT(!State.InJail() && State.IsFugitive());
			EXPECT(State.Wanted.Stars() >= 4);
			EXPECT(State.Justice.Escape().OwedDays > 0);
		}
		else
		{
			++Caught;
			EXPECT(State.InJail() && !State.IsFugitive());
			EXPECT(Result.SolitaryDays > 0);
			EXPECT(State.Justice.Jail().SentenceDays > State.Justice.Jail().DaysServed);
		}
	}
	// Both outcomes are real: it's a gamble, not a cutscene.
	EXPECT(Escapes > 3);
	EXPECT(Caught > 3);
}

DTG_TEST(LifeOnTheRun)
{
	GameState State = Jailed(501);
	// A bribed guard and a prepared plan are the surest way out; try until one lands.
	bool bOut = false;
	for (int Attempt = 0; Attempt < 25 && !bOut; ++Attempt)
	{
		if (State.Justice.Escape().Plan == EscapePlan::None)
		{
			State.Funds.Deposit(Dollars(100000), MoneyCategory::Other, "test", 0);
			if (!State.PlanEscape(EscapePlan::OutsideCrew).bOk)
			{
				break;
			}
			for (int Night = 0; Night < State.QuoteEscape(EscapePlan::OutsideCrew).PrepDays; ++Night)
			{
				State.PrepareEscape();
			}
		}
		if (State.Justice.Escape().Plan != EscapePlan::None)
		{
			bOut = State.AttemptEscape().bEscaped;
		}
	}
	EXPECT(bOut);
	if (!bOut)
	{
		return;
	}
	// A fugitive can't hold a job, can't take one, and can't run for office.
	EXPECT(State.Jobs.empty());
	EXPECT(!State.ApplyForCareer("landscaper").bOk);
	EXPECT(!State.RunForOffice(Office::CityCouncil).bOk);
	// Police are out in force while the manhunt is hot.
	const double Hunting = State.PolicePatrolChance(5000.0);
	// Keeping your head down cools it off.
	State.Wanted.Clear();
	for (int Day = 0; Day < 40; ++Day)
	{
		State.Skip(24 * 60);
	}
	EXPECT(State.Justice.Escape().Manhunt < 1.0);
	EXPECT(State.PolicePatrolChance(5000.0) <= Hunting);

	// It all survives a save.
	GameState Restored = dtg_test::LoadedState();
	EXPECT(Restored.LoadFromJson(State.SaveToJson(), nullptr));
	EXPECT(Restored.IsFugitive());
	EXPECT(Restored.Justice.Escape().OwedDays == State.Justice.Escape().OwedDays);

	// Getting picked up puts the owed time back plus an escape charge; turning yourself in costs less.
	GameState Recaptured = Restored;
	const int Owed = Recaptured.Justice.Escape().OwedDays;
	Recaptured.ReportCrime(Crime::Speeding, true, false);
	Recaptured.Busted();
	EXPECT(Recaptured.InJail() && !Recaptured.IsFugitive());
	EXPECT(Recaptured.Justice.Jail().SentenceDays > Owed);
	const int Hard = Recaptured.Justice.Jail().SentenceDays;

	GameState Surrendered = Restored;
	EXPECT(Surrendered.Surrender().bOk);
	EXPECT(Surrendered.InJail() && !Surrendered.IsFugitive());
	EXPECT(Surrendered.Justice.Jail().SentenceDays < Hard);
	// Either way, an escape conviction is on the record for good.
	EXPECT(Surrendered.Justice.HasFelony());
}

namespace
{
	/** A player with a scientist's skills, money, an approved treatment and friendly contacts. */
	GameState Schemer(std::uint64_t Seed)
	{
		GameState State = dtg_test::FreshGame(Seed);
		State.Funds.Deposit(Dollars(5000000), MoneyCategory::Other, "test", 0);
		State.Player.AddSkillXp(Skill::Science, 6000);
		State.Player.AddSkillXp(Skill::Medicine, 6000);
		State.Player.AddSkillXp(Skill::Business, 6000);
		for (const char* Id : {"npc-lab-1", "npc-courier-1", "npc-official-1", "npc-fixer-1"})
		{
			NpcMind& Mind = State.Npcs.GetOrCreate(Id);
			Mind.Affinity = 0.8;
			Mind.Awareness = 0.1;
		}
		return State;
	}
}

DTG_TEST(ArrangingAnOutbreakTakesACrew)
{
	GameState State = Schemer(601);
	// You can't release anything without someone who can reach a sample.
	EXPECT(!State.ReleaseStrain().bOk);

	// Somebody who has woken up to the world, or who doesn't trust you, won't take the job at any price.
	NpcMind& Awake = State.Npcs.GetOrCreate("npc-awake");
	Awake.Affinity = 0.9;
	Awake.Awareness = 0.9;
	EXPECT(!State.QuoteInsider("npc-awake", InsiderRole::LabTech).bAvailable);
	NpcMind& Stranger = State.Npcs.GetOrCreate("npc-stranger");
	Stranger.Affinity = -0.2;
	EXPECT(!State.QuoteInsider("npc-stranger", InsiderRole::LabTech).bAvailable);

	// Each role names its own price: the official has the most to lose and asks for the biggest cut.
	const RecruitQuote Lab = State.QuoteInsider("npc-lab-1", InsiderRole::LabTech);
	const RecruitQuote Courier = State.QuoteInsider("npc-courier-1", InsiderRole::Courier);
	const RecruitQuote Official = State.QuoteInsider("npc-official-1", InsiderRole::HealthOfficial);
	EXPECT(Lab.bAvailable && Courier.bAvailable && Official.bAvailable);
	EXPECT(Official.Cut > Courier.Cut);
	EXPECT(Lab.Retainer > 0 && Lab.FlipRisk > 0.0);

	const Money Before = State.Funds.Balance();
	EXPECT(State.RecruitInsider("npc-lab-1", InsiderRole::LabTech).bOk);
	EXPECT(State.Funds.Balance() < Before); // the retainer is paid now
	EXPECT(State.Scheme.HasRole(InsiderRole::LabTech));
	EXPECT(State.Scheme.Stage() == SchemeStage::Planning);
	EXPECT(!State.RecruitInsider("npc-lab-1", InsiderRole::LabTech).bOk); // already in

	// The dilemma each insider faces is a real prisoner's dilemma: flipping first beats staying quiet
	// when the other talks, and both flipping is worse for them than both staying silent.
	const MatrixGame Game = State.InsiderDilemma("npc-lab-1");
	EXPECT(Game.Rows == 2 && Game.Cols == 2);
	EXPECT(Game.PayCol(0, 1) > Game.PayCol(0, 0) * 0.0 - 1e9);
	EXPECT(Game.PayCol(0, 1) > Game.PayCol(1, 1)); // flipping first beats being flipped on
	EXPECT(Game.PayCol(0, 0) > Game.PayCol(1, 1)); // mutual silence beats mutual betrayal

	// Walking away before anything happens is allowed; the retainers are gone.
	GameState Reconsiders = State;
	EXPECT(Reconsiders.CallOffScheme().bOk);
	EXPECT(Reconsiders.Scheme.Stage() == SchemeStage::None);

	// With a lab tech aboard, the strain can be released and an outbreak starts.
	EXPECT(!State.Outbreak.IsActive());
	EXPECT(State.ReleaseStrain().bOk);
	EXPECT(State.Outbreak.IsActive());
	EXPECT(State.Scheme.IsLive());
	EXPECT(!State.ReleaseStrain().bOk); // one is enough

	// A health official on the payroll slows the state's response.
	GameState WithOfficial = Schemer(602);
	EXPECT(WithOfficial.RecruitInsider("npc-official-1", InsiderRole::HealthOfficial).bOk);
	EXPECT(WithOfficial.Scheme.ResponseDrag() > State.Scheme.ResponseDrag() * 0.0);
	EXPECT(WithOfficial.Scheme.ResponseDrag() > 0.0);
}

DTG_TEST(TheMorePeopleKnowTheFasterItUnravels)
{
	// A big, unpaid crew gets caught far more often than one careful partner who was paid.
	int ExposedBig = 0;
	int ExposedSmall = 0;
	for (std::uint64_t Seed = 1; Seed <= 25; ++Seed)
	{
		GameState Big = Schemer(700 + Seed);
		Big.RecruitInsider("npc-lab-1", InsiderRole::LabTech);
		Big.RecruitInsider("npc-courier-1", InsiderRole::Courier);
		Big.RecruitInsider("npc-official-1", InsiderRole::HealthOfficial);
		Big.ReleaseStrain();
		Big.PayTheCrew(false); // stiffed
		for (int Day = 0; Day < 120 && Big.Scheme.IsLive(); ++Day)
		{
			Big.Skip(24 * 60);
		}
		ExposedBig += Big.Scheme.Stage() == SchemeStage::Exposed ? 1 : 0;

		GameState Small = Schemer(800 + Seed);
		Small.RecruitInsider("npc-lab-1", InsiderRole::LabTech);
		Small.RecruitInsider("npc-fixer-1", InsiderRole::Fixer);
		Small.ReleaseStrain();
		Small.PayTheCrew(true);
		for (int Day = 0; Day < 120 && Small.Scheme.IsLive(); ++Day)
		{
			Small.Skip(24 * 60);
		}
		ExposedSmall += Small.Scheme.Stage() == SchemeStage::Exposed ? 1 : 0;
	}
	EXPECT(ExposedBig > ExposedSmall);
	EXPECT(ExposedBig > 2);         // it does come apart
	EXPECT(ExposedSmall < 25);      // but a tight crew can get away with it

	// Given long enough with nobody sick and nobody talking, the case goes cold and the player walks.
	int GotAway = 0;
	for (std::uint64_t Seed = 1; Seed <= 12; ++Seed)
	{
		GameState Careful = Schemer(900 + Seed);
		Careful.RecruitInsider("npc-lab-1", InsiderRole::LabTech);
		Careful.RecruitInsider("npc-fixer-1", InsiderRole::Fixer);
		Careful.ReleaseStrain();
		Careful.PayTheCrew(true);
		for (int Day = 0; Day < 400 && Careful.Scheme.IsLive(); ++Day)
		{
			Careful.Skip(24 * 60);
		}
		GotAway += Careful.Scheme.Stage() == SchemeStage::GotAway ? 1 : 0;
	}
	EXPECT(GotAway > 0);
}

DTG_TEST(GettingCaughtCostsEverything)
{
	GameState State = Schemer(901);
	EXPECT(State.RecruitInsider("npc-lab-1", InsiderRole::LabTech).bOk);
	EXPECT(State.RecruitInsider("npc-courier-1", InsiderRole::Courier).bOk);
	EXPECT(State.ReleaseStrain().bOk);
	State.Markets.PlayerReputation().Followers = 50000;
	const double TrustBefore = State.Markets.PlayerReputation().Trust;

	// Run it until investigators close the case, however long that takes.
	for (int Day = 0; Day < 2000 && State.Scheme.IsLive(); ++Day)
	{
		State.Markets.PlayerReputation().Trust = 0.1; // an angry public speeds it along
		State.Skip(24 * 60);
	}
	EXPECT(State.Scheme.Stage() == SchemeStage::Exposed || State.Scheme.Stage() == SchemeStage::GotAway);
	if (State.Scheme.Stage() != SchemeStage::Exposed)
	{
		return;
	}
	EXPECT(State.Markets.PlayerReputation().Trust < TrustBefore);
	EXPECT(State.Markets.PlayerReputation().Followers < 50000);
	EXPECT(State.Wanted.Stars() >= 4);
	// The charges are waiting: conspiracy is the heaviest sentence in the book.
	EXPECT(State.Busted() == 0);
	EXPECT(State.Justice.Case().bOpen);
	EXPECT(State.Justice.Case().TrialDays > SentenceDaysFor(Crime::Fraud));
	EXPECT(SentenceDaysFor(Crime::Conspiracy) > SentenceDaysFor(Crime::VehicleTheft));
	EXPECT(CrimeSeverity(Crime::Conspiracy) == Severity::Felony);

	// And the whole thing survives a save.
	GameState Restored = dtg_test::LoadedState();
	EXPECT(Restored.LoadFromJson(State.SaveToJson(), nullptr));
	EXPECT(Restored.Scheme.Stage() == SchemeStage::Exposed);
	EXPECT(Restored.Scheme.Insiders().size() == State.Scheme.Insiders().size());
}
