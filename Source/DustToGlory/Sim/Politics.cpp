#include "Sim/Politics.h"

#include "Sim/Market.h"

namespace dtg::sim
{
	namespace detail_politics
	{
		// Mid-1990s figures: filing fees, campaign budgets and salaries in era dollars; days are game days.
		const OfficeInfo Offices[] = {
			{Office::None, "Private Citizen", 0, Office::None, 0, 0, 0, 0, 0, 0.0},
			{Office::CityCouncil, "City Councilmember", 500, Office::None, Dollars(500), Dollars(5000), 3, 14, Dollars(300), 0.45},
			{Office::Mayor, "Mayor", 5000, Office::None, Dollars(2000), Dollars(40000), 4, 14, Dollars(1500), 0.50},
			{Office::StateRepresentative, "State Representative", 15000, Office::None, Dollars(5000), Dollars(120000), 4, 14, Dollars(800), 0.50},
			{Office::Governor, "Governor", 50000, Office::Mayor, Dollars(25000), Dollars(2000000), 5, 21, Dollars(2000), 0.52},
			{Office::Senator, "U.S. Senator", 150000, Office::StateRepresentative, Dollars(100000), Dollars(6000000), 6, 28, Dollars(2600), 0.53},
			{Office::President, "President of the United States", 500000, Office::Governor, Dollars(1000000), Dollars(60000000), 7, 28, Dollars(3800), 0.55},
		};

		const std::vector<std::string>& Opponents()
		{
			static const std::vector<std::string> Names = {
				"Councilwoman Hargrove", "Dale Whitcomb", "Evelyn Strand", "Rev. Marcus Bell", "Judge Patricia Ames",
				"Senator Lowell Pryce", "Governor Anita Vale", "Businessman Clark Rourke", "Dr. Helen Okoye", "Sheriff Tom Brandt"};
			return Names;
		}

		// How much legislative clout each office has.
		double Clout(Office Value)
		{
			switch (Value)
			{
			case Office::CityCouncil: return 0.05;
			case Office::Mayor: return 0.15;
			case Office::StateRepresentative: return 0.10;
			case Office::Governor: return 0.25;
			case Office::Senator: return 0.20;
			case Office::President: return 0.35;
			default: return 0.0;
			}
		}

		bool Reached(Office Highest, Office Required)
		{
			if (Required == Office::None)
			{
				return true;
			}
			// Governor accepts a sitting or former mayor or state rep; senator and president accept higher offices too.
			if (Required == Office::Mayor)
			{
				return static_cast<int>(Highest) >= static_cast<int>(Office::Mayor);
			}
			if (Required == Office::StateRepresentative)
			{
				return static_cast<int>(Highest) >= static_cast<int>(Office::StateRepresentative);
			}
			return static_cast<int>(Highest) >= static_cast<int>(Required);
		}
	}

	const OfficeInfo& GetOfficeInfo(Office Value)
	{
		for (const OfficeInfo& Entry : detail_politics::Offices)
		{
			if (Entry.Id == Value)
			{
				return Entry;
			}
		}
		return detail_politics::Offices[0];
	}

	const char* OfficeTitle(Office Value)
	{
		return GetOfficeInfo(Value).Title;
	}

	bool PoliticalCareer::CanRun(Office Target, int Followers, double Trust, std::string* OutReason) const
	{
		if (Target == Office::None || Target == Office::Count)
		{
			if (OutReason) *OutReason = "Pick an office.";
			return false;
		}
		if (Run.bActive)
		{
			if (OutReason) *OutReason = "You're already running a campaign.";
			return false;
		}
		if (Target == CurrentOffice)
		{
			if (OutReason) *OutReason = "You already hold that office.";
			return false;
		}
		const OfficeInfo& Info = GetOfficeInfo(Target);
		if (!detail_politics::Reached(HighestOffice, Info.Requires))
		{
			if (OutReason) *OutReason = std::string("Voters want experience: serve as ") + OfficeTitle(Info.Requires) + " first.";
			return false;
		}
		if (Followers < Info.FollowersNeeded)
		{
			if (OutReason) *OutReason = "You need a following of " + std::to_string(Info.FollowersNeeded) + " people to be taken seriously.";
			return false;
		}
		if (Trust < 0.35)
		{
			if (OutReason) *OutReason = "Your public trust is too low. Repair your reputation first.";
			return false;
		}
		return true;
	}

