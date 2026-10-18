#include "Sim/Ministry.h"

namespace dtg::sim
{
	namespace detail_ministry
	{
		constexpr OutreachInfo Outreach[] = {
			{OutreachKind::FoodDrive, "Food Drive", "Collect and hand out groceries to families in need.", Dollars(400), 4, 8, 25, 3.0},
			{OutreachKind::YouthNight, "Youth Night", "Games, music and mentoring for local teens.", Dollars(250), 3, 5, 20, 2.0},
			{OutreachKind::CommunityCleanup, "Community Cleanup", "Clean up a park and repaint a playground.", Dollars(150), 4, 4, 15, 2.5},
			{OutreachKind::HospitalVisits, "Hospital Visits", "Visit patients and support their families.", Dollars(50), 3, 2, 10, 2.0},
			{OutreachKind::BenefitConcert, "Benefit Concert", "A concert that raises money for a local shelter.", Dollars(1500), 5, 20, 70, 5.0},
			{OutreachKind::MissionTrip, "Mission Trip", "Help rebuild homes after a storm in a nearby town.", Dollars(6000), 48, 30, 90, 10.0},
		};

		// Seats per sanctuary level: storefront, chapel, church, campus.
		constexpr int SanctuarySeats[Ministry::MaxFacilityLevel + 1] = {40, 150, 600, 2500};
	}

	const char* SermonThemeName(SermonTheme Theme)
	{
		switch (Theme)
		{
		case SermonTheme::Hope: return "Hope";
		case SermonTheme::Grace: return "Grace";
		case SermonTheme::Faith: return "Faith";
		case SermonTheme::Love: return "Love";
		case SermonTheme::Forgiveness: return "Forgiveness";
		case SermonTheme::Service: return "Service";
		case SermonTheme::Stewardship: return "Stewardship";
		case SermonTheme::Gratitude: return "Gratitude";
		case SermonTheme::Family: return "Family";
		case SermonTheme::Perseverance: return "Perseverance";
		case SermonTheme::Count: break;
		}
		return "Hope";
	}

	const OutreachInfo& GetOutreachInfo(OutreachKind Kind)
	{
		for (const OutreachInfo& Entry : detail_ministry::Outreach)
		{
			if (Entry.Kind == Kind)
			{
				return Entry;
			}
		}
		return detail_ministry::Outreach[0];
	}

	const char* FacilityName(Facility Value)
	{
		switch (Value)
		{
		case Facility::Sanctuary: return "Sanctuary";
		case Facility::FoodPantry: return "Food Pantry";
		case Facility::YouthCenter: return "Youth Center";
		case Facility::MusicMinistry: return "Music Ministry";
		case Facility::MediaMinistry: return "Media Ministry";
		case Facility::Count: break;
		}
		return "Facility";
	}

	const char* OrdinationTitle(int Level)
	{
		switch (Clamp(Level, 0, 4))
		{
		case 0: return "Lay Minister";
		case 1: return "Licensed Minister";
		case 2: return "Ordained Pastor";
		case 3: return "Senior Pastor";
		default: return "Bishop";
		}
	}

	bool Ministry::Found(const std::string& ChurchTitle)
	{
		if (IsFounded() || ChurchTitle.empty())
		{
			return false;
		}
		ChurchName = ChurchTitle;
		MemberCount = 12;
		EngagementValue = 0.5;
		IntegrityValue = 1.0;
		return true;
	}

	int Ministry::Capacity() const
	{
		return detail_ministry::SanctuarySeats[Clamp(FacilityLevel(Facility::Sanctuary), 0, MaxFacilityLevel)];
	}

	Money Ministry::UpgradeCost(Facility Value) const
	{
		const int Next = FacilityLevel(Value) + 1;
		if (Next > MaxFacilityLevel)
		{
			return 0;
		}
		static const Money SanctuaryCosts[] = {0, Dollars(25000), Dollars(250000), Dollars(2500000)};
		if (Value == Facility::Sanctuary)
		{
			return SanctuaryCosts[Next];
		}
		return Dollars(4000) * Next * Next;
	}

	bool Ministry::UpgradeFacility(Facility Value, std::string* OutError)
	{
		if (!IsFounded())
		{
			if (OutError) *OutError = "Found a ministry first.";
			return false;
		}
		const Money Cost = UpgradeCost(Value);
		if (Cost == 0)
		{
			if (OutError) *OutError = "Already at the highest level.";
			return false;
		}
		if (Value == Facility::Sanctuary && FacilityLevel(Value) + 1 >= 2 && OrdinationLevel < 1)
		{
			if (OutError) *OutError = "A larger church needs a licensed minister. Study at the university.";
			return false;
		}
		if (Cost > FundBalance)
		{
			if (OutError) *OutError = "The ministry fund needs " + FormatMoney(Cost) + ".";
			return false;
		}
		FundBalance -= Cost;
		Facilities[static_cast<std::size_t>(Value)] += 1;
		return true;
	}

