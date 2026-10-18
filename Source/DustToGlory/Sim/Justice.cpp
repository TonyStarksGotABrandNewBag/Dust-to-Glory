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
		case Crime::Fraud: return Severity::Felony;
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