	bool PoliticalCareer::StartCampaign(Office Target, int Followers, double Trust, double ReputationScore, Wallet& Funds, std::int64_t Now, Rng& Random, std::string* OutError)
	{
		if (!CanRun(Target, Followers, Trust, OutError))
		{
			return false;
		}
		const OfficeInfo& Info = GetOfficeInfo(Target);
		if (!Funds.TrySpend(Info.FilingFee, MoneyCategory::Other, std::string("Filing fee: ") + Info.Title, Now))
		{
			if (OutError) *OutError = "Filing costs " + FormatMoney(Info.FilingFee) + ".";
			return false;
		}
		Run = Campaign();
		Run.bActive = true;
		Run.Target = Target;
		Run.Support = Clamp(0.25 + 0.3 * ReputationScore / 100.0 + 0.02 * std::log10(1.0 + std::max(0, Followers)), 0.05, 0.7);
		Run.OpponentSupport = Info.OpponentStrength;
		Run.Opponent = Random.Pick(detail_politics::Opponents());
		Run.ElectionMinute = Now + static_cast<std::int64_t>(Info.CampaignDays) * 24 * 60;
		return true;
	}

	double PoliticalCareer::Rally(double Performance)
	{
		if (!Run.bActive) return 0.0;
		const double Gain = (0.01 + 0.03 * Clamp(Performance, 0.0, 1.0)) / (1.0 + 0.3 * Run.Rallies);
		Run.Rallies += 1;
		Run.Support = Clamp(Run.Support + Gain, 0.0, 1.0);
		return Gain;
	}

	double PoliticalCareer::RunAds(Money Amount, Wallet& Funds, std::int64_t Now)
	{
		if (!Run.bActive || Amount <= 0 || !Funds.TrySpend(Amount, MoneyCategory::Other, "Campaign ads", Now))
		{
			return 0.0;
		}
		const OfficeInfo& Info = GetOfficeInfo(Run.Target);
		Run.Spent += Amount;
		// Diminishing returns: the first budget's worth of ads moves the needle most.
		const double Share = static_cast<double>(Amount) / static_cast<double>(std::max<Money>(1, Info.TypicalBudget));
		const double Gain = 0.08 * Share / (1.0 + static_cast<double>(Run.Spent) / static_cast<double>(std::max<Money>(1, Info.TypicalBudget)));
		Run.Support = Clamp(Run.Support + Gain, 0.0, 1.0);
		return Gain;
	}

	double PoliticalCareer::Debate(double Performance)
	{
		if (!Run.bActive) return 0.0;
		Run.Debates += 1;
		const double Gain = Clamp(Performance, 0.0, 1.0) >= 0.6 ? 0.05 * Performance : -0.04 * (1.0 - Performance);
		Run.Support = Clamp(Run.Support + Gain, 0.0, 1.0);
		return Gain;
	}

	double PoliticalCareer::Endorsement(double EndorserReputation)
	{
		if (!Run.bActive) return 0.0;
		Run.Endorsements += 1;
		const double Gain = 0.025 * Clamp(EndorserReputation / 100.0, 0.0, 1.0) / (1.0 + 0.2 * Run.Endorsements);
		Run.Support = Clamp(Run.Support + Gain, 0.0, 1.0);
		return Gain;
	}

	CampaignRound PoliticalCareer::PlayStrategy(const MatrixGame& Game, int PlayerChoice, double Performance, Rng& Random)
	{
		CampaignRound Round;
		if (!Run.bActive || PlayerChoice < 0 || PlayerChoice >= Game.Rows)
		{
			return Round;
		}
		// The opponent's campaign manager plays the minimax (Nash) mix, so it can't be exploited.
		Equilibrium Solution;
		if (!SolveEquilibrium(Game, Solution))
		{
			Solution.Col.assign(static_cast<std::size_t>(Game.Cols), 1.0 / Game.Cols);
		}
		Round.bPlayed = true;
		Round.PlayerChoice = PlayerChoice;
		Round.OpponentChoice = SampleStrategy(Solution.Col, Random);
		Round.Solution = Solution;
		const double Raw = Game.PayRow(PlayerChoice, Round.OpponentChoice);
		// Execution matters: a well-run plan amplifies gains and softens losses.
		const double Execution = 0.5 + Clamp(Performance, 0.0, 1.0);
		Round.SupportChange = Raw >= 0.0 ? Raw * Execution : Raw / Execution;
		Run.Support = Clamp(Run.Support + Round.SupportChange, 0.0, 1.0);
		return Round;
	}

