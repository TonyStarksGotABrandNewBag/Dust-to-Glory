// Ministry career. The player can found a congregation, preach, run community outreach and receive a
// salary from weekly tithes and offerings. Integrity matters: paying yourself too much of the giving
// erodes trust and the congregation shrinks.
#pragma once

#include "Sim/SimCommon.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace dtg::sim
{
	enum class SermonTheme : int
	{
		Hope,
		Grace,
		Faith,
		Love,
		Forgiveness,
		Service,
		Stewardship,
		Gratitude,
		Family,
		Perseverance,
		Count
	};

	const char* SermonThemeName(SermonTheme Theme);

	enum class OutreachKind : int
	{
		FoodDrive,
		YouthNight,
		CommunityCleanup,
		HospitalVisits,
		BenefitConcert,
		MissionTrip,
		Count
	};

	struct OutreachInfo
	{
		OutreachKind Kind;
		const char* Name;
		const char* Description;
		Money Cost;
		int Hours;
		int MinVisitors;
		int MaxVisitors;
		double Reputation;
	};

	const OutreachInfo& GetOutreachInfo(OutreachKind Kind);

	enum class Facility : int
	{
		Sanctuary,
		FoodPantry,
		YouthCenter,
		MusicMinistry,
		MediaMinistry,
		Count
	};

	const char* FacilityName(Facility Value);

	/** Ordination path: 0 lay minister, 1 licensed, 2 ordained, 3 senior pastor, 4 bishop. */
	const char* OrdinationTitle(int Level);

	struct SermonResult
	{
		bool bValid = false;
		int NewVisitors = 0;
		int NewMembers = 0;
		double EngagementBefore = 0.0;
		double EngagementAfter = 0.0;
		bool bRepeatTheme = false;
	};

	struct OutreachResult
	{
		bool bValid = false;
		int Visitors = 0;
		int NewMembers = 0;
		double ReputationGained = 0.0;
	};

	struct OfferingResult
	{
		int Attendance = 0;
		Money Total = 0;
		Money Salary = 0;
		Money ToFund = 0;
		int MembersBefore = 0;
		int MembersAfter = 0;
		double IntegrityAfter = 1.0;
	};

	class Ministry
	{
	public:
		static constexpr int MaxFacilityLevel = 3;

		bool IsFounded() const { return !ChurchName.empty(); }
		bool Found(const std::string& ChurchTitle);
		const std::string& Name() const { return ChurchName; }

		int Members() const { return MemberCount; }
		int Capacity() const;
		double Engagement() const { return EngagementValue; }
		double Integrity() const { return IntegrityValue; }
		double Reputation() const { return ReputationValue; }
		Money Fund() const { return FundBalance; }
		int Ordination() const { return OrdinationLevel; }
		void SetOrdination(int Level) { OrdinationLevel = Clamp(Level, 0, 4); }

		/** Share of each week's giving paid to the player as salary, 0..0.5. Above 0.35 hurts integrity. */
		double SalaryShare() const { return SalaryShareValue; }
		void SetSalaryShare(double Share) { SalaryShareValue = Clamp(Share, 0.0, 0.5); }

		int FacilityLevel(Facility Value) const { return Facilities[static_cast<std::size_t>(Value)]; }
		Money UpgradeCost(Facility Value) const;
		/** Paid from the ministry fund, not the player's wallet. */
		bool UpgradeFacility(Facility Value, std::string* OutError);

		/** Performance 0..1 comes from the sermon minigame. */
		SermonResult Preach(SermonTheme Theme, double Performance, Rng& Random);
		bool RunOutreach(OutreachKind Kind, Rng& Random, OutreachResult& Out, std::string* OutError);
		/** Run every Sunday. Grows or shrinks membership and splits the giving between salary and fund. */
		OfferingResult CollectWeeklyOffering(Rng& Random);

		void AddToFund(Money Amount) { FundBalance += std::max<Money>(0, Amount); }

		/** How sound the teaching is (0..1). Gimmicks and shortcuts erode it; exposes punish it. */
		double Doctrine() const { return DoctrineValue; }
		/** A flashy fundraising telethon: a big one-time haul for the fund, at a cost to doctrine and integrity. */
		Money RunTelethon(Rng& Random);
		/** Applies outside reputation damage (a smear, a confirmed allegation, an expose). */
		void ApplyScandal(double TrustLoss);
		/** Teaching series, accountability board, open books: rebuild trust and doctrine over time. */
		bool CommitToAccountability(std::string* OutError);
		/** The ministry collapses: members scatter and the church closes. */
		void Dissolve();

		/** Save/load. */
		struct Snapshot
		{
			std::string Name;
			int Members = 0;
			double Engagement = 0.5;
			double Integrity = 1.0;
			double Reputation = 0.0;
			double SalaryShare = 0.25;
			Money Fund = 0;
			int Ordination = 0;
			std::array<int, static_cast<std::size_t>(Facility::Count)> Facilities{};
			int LastTheme = -1;
			int PendingVisitors = 0;
			int WeeksRun = 0;
			double Doctrine = 1.0;
		};
		Snapshot Save() const;
		void Load(const Snapshot& Data);

	private:
		std::string ChurchName;
		int MemberCount = 0;
		double EngagementValue = 0.5;
		double IntegrityValue = 1.0;
		double ReputationValue = 0.0;
		double SalaryShareValue = 0.25;
		Money FundBalance = 0;
		int OrdinationLevel = 0;
		std::array<int, static_cast<std::size_t>(Facility::Count)> Facilities{};
		int LastTheme = -1;
		int PendingVisitors = 0; // visitors who may join at the next service
		int WeeksRun = 0;
		double DoctrineValue = 1.0;
	};
}
