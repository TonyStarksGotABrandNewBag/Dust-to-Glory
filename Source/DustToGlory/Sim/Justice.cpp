#include "Sim/Justice.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	namespace detail_justice
	{
		constexpr std::int64_t MinutesPerDay = 24 * 60;
		constexpr std::int64_t MinutesPerYear = 365 * MinutesPerDay;
	}

	Severity CrimeSeverity(Crime Value)
	{
		switch (Value)
		{
		case Crime::Speeding: return Severity::Infraction;
		case Crime::RecklessDriving:
		case Crime::PropertyDamage:
		case Crime::EvadingPolice: return Severity::Misdemeanor;
		case Crime::VehicleTheft:
		case Crime::HitPoliceCar:
		case Crime::UnauthorizedHack:
		case Crime::Assault:
		case Crime::Fraud:
		case Crime::Bribery:
		case Crime::Escape:
		case Crime::Conspiracy: return Severity::Felony;
		case Crime::Count: break;
		}
		return Severity::Infraction;
	}

	int SentenceDaysFor(Crime Value)
	{
		switch (Value)
		{
		case Crime::Speeding: return 0;
		case Crime::RecklessDriving: return 5;
		case Crime::PropertyDamage: return 5;
		case Crime::EvadingPolice: return 10;
		case Crime::VehicleTheft: return 30;
		case Crime::HitPoliceCar: return 20;
		case Crime::UnauthorizedHack: return 45;
		case Crime::Assault: return 30;
		case Crime::Fraud: return 90;
		case Crime::Bribery: return 60;
		case Crime::Escape: return 120;
		case Crime::Conspiracy: return 365;
		case Crime::Count: break;
		}
		return 0;
	}

	const char* JailActivityName(JailActivity Activity)
	{
		switch (Activity)
		{
		case JailActivity::KeepHeadDown: return "Keep your head down";
		case JailActivity::Work: return "Work your jail job";
		case JailActivity::Study: return "Take classes";
		case JailActivity::Exercise: return "Work out in the yard";
		case JailActivity::Count: break;
		}
		return "Pass the time";
	}

	int JusticeSystem::Felonies() const
	{
		int Count = 0;
		for (const RecordEntry& Entry : Entries)
		{
			Count += !Entry.bExpunged && CrimeSeverity(Entry.Charge) == Severity::Felony ? 1 : 0;
		}
		return Count;
	}

	int JusticeSystem::PriorConvictions() const
	{
		int Count = 0;
		for (const RecordEntry& Entry : Entries)
		{
			Count += Entry.bExpunged ? 0 : 1;
		}
		return Count;
	}

	bool JusticeSystem::Arrest(const std::vector<Crime>& Charges, double Evidence, const std::string& CoDefendantId, const std::string& CoDefendantName,
		Money Balance)
	{
		OpenCase = CourtCase();
		Severity Worst = Severity::Infraction;
		int Days = 0;
		for (const Crime Charge : Charges)
		{
			Worst = std::max(Worst, CrimeSeverity(Charge));
			Days += SentenceDaysFor(Charge);
		}
		Days = static_cast<int>(std::lround(Days * (1.0 + 0.25 * std::min(4, PriorConvictions()))));
		if (Worst == Severity::Infraction || (Worst == Severity::Misdemeanor && PriorConvictions() == 0 && Days <= 10))
		{
			OpenCase.Fine = Dollars(150) * static_cast<Money>(std::max<std::size_t>(1, Charges.size())) + std::max<Money>(0, Balance / 20);
			return false;
		}
		OpenCase.bOpen = true;
		OpenCase.Charges = Charges;
		OpenCase.Evidence = Clamp(Evidence, 0.05, 0.98);
		OpenCase.CoDefendantId = CoDefendantId;
		OpenCase.CoDefendantName = CoDefendantName;
		OpenCase.TrialDays = std::max(3, Days);
		return true;
	}

	Money JusticeSystem::LawyerFee(int TrialDays)
	{
		return Dollars(1500) + Dollars(40) * TrialDays;
	}

	double JusticeSystem::ConvictionChance(int PeopleSkill) const
	{
		const double Defense = (OpenCase.bLawyer ? 0.25 : 0.0) + 0.015 * PeopleSkill;
		return Clamp(OpenCase.Evidence - Defense + 0.1, 0.05, 0.97);
	}

	int JusticeSystem::PleaOffer(int PeopleSkill) const
	{
		// The prosecutor's fallback is the expected sentence minus what a trial costs the county; the defendant's
		// is the expected sentence plus a premium for the risk of the maximum. The Nash bargain splits that range,
		// and a defense lawyer raises the defendant's bargaining power.
		const double Expected = ConvictionChance(PeopleSkill) * OpenCase.TrialDays;
		const double Low = std::max(0.0, Expected - 0.15 * OpenCase.TrialDays);
		const double High = Expected + 0.25 * OpenCase.TrialDays;
		const Bargain Deal = NashBargain(High - Low, 0.0, 0.0, OpenCase.bLawyer ? 0.65 : 0.45);
		return std::max(1, static_cast<int>(std::lround(High - Deal.PayoffA)));
	}

	Verdict JusticeSystem::Convict(int Days, const std::string& Summary, std::int64_t Now)
	{
		Verdict Result;
		Result.bDecided = true;
		Result.bConvicted = true;
		Result.SentenceDays = std::max(0, Days);
		Result.Summary = Summary;
		for (const Crime Charge : OpenCase.Charges)
		{
			RecordEntry Entry;
			Entry.Charge = Charge;
			Entry.Minute = Now;
			Entry.SentenceDays = Result.SentenceDays;
			Entries.push_back(Entry);
		}
		OpenCase = CourtCase();
		return Result;
	}

	Verdict JusticeSystem::TakePlea(int PeopleSkill, std::int64_t Now)
	{
		if (!OpenCase.bOpen)
		{
			return Verdict();
		}
		const int Days = PleaOffer(PeopleSkill);
		return Convict(Days, "You took the plea deal: " + std::to_string(Days) + " days.", Now);
	}

	Verdict JusticeSystem::GoToTrial(int PeopleSkill, Rng& Random, std::int64_t Now)
	{
		if (!OpenCase.bOpen)
		{
			return Verdict();
		}
		if (Random.Chance(ConvictionChance(PeopleSkill)))
		{
			return Convict(OpenCase.TrialDays, "Guilty. The judge sentenced you to " + std::to_string(OpenCase.TrialDays) + " days.", Now);
		}
		Verdict Result;
		Result.bDecided = true;
		Result.Summary = "Not guilty! You walk out of the courthouse.";
		OpenCase = CourtCase();
		return Result;
	}

	MatrixGame JusticeSystem::CoDefendantGame(int TrialDays)
	{
		const double Light = -0.2 * TrialDays;
		const double Max = -1.0 * TrialDays;
		const double Probation = -0.02 * TrialDays;
		const double Both = -0.6 * TrialDays;
		MatrixGame Game = MatrixGame::Make(2, 2, {Light, Max, Probation, Both}, {Light, Probation, Max, Both});
		Game.RowLabels = {"Stay silent", "Testify"};
		Game.ColLabels = {"Stays silent", "Testifies"};
		return Game;
	}

	Verdict JusticeSystem::CoDefendantVerdict(bool bPlayerTestifies, bool bOtherTestifies, std::int64_t Now)
	{
		if (!OpenCase.bOpen)
		{
			return Verdict();
		}
		const MatrixGame Game = CoDefendantGame(OpenCase.TrialDays);
		const int Days = static_cast<int>(std::lround(-Game.PayRow(bPlayerTestifies ? 1 : 0, bOtherTestifies ? 1 : 0)));
		const std::string Other = OpenCase.CoDefendantName.empty() ? std::string("Your co-defendant") : OpenCase.CoDefendantName;
		std::string Summary;
		if (!bPlayerTestifies && !bOtherTestifies)
		{
			Summary = "You both stayed silent. With thin evidence, you each got " + std::to_string(Days) + " days.";
		}
		else if (bPlayerTestifies && !bOtherTestifies)
		{
			Summary = "You testified. " + Other + " got the full sentence; you got " + std::to_string(Days) + " days.";
		}
		else if (!bPlayerTestifies && bOtherTestifies)
		{
			Summary = Other + " testified against you. You got the full sentence: " + std::to_string(Days) + " days.";
		}
		else
		{
			Summary = "You testified against each other. You both got " + std::to_string(Days) + " days.";
		}
		return Convict(Days, Summary, Now);
	}

	const char* EscapePlanName(EscapePlan Plan)
	{
		switch (Plan)
		{
		case EscapePlan::None: return "No plan";
		case EscapePlan::Tunnel: return "Dig a tunnel";
		case EscapePlan::BribeGuard: return "Bribe a guard";
		case EscapePlan::OutsideCrew: return "Hire a crew outside";
		case EscapePlan::MedicalRun: return "Slip the medical transfer";
		case EscapePlan::PowerBreak: return "Go through the wall";
		case EscapePlan::Count: break;
		}
		return "Plan";
	}

	const char* EscapePlanDescription(EscapePlan Plan)
	{
		switch (Plan)
		{
		case EscapePlan::None: return "Serve your time.";
		case EscapePlan::Tunnel: return "Scrape away at the wall behind your bunk, night after night, and hope the shakedown lands elsewhere. Slow, cheap, and the longest odds of being found before you go.";
		case EscapePlan::BribeGuard: return "A night-shift officer will look the other way for the right number. Fast and quiet -- if he takes the money and keeps his word.";
		case EscapePlan::OutsideCrew: return "Pay people on the outside to cut the fence and have an engine running. Expensive, quick to arrange, and loud when it happens.";
		case EscapePlan::MedicalRun: return "Fake an illness until they drive you to the hospital, then walk out a side door. Needs patience and a convincing act.";
		case EscapePlan::PowerBreak: return "Stop hiding what you are and take the wall with you. No money, no waiting -- and the whole state knows who did it.";
		case EscapePlan::Count: break;
		}
		return "";
	}

	EscapeOdds JusticeSystem::QuoteEscape(EscapePlan Plan, const Profile& Player, double PowerPotency, Money Balance) const
	{
		EscapeOdds Odds;
		if (!JailValue.bInJail)
		{
			Odds.Reason = "You're not inside.";
			return Odds;
		}
		if (Plan == EscapePlan::None || Plan == EscapePlan::Count)
		{
			Odds.Reason = "Pick a plan.";
			return Odds;
		}
		const double Fitness = Player.SkillLevel(Skill::Fitness) / 10.0;
		const double Trades = Player.SkillLevel(Skill::Trades) / 10.0;
		const double People = Player.SkillLevel(Skill::People) / 10.0;
		const double Medicine = Player.SkillLevel(Skill::Medicine) / 10.0;
		const double Power = Clamp(PowerPotency, 0.0, 3.0);
		const double Watched = Clamp(EscapeValue.Suspicion + 0.12 * EscapeValue.FailedAttempts, 0.0, 0.9);
		switch (Plan)
		{
		case EscapePlan::Tunnel:
			Odds.PrepDays = 7 - static_cast<int>(std::lround(3.0 * Trades));
			Odds.Success = 0.55 + 0.25 * Trades + 0.1 * Fitness;
			break;
		case EscapePlan::BribeGuard:
		{
			// The guard's price is a Nash bargain: the player's surplus is the time they'd otherwise serve,
			// the guard's disagreement point is what he risks -- his job, his pension and a bribery charge.
			const int Left = std::max(1, JailValue.SentenceDays - JailValue.DaysServed);
			// What a day of freedom is worth to this player (their time is worth more when they have more going on),
			// against what the officer is risking: his job, his pension and a bribery charge of his own.
			const double Freedom = (300.0 + std::min(1200.0, 0.004 * ToDollars(Balance))) * Left;
			const double Risk = 2500.0 + 6000.0 * Watched;
			const Bargain Deal = NashBargain(std::max(0.0, Freedom - Risk), 0.0, 0.0, Clamp(0.3 + 0.35 * People, 0.1, 0.8));
			Odds.GuardCut = Freedom > 0.0 ? Clamp((Risk + Deal.PayoffB) / Freedom, 0.0, 1.0) : 0.0;
			Odds.Cost = ToMoney(Risk + Deal.PayoffB);
			Odds.PrepDays = 2;
			Odds.Success = 0.55 + 0.3 * People;
			break;
		}
		case EscapePlan::OutsideCrew:
			Odds.Cost = Dollars(9000) + Dollars(120) * std::max(0, JailValue.SentenceDays - JailValue.DaysServed);
			Odds.PrepDays = 3;
			Odds.Success = 0.6 + 0.15 * People + 0.1 * Fitness;
			break;
		case EscapePlan::MedicalRun:
			Odds.PrepDays = 5 - static_cast<int>(std::lround(2.0 * Medicine));
			Odds.Success = 0.45 + 0.3 * Medicine + 0.2 * People;
			break;
		case EscapePlan::PowerBreak:
			if (Power <= 0.0)
			{
				Odds.Reason = "You'd need powers for that.";
				return Odds;
			}
			Odds.PrepDays = 1;
			Odds.Success = 0.35 + 0.35 * Power + 0.1 * Fitness;
			break;
		case EscapePlan::None:
		case EscapePlan::Count:
			break;
		}
		Odds.PrepDays = std::max(1, Odds.PrepDays);
		Odds.Success = Clamp(Odds.Success * (1.0 - 0.5 * Watched), 0.05, 0.95);
		// How often the guards toss the cells is the inspection game's equilibrium: they inspect just often
		// enough that preparing is a coin flip, and they'd rather not spend the shift on it.
		const double Gain = 1.0 + 0.1 * std::max(0, JailValue.SentenceDays - JailValue.DaysServed);
		const double Fine = 5.0 + 20.0 * (1.0 - Watched);
		const InspectionResult Inspection = InspectionGame(Gain, Fine, 1.0, 3.0 + 6.0 * Watched);
		double Exposure = 1.0;
		switch (Plan)
		{
		case EscapePlan::Tunnel: Exposure = 1.0; break;
		case EscapePlan::MedicalRun: Exposure = 0.7; break;
		case EscapePlan::BribeGuard: Exposure = 0.5; break;   // only the guard knows
		case EscapePlan::OutsideCrew: Exposure = 0.35; break; // the work happens outside
		case EscapePlan::PowerBreak: Exposure = 0.2; break;   // nothing to find in the cell
		default: break;
		}
		Odds.SearchChance = Clamp(Inspection.InspectProbability * Exposure * (0.4 + 0.6 * Watched), 0.02, 0.5);
		if (Odds.Cost > Balance)
		{
			Odds.Reason = "You can't cover " + FormatMoney(Odds.Cost) + " from the inside.";
			return Odds;
		}
		Odds.bAvailable = true;
		return Odds;
	}

	bool JusticeSystem::StartEscape(EscapePlan Plan, bool bPaid)
	{
		if (!JailValue.bInJail || Plan == EscapePlan::None || Plan == EscapePlan::Count)
		{
			return false;
		}
		EscapeValue.Plan = Plan;
		EscapeValue.PrepDone = 0;
		EscapeValue.bPaid = bPaid;
		return true;
	}

	void JusticeSystem::AbandonEscape()
	{
		EscapeValue.Plan = EscapePlan::None;
		EscapeValue.PrepDone = 0;
		EscapeValue.bPaid = false;
	}

	void JusticeSystem::Solitary(int Days)
	{
		EscapeValue.Plan = EscapePlan::None;
		EscapeValue.PrepDone = 0;
		EscapeValue.bPaid = false;
		EscapeValue.FailedAttempts += 1;
		EscapeValue.Suspicion = Clamp(EscapeValue.Suspicion + 0.3, 0.0, 1.0);
		JailValue.GoodBehavior = 0.0;
		JailValue.SentenceDays += Days;
		JailValue.ReleaseMinute += static_cast<std::int64_t>(Days) * detail_justice::MinutesPerDay;
		// No parole hearing for a while after an attempt.
		JailValue.NextParoleMinute += static_cast<std::int64_t>(Days + 30) * detail_justice::MinutesPerDay;
	}

	EscapeResult JusticeSystem::PrepareEscape(const Profile& Player, double PowerPotency, Money Balance, Rng& Random)
	{
		EscapeResult Result;
		const EscapeOdds Odds = QuoteEscape(EscapeValue.Plan, Player, PowerPotency, Balance);
		if (EscapeValue.Plan == EscapePlan::None || !JailValue.bInJail)
		{
			Result.Summary = "There's no plan to work on.";
			return Result;
		}
		EscapeValue.PrepDone += 1;
		EscapeValue.Suspicion = Clamp(EscapeValue.Suspicion + 0.02, 0.0, 1.0);
		if (Random.Chance(Odds.SearchChance))
		{
			Result.bCaught = true;
			Result.SolitaryDays = 10;
			Solitary(Result.SolitaryDays);
			Result.Summary = "Shakedown. They found it. Ten days in segregation and your plan is gone.";
			return Result;
		}
		const int Need = std::max(1, Odds.PrepDays);
		Result.Summary = EscapeValue.PrepDone >= Need
			? "Everything's in place. You could go tonight."
			: "Another night of quiet work (" + std::to_string(EscapeValue.PrepDone) + "/" + std::to_string(Need) + ").";
		return Result;
	}

	void JusticeSystem::GoOnTheRun()
	{
		EscapeValue.OwedDays = std::max(1, JailValue.SentenceDays - JailValue.DaysServed);
		EscapeValue.bFugitive = true;
		EscapeValue.DaysAtLarge = 0;
		EscapeValue.Manhunt = 1.0;
		EscapeValue.Plan = EscapePlan::None;
		EscapeValue.PrepDone = 0;
		EscapeValue.bPaid = false;
		JailValue = JailState();
	}

	EscapeResult JusticeSystem::AttemptEscape(const Profile& Player, double PowerPotency, Money Balance, Rng& Random)
	{
		EscapeResult Result;
		if (!JailValue.bInJail || EscapeValue.Plan == EscapePlan::None)
		{
			Result.Summary = "You need a plan first.";
			return Result;
		}
		const EscapePlan Plan = EscapeValue.Plan;
		const EscapeOdds Odds = QuoteEscape(Plan, Player, PowerPotency, Balance);
		Result.bAttempted = true;
		// Going before the plan is ready is its own gamble.
		const double Readiness = Clamp(static_cast<double>(EscapeValue.PrepDone) / std::max(1, Odds.PrepDays), 0.0, 1.0);
		double Chance = Odds.Success * (0.25 + 0.75 * Readiness);
		if (Plan == EscapePlan::BribeGuard && !EscapeValue.bPaid)
		{
			Chance *= 0.2; // he never got his money
		}
		if (Random.Chance(Chance))
		{
			Result.bEscaped = true;
			GoOnTheRun();
			switch (Plan)
			{
			case EscapePlan::Tunnel: Result.Summary = "You come up in the scrub past the fence line and run. Nobody shouts."; break;
			case EscapePlan::BribeGuard: Result.Summary = "The side door is unlocked at 2am, exactly as paid for. You walk out."; break;
			case EscapePlan::OutsideCrew: Result.Summary = "Headlights, a cut fence, an open door. You're three counties away by sunrise."; break;
			case EscapePlan::MedicalRun: Result.Summary = "A gown, a corridor, a fire exit. The deputy is still outside the exam room."; break;
			case EscapePlan::PowerBreak: Result.Summary = "The block wall comes apart and you're gone before the siren winds up. Everyone saw it."; break;
			default: Result.Summary = "You're out."; break;
			}
			return Result;
		}
		Result.bCaught = true;
		Result.SolitaryDays = 20 + 10 * EscapeValue.FailedAttempts;
		Solitary(Result.SolitaryDays);
		Result.Summary = "They were waiting. " + std::to_string(Result.SolitaryDays) + " days added and you're on the watch list now.";
		return Result;
	}

	bool JusticeSystem::AdvanceManhunt(bool bLayingLow, Rng& Random)
	{
		if (!EscapeValue.bFugitive)
		{
			return false;
		}
		EscapeValue.DaysAtLarge += 1;
		// Interest fades while you keep your head down, and comes back the moment you don't.
		EscapeValue.Manhunt = Clamp(EscapeValue.Manhunt * (bLayingLow ? 0.93 : 1.02) - (bLayingLow ? 0.01 : 0.0), 0.05, 1.0);
		return Random.Chance(0.05 * EscapeValue.Manhunt);
	}

	void JusticeSystem::Recapture(std::int64_t Now)
	{
		if (!EscapeValue.bFugitive)
		{
			return;
		}
		RecordEntry Entry;
		Entry.Charge = Crime::Escape;
		Entry.Minute = Now;
		Entry.SentenceDays = SentenceDaysFor(Crime::Escape);
		Entries.push_back(Entry);
		const int Days = EscapeValue.OwedDays + Entry.SentenceDays;
		EscapeValue.bFugitive = false;
		EscapeValue.OwedDays = 0;
		EscapeValue.Manhunt = 0.0;
		EscapeValue.Suspicion = 1.0;
		BeginSentence(Days, Now);
	}

	void JusticeSystem::Surrender(std::int64_t Now)
	{
		if (!EscapeValue.bFugitive)
		{
			return;
		}
		RecordEntry Entry;
		Entry.Charge = Crime::Escape;
		Entry.Minute = Now;
		Entry.SentenceDays = SentenceDaysFor(Crime::Escape) / 3;
		Entries.push_back(Entry);
		const int Days = EscapeValue.OwedDays + Entry.SentenceDays;
		EscapeValue.bFugitive = false;
		EscapeValue.OwedDays = 0;
		EscapeValue.Manhunt = 0.0;
		EscapeValue.Suspicion = 0.6;
		BeginSentence(Days, Now);
	}

	void JusticeSystem::BeginSentence(int Days, std::int64_t Now)
	{
		JailValue = JailState();
		if (Days <= 0)
		{
			return;
		}
		JailValue.bInJail = true;
		JailValue.SentenceDays = Days;
		JailValue.ReleaseMinute = Now + static_cast<std::int64_t>(Days) * detail_justice::MinutesPerDay;
		JailValue.NextParoleMinute = Now + static_cast<std::int64_t>(Days / 2) * detail_justice::MinutesPerDay;
	}

	JailDayResult JusticeSystem::ServeDay(JailActivity Activity, Profile& Player, std::int64_t Now)
	{
		using detail_justice::MinutesPerDay;
		JailDayResult Result;
		JailState& Jail = JailValue;
		if (!Jail.bInJail)
		{
			Result.Summary = "You're not in jail.";
			return Result;
		}
		Jail.DaysServed += 1;
		switch (Activity)
		{
		case JailActivity::KeepHeadDown:
			Jail.GoodBehavior = Clamp(Jail.GoodBehavior + 0.05, 0.0, 1.0);
			Result.Summary = "A quiet day. The guards notice.";
			break;
		case JailActivity::Work:
			Jail.GoodBehavior = Clamp(Jail.GoodBehavior + 0.04, 0.0, 1.0);
			Result.Earned = Dollars(4);
			Player.AddSkillXp(Skill::Trades, 10);
			Result.Summary = "You put in a shift in the kitchen.";
			break;
		case JailActivity::Study:
			Jail.GoodBehavior = Clamp(Jail.GoodBehavior + 0.03, 0.0, 1.0);
			Jail.ProgramDays += 1;
			Player.AddSkillXp(Skill::Science, 15);
			Result.Summary = "Another day of classes.";
			if (Jail.ProgramDays >= 10)
			{
				// Finishing a program takes a tenth off the original sentence.
				Jail.ProgramDays = 0;
				Jail.ProgramsCompleted += 1;
				Result.DaysCut = std::max(1, Jail.SentenceDays / 10);
				Result.Summary = "You finished a program! " + std::to_string(Result.DaysCut) + " days off your sentence.";
			}
			break;
		case JailActivity::Exercise:
			Player.AddSkillXp(Skill::Fitness, 20);
			Result.Summary = "You worked out in the yard.";
			break;
		case JailActivity::Count:
			break;
		}
		// Good-time credit: steady good behavior earns a day off for every few served.
		if (Jail.GoodBehavior > 0.7 && Jail.DaysServed % 3 == 0)
		{
			Result.DaysCut += 1;
		}
		Jail.ReleaseMinute -= static_cast<std::int64_t>(Result.DaysCut) * MinutesPerDay;
		if (Now + MinutesPerDay >= Jail.ReleaseMinute)
		{
			Release();
			Result.bReleased = true;
			Result.Summary += " You've served your time. You're free to go.";
		}
		return Result;
	}

	double JusticeSystem::ParoleChance() const
	{
		return Clamp(0.25 + 0.45 * JailValue.GoodBehavior + 0.1 * JailValue.ProgramsCompleted - 0.08 * Felonies(), 0.05, 0.95);
	}

	bool JusticeSystem::CanRequestParole(std::int64_t Now) const
	{
		return JailValue.bInJail && Now >= JailValue.NextParoleMinute;
	}

	bool JusticeSystem::ParoleHearing(std::int64_t Now, Rng& Random)
	{
		if (!CanRequestParole(Now))
		{
			return false;
		}
		if (Random.Chance(ParoleChance()))
		{
			Release();
			return true;
		}
		JailValue.NextParoleMinute = Now + 30 * detail_justice::MinutesPerDay;
		return false;
	}

	Money JusticeSystem::AppealCost(int SentenceDays)
	{
		return Dollars(3000) + Dollars(60) * SentenceDays;
	}

	bool JusticeSystem::Appeal(Rng& Random)
	{
		if (!JailValue.bInJail || Entries.empty())
		{
			return false;
		}
		if (!Random.Chance(0.25))
		{
			return false;
		}
		// Overturned: the convictions from this case come off the record.
		const std::int64_t CaseMinute = Entries.back().Minute;
		Entries.erase(std::remove_if(Entries.begin(), Entries.end(), [CaseMinute](const RecordEntry& Entry) { return Entry.Minute == CaseMinute; }),
			Entries.end());
		Release();
		return true;
	}

	bool JusticeSystem::Expunge(std::int64_t Now)
	{
		bool bChanged = false;
		for (RecordEntry& Entry : Entries)
		{
			if (!Entry.bExpunged && Now - Entry.Minute >= 3 * detail_justice::MinutesPerYear)
			{
				Entry.bExpunged = true;
				bChanged = true;
			}
		}
		return bChanged;
	}

	void JusticeSystem::Pardon()
	{
		for (RecordEntry& Entry : Entries)
		{
			Entry.bExpunged = true;
		}
		Release();
	}

	void JusticeSystem::Release()
	{
		JailValue = JailState();
	}
}
