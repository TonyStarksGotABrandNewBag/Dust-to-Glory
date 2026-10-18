// Politics: run for office from City Council to President, campaign, win elections and pass laws that
// change how the rest of the game behaves.
#pragma once

#include "Sim/Economy.h"
#include "Sim/GameTheory.h"
#include "Sim/SimCommon.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	enum class Office : int
	{
		None,
		CityCouncil,
		Mayor,
		StateRepresentative,
		Governor,
		Senator,
		President,
		Count
	};

	struct OfficeInfo
	{
		Office Id;
		const char* Title;
		int FollowersNeeded;
		Office Requires;          // an office that must have been held first (None = no requirement)
		Money FilingFee;
		Money TypicalBudget;      // what a competitive campaign spends on ads
		int CampaignDays;
		int TermDays;
		Money WeeklySalary;
		double OpponentStrength;  // starting poll share of the opponent
	};

	const OfficeInfo& GetOfficeInfo(Office Value);
	const char* OfficeTitle(Office Value);

	/** System-wide effects of a law. Multipliers default to "no change". */
	struct LawEffects
	{
		double SpeedingHeatMul = 1.0;
		double PoliceUnitsMul = 1.0;
		double BusinessHealthBonus = 0.0;
		double WageMul = 1.0;
		double TrafficDensityMul = 1.0;
		double HousingPriceMul = 1.0;
		double VehiclePriceMul = 1.0;
		double HackingHeatMul = 1.0;
		double TuitionMul = 1.0;
		bool bStreetRacingLegal = false;
	};

	struct LawDef
	{
		std::string Id;
		std::string Name;
		std::string Description;
		Office MinimumOffice = Office::CityCouncil;
		double Popularity = 0.0;   // -1..1: how the public feels about it
		LawEffects Effects;
	};

	struct Campaign
	{
		bool bActive = false;
		Office Target = Office::None;
		double Support = 0.3;
		double OpponentSupport = 0.5;
		std::string Opponent;
		Money Spent = 0;
		int Rallies = 0;
		int Debates = 0;
		int Endorsements = 0;
		std::int64_t ElectionMinute = 0;
	};

	struct CampaignRound
	{
		bool bPlayed = false;
		int PlayerChoice = 0;
		int OpponentChoice = 0;
		double SupportChange = 0.0;
		Equilibrium Solution;
	};

	struct ElectionResult
	{
		bool bHeld = false;
		bool bWon = false;
		Office Target = Office::None;
		double PlayerShare = 0.0;
		double OpponentShare = 0.0;
		std::string Opponent;
	};

	struct LawVote
	{
		bool bPassed = false;
		double YesShare = 0.0;
		std::string Message;
	};

	class PoliticalCareer
	{
	public:
		Office Current() const { return CurrentOffice; }
		Office Highest() const { return HighestOffice; }
		double Approval() const { return ApprovalValue; }
		const Campaign& ActiveCampaign() const { return Run; }
		const std::vector<std::string>& EnactedLaws() const { return Laws; }
		std::int64_t TermEnds() const { return TermEndMinute; }

		bool CanRun(Office Target, int Followers, double Trust, std::string* OutReason) const;
		/** Files paperwork (fee from the wallet) and opens a campaign. Starting support comes from reputation. */
		bool StartCampaign(Office Target, int Followers, double Trust, double ReputationScore, Wallet& Funds, std::int64_t Now, Rng& Random, std::string* OutError);
		/** Campaign actions; each returns the change in support. */
		double Rally(double Performance);
		double RunAds(Money Amount, Wallet& Funds, std::int64_t Now);
		double Debate(double Performance);
		double Endorsement(double EndorserReputation);
		/** A strategy round of the campaign game (see CampaignGame); the opponent plays its Nash equilibrium mix. */
		CampaignRound PlayStrategy(const MatrixGame& Game, int PlayerChoice, double Performance, Rng& Random);
		/** Scandals hit the polls too. */
		void Scandal(double Severity);
		/** Holds the election when its day arrives. */
		ElectionResult Update(std::int64_t Now, Rng& Random);

		/** In office: propose a law. Passing depends on approval, the office's clout and how popular the law is. */
		LawVote Propose(const LawDef& Law, Rng& Random);
		bool Repeal(const std::string& LawId);
		/** Outside office (or above it): fund a lobbying effort. */
		LawVote Lobby(const LawDef& Law, Money Amount, Wallet& Funds, std::int64_t Now, Rng& Random);
		bool IsEnacted(const std::string& LawId) const;
		/** Weekly paycheck while in office (0 when not). */
		Money WeeklySalary() const;
		void Leave();

		struct Snapshot
		{
			Office CurrentOffice = Office::None;
			Office HighestOffice = Office::None;
			double Approval = 0.5;
			std::int64_t TermEndMinute = 0;
			std::vector<std::string> Laws;
			Campaign Run;
			int Wins = 0;
			int Losses = 0;
		};
		Snapshot Save() const;
		void Load(const Snapshot& Data);
		int Wins() const { return WinCount; }
		int Losses() const { return LossCount; }

	private:
		Office CurrentOffice = Office::None;
		Office HighestOffice = Office::None;
		double ApprovalValue = 0.5;
		std::int64_t TermEndMinute = 0;
		std::vector<std::string> Laws;
		Campaign Run;
		int WinCount = 0;
		int LossCount = 0;
	};

	/** Combined effects of every enacted law. */
	LawEffects CombineLaws(const std::vector<LawDef>& Catalog, const std::vector<std::string>& Enacted);
}