	void PoliticalCareer::Scandal(double Severity)
	{
		const double Hit = 0.15 * Clamp(Severity, 0.0, 1.0);
		if (Run.bActive)
		{
			Run.Support = Clamp(Run.Support - Hit, 0.0, 1.0);
		}
		ApprovalValue = Clamp(ApprovalValue - Hit, 0.0, 1.0);
	}

	ElectionResult PoliticalCareer::Update(std::int64_t Now, Rng& Random)
	{
		ElectionResult Result;
		// Terms end; the officeholder steps down unless re-elected (re-running is a fresh campaign).
		if (CurrentOffice != Office::None && TermEndMinute > 0 && Now >= TermEndMinute && !Run.bActive)
		{
			Leave();
		}
		if (!Run.bActive || Now < Run.ElectionMinute)
		{
			return Result;
		}
		Result.bHeld = true;
		Result.Target = Run.Target;
		Result.Opponent = Run.Opponent;
		const double Player = Clamp(Run.Support + 0.03 * Gaussian(Random), 0.0, 1.0);
		const double Opponent = Clamp(Run.OpponentSupport + 0.03 * Gaussian(Random), 0.0, 1.0);
		const double Total = std::max(1e-6, Player + Opponent);
		Result.PlayerShare = Player / Total;
		Result.OpponentShare = Opponent / Total;
		Result.bWon = Result.PlayerShare > Result.OpponentShare;
		if (Result.bWon)
		{
			CurrentOffice = Run.Target;
			if (static_cast<int>(Run.Target) > static_cast<int>(HighestOffice))
			{
				HighestOffice = Run.Target;
			}
			ApprovalValue = Clamp(0.45 + 0.4 * (Result.PlayerShare - 0.5) * 2.0, 0.3, 0.8);
			TermEndMinute = Now + static_cast<std::int64_t>(GetOfficeInfo(Run.Target).TermDays) * 24 * 60;
			WinCount += 1;
		}
		else
		{
			LossCount += 1;
		}
		Run = Campaign();
		return Result;
	}

	LawVote PoliticalCareer::Propose(const LawDef& Law, Rng& Random)
	{
		LawVote Vote;
		if (CurrentOffice == Office::None)
		{
			Vote.Message = "Only officeholders can propose laws. Try lobbying instead.";
			return Vote;
		}
		if (static_cast<int>(CurrentOffice) < static_cast<int>(Law.MinimumOffice))
		{
			Vote.Message = std::string("This needs the clout of a ") + OfficeTitle(Law.MinimumOffice) + ".";
			return Vote;
		}
		if (IsEnacted(Law.Id))
		{
			Vote.Message = "That law is already on the books.";
			return Vote;
		}
		const double Base = 0.35 + detail_politics::Clout(CurrentOffice) + 0.3 * (ApprovalValue - 0.5) + 0.2 * Law.Popularity;
		Vote.YesShare = Clamp(Base + 0.06 * Gaussian(Random), 0.0, 1.0);
		Vote.bPassed = Vote.YesShare > 0.5;
		if (Vote.bPassed)
		{
			Laws.push_back(Law.Id);
			ApprovalValue = Clamp(ApprovalValue + 0.06 * Law.Popularity, 0.0, 1.0);
			Vote.Message = Law.Name + " passed with " + std::to_string(static_cast<int>(std::lround(Vote.YesShare * 100.0))) + "% support.";
		}
		else
		{
			ApprovalValue = Clamp(ApprovalValue - 0.02, 0.0, 1.0);
			Vote.Message = Law.Name + " failed (" + std::to_string(static_cast<int>(std::lround(Vote.YesShare * 100.0))) + "% yes).";
		}
		return Vote;
	}