	SermonResult Ministry::Preach(SermonTheme Theme, double Performance, Rng& Random)
	{
		SermonResult Result;
		if (!IsFounded())
		{
			return Result;
		}
		const double Perf = Clamp(Performance, 0.0, 1.0);
		Result.bRepeatTheme = static_cast<int>(Theme) == LastTheme;
		Result.EngagementBefore = EngagementValue;
		// Fresh themes land better than repeating last week's message.
		const double Freshness = Result.bRepeatTheme ? 0.4 : 1.0;
		const double Delta = (Perf - 0.45) * 0.12 * Freshness;
		EngagementValue = Clamp(EngagementValue + Delta, 0.05, 1.0);
		Result.EngagementAfter = EngagementValue;
		LastTheme = static_cast<int>(Theme);

		const int Music = FacilityLevel(Facility::MusicMinistry);
		const int Media = FacilityLevel(Facility::MediaMinistry);
		const double Draw = (1.0 + 0.25 * Music + 0.5 * Media) * (0.5 + Perf) * (1.0 + ReputationValue / 100.0);
		Result.NewVisitors = static_cast<int>(std::lround(Random.Range(1.0, 4.0) * Draw));
		PendingVisitors += Result.NewVisitors;
		Result.NewMembers = 0;
		Result.bValid = true;
		return Result;
	}

	bool Ministry::RunOutreach(OutreachKind Kind, Rng& Random, OutreachResult& Out, std::string* OutError)
	{
		if (!IsFounded())
		{
			if (OutError) *OutError = "Found a ministry first.";
			return false;
		}
		const OutreachInfo& Info = GetOutreachInfo(Kind);
		if (Info.Cost > FundBalance)
		{
			if (OutError) *OutError = "The ministry fund needs " + FormatMoney(Info.Cost) + ".";
			return false;
		}
		FundBalance -= Info.Cost;
		const int Pantry = FacilityLevel(Facility::FoodPantry);
		const int Youth = FacilityLevel(Facility::YouthCenter);
		double Boost = 1.0;
		if (Kind == OutreachKind::FoodDrive) Boost += 0.3 * Pantry;
		if (Kind == OutreachKind::YouthNight) Boost += 0.3 * Youth;
		Out.Visitors = static_cast<int>(std::lround(Random.RangeInt(Info.MinVisitors, Info.MaxVisitors) * Boost * IntegrityValue));
		Out.NewMembers = Out.Visitors / 4;
		Out.ReputationGained = Info.Reputation * IntegrityValue;
		ReputationValue = Clamp(ReputationValue + Out.ReputationGained, 0.0, 100.0);
		PendingVisitors += Out.Visitors - Out.NewMembers;
		MemberCount = std::min(Capacity(), MemberCount + Out.NewMembers);
		EngagementValue = Clamp(EngagementValue + 0.03, 0.05, 1.0);
		Out.bValid = true;
		return true;
	}

	OfferingResult Ministry::CollectWeeklyOffering(Rng& Random)
	{
		OfferingResult Result;
		if (!IsFounded())
		{
			return Result;
		}
		Result.MembersBefore = MemberCount;

		// Integrity follows how much of the giving the minister keeps.
		if (SalaryShareValue > 0.35)
		{
			IntegrityValue -= 0.04 * ((SalaryShareValue - 0.35) / 0.05 + 1.0);
		}
		else
		{
			IntegrityValue += 0.02;
		}
		IntegrityValue = Clamp(IntegrityValue, 0.05, 1.0);

		// Membership: visitors join based on engagement; people drift away when trust or engagement is low.
		const int Joined = static_cast<int>(std::lround(PendingVisitors * (0.25 + 0.4 * EngagementValue) * IntegrityValue));
		PendingVisitors = 0;
		const double ChurnRate = 0.02 + 0.10 * (1.0 - IntegrityValue) + 0.04 * (1.0 - EngagementValue);
		const int Left = static_cast<int>(std::lround(MemberCount * ChurnRate * Random.Range(0.6, 1.4)));
		const int YouthKeep = FacilityLevel(Facility::YouthCenter);
		MemberCount = Clamp(MemberCount + Joined - std::max(0, Left - YouthKeep), 5, Capacity());

		Result.Attendance = std::min(Capacity(), static_cast<int>(std::lround(MemberCount * (0.55 + 0.4 * EngagementValue))));
		const double PerPerson = 18.0 * (0.6 + 0.8 * EngagementValue) * (0.7 + 0.6 * IntegrityValue);
		const double Total = Result.Attendance * PerPerson * Random.Range(0.9, 1.1);
		Result.Total = ToMoney(std::round(Total));
		Result.Salary = ToMoney(std::round(Total * SalaryShareValue));
		Result.ToFund = Result.Total - Result.Salary;
		FundBalance += Result.ToFund;

		// The food pantry keeps building goodwill every week.
		ReputationValue = Clamp(ReputationValue + 0.5 * FacilityLevel(Facility::FoodPantry), 0.0, 100.0);
		// Engagement drifts back toward the middle without fresh sermons.
		EngagementValue = Clamp(EngagementValue + (0.5 - EngagementValue) * 0.05, 0.05, 1.0);
		WeeksRun += 1;

		Result.MembersAfter = MemberCount;
		Result.IntegrityAfter = IntegrityValue;
		return Result;
	}

	Money Ministry::RunTelethon(Rng& Random)
	{
		if (!IsFounded())
		{
			return 0;
		}
		const Money Raised = ToMoney(std::round(MemberCount * Random.Range(40.0, 90.0) * (1.0 + ReputationValue / 50.0)));
		FundBalance += Raised;
		DoctrineValue = Clamp(DoctrineValue - 0.15, 0.0, 1.0);
		IntegrityValue = Clamp(IntegrityValue - 0.08, 0.05, 1.0);
		return Raised;
	}

	void Ministry::ApplyScandal(double TrustLoss)
	{
		if (!IsFounded())
		{
			return;
		}
		const double Loss = Clamp(TrustLoss, 0.0, 1.0);
		IntegrityValue = Clamp(IntegrityValue - 0.5 * Loss, 0.05, 1.0);
		EngagementValue = Clamp(EngagementValue - 0.3 * Loss, 0.05, 1.0);
		ReputationValue = Clamp(ReputationValue - 40.0 * Loss, 0.0, 100.0);
		MemberCount = std::max(5, static_cast<int>(std::lround(MemberCount * (1.0 - 0.6 * Loss))));
	}

	bool Ministry::CommitToAccountability(std::string* OutError)
	{
		const Money Cost = Dollars(1500);
		if (!IsFounded() || FundBalance < Cost)
		{
			if (OutError) *OutError = "The ministry fund needs " + FormatMoney(Cost) + ".";
			return false;
		}
		FundBalance -= Cost;
		DoctrineValue = Clamp(DoctrineValue + 0.2, 0.0, 1.0);
		IntegrityValue = Clamp(IntegrityValue + 0.1, 0.05, 1.0);
		return true;
	}

	void Ministry::Dissolve()
	{
		*this = Ministry();
	}

	Ministry::Snapshot Ministry::Save() const
	{
		Snapshot Data;
		Data.Name = ChurchName;
		Data.Members = MemberCount;
		Data.Engagement = EngagementValue;
		Data.Integrity = IntegrityValue;
		Data.Reputation = ReputationValue;
		Data.SalaryShare = SalaryShareValue;
		Data.Fund = FundBalance;
		Data.Ordination = OrdinationLevel;
		Data.Facilities = Facilities;
		Data.LastTheme = LastTheme;
		Data.PendingVisitors = PendingVisitors;
		Data.WeeksRun = WeeksRun;
		Data.Doctrine = DoctrineValue;
		return Data;
	}

	void Ministry::Load(const Snapshot& Data)
	{
		ChurchName = Data.Name;
		MemberCount = Data.Members;
		EngagementValue = Clamp(Data.Engagement, 0.05, 1.0);
		IntegrityValue = Clamp(Data.Integrity, 0.05, 1.0);
		ReputationValue = Clamp(Data.Reputation, 0.0, 100.0);
		SalaryShareValue = Clamp(Data.SalaryShare, 0.0, 0.5);
		FundBalance = Data.Fund;
		OrdinationLevel = Clamp(Data.Ordination, 0, 4);
		Facilities = Data.Facilities;
		for (int& Level : Facilities)
		{
			Level = Clamp(Level, 0, MaxFacilityLevel);
		}
		LastTheme = Data.LastTheme;
		PendingVisitors = std::max(0, Data.PendingVisitors);
		WeeksRun = std::max(0, Data.WeeksRun);
		DoctrineValue = Clamp(Data.Doctrine, 0.0, 1.0);
	}
}