	bool PoliticalCareer::Repeal(const std::string& LawId)
	{
		for (auto It = Laws.begin(); It != Laws.end(); ++It)
		{
			if (*It == LawId)
			{
				Laws.erase(It);
				return true;
			}
		}
		return false;
	}

	LawVote PoliticalCareer::Lobby(const LawDef& Law, Money Amount, Wallet& Funds, std::int64_t Now, Rng& Random)
	{
		LawVote Vote;
		if (IsEnacted(Law.Id))
		{
			Vote.Message = "That law is already on the books.";
			return Vote;
		}
		if (Amount <= 0 || !Funds.TrySpend(Amount, MoneyCategory::Other, "Lobbying: " + Law.Name, Now))
		{
			Vote.Message = "You can't afford that lobbying effort.";
			return Vote;
		}
		const double Pressure = 0.15 * std::log10(1.0 + ToDollars(Amount) / 1000.0);
		Vote.YesShare = Clamp(0.3 + Pressure + 0.2 * Law.Popularity + 0.06 * Gaussian(Random), 0.0, 1.0);
		Vote.bPassed = Vote.YesShare > 0.5;
		if (Vote.bPassed)
		{
			Laws.push_back(Law.Id);
			Vote.Message = "Your lobbying worked: " + Law.Name + " passed.";
		}
		else
		{
			Vote.Message = "Lawmakers weren't persuaded. " + Law.Name + " stalled.";
		}
		return Vote;
	}

	bool PoliticalCareer::IsEnacted(const std::string& LawId) const
	{
		for (const std::string& Id : Laws)
		{
			if (Id == LawId) return true;
		}
		return false;
	}

	Money PoliticalCareer::WeeklySalary() const
	{
		return GetOfficeInfo(CurrentOffice).WeeklySalary;
	}

	void PoliticalCareer::Leave()
	{
		CurrentOffice = Office::None;
		TermEndMinute = 0;
	}

	PoliticalCareer::Snapshot PoliticalCareer::Save() const
	{
		Snapshot Data;
		Data.CurrentOffice = CurrentOffice;
		Data.HighestOffice = HighestOffice;
		Data.Approval = ApprovalValue;
		Data.TermEndMinute = TermEndMinute;
		Data.Laws = Laws;
		Data.Run = Run;
		Data.Wins = WinCount;
		Data.Losses = LossCount;
		return Data;
	}

	void PoliticalCareer::Load(const Snapshot& Data)
	{
		CurrentOffice = Data.CurrentOffice;
		HighestOffice = Data.HighestOffice;
		ApprovalValue = Clamp(Data.Approval, 0.0, 1.0);
		TermEndMinute = Data.TermEndMinute;
		Laws = Data.Laws;
		Run = Data.Run;
		WinCount = Data.Wins;
		LossCount = Data.Losses;
	}

	LawEffects CombineLaws(const std::vector<LawDef>& Catalog, const std::vector<std::string>& Enacted)
	{
		LawEffects Total;
		for (const std::string& Id : Enacted)
		{
			for (const LawDef& Law : Catalog)
			{
				if (Law.Id != Id)
				{
					continue;
				}
				const LawEffects& Fx = Law.Effects;
				Total.SpeedingHeatMul *= Fx.SpeedingHeatMul;
				Total.PoliceUnitsMul *= Fx.PoliceUnitsMul;
				Total.BusinessHealthBonus += Fx.BusinessHealthBonus;
				Total.WageMul *= Fx.WageMul;
				Total.TrafficDensityMul *= Fx.TrafficDensityMul;
				Total.HousingPriceMul *= Fx.HousingPriceMul;
				Total.VehiclePriceMul *= Fx.VehiclePriceMul;
				Total.HackingHeatMul *= Fx.HackingHeatMul;
				Total.TuitionMul *= Fx.TuitionMul;
				Total.bStreetRacingLegal = Total.bStreetRacingLegal || Fx.bStreetRacingLegal;
			}
		}
		return Total;
	}
}
