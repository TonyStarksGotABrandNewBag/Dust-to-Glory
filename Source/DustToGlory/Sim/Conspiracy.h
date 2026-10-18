// The villain route: arranging an outbreak so your company can sell the cure.
//
// The player recruits insiders -- a lab technician, a shipping clerk, a health official, a fixer -- each of whom
// names a price (a Nash bargain over the scheme's expected take). With enough of them the strain can be released;
// it spreads like any other outbreak (see Outbreak.h) while the player's medicine is the only treatment going.
//
// Every conspirator is also a liability. Investigators build a case day by day, and each insider plays a
// prisoner's dilemma against the player: stay quiet and share the money, or flip first for immunity. The more
// people who know, the sloppier the scheme, the more the player is squeezing the state, the faster it unravels.
// Exposure means a conspiracy charge, voided contracts and a reputation nobody comes back from quickly.
//
// Everything here is game bookkeeping -- costs, odds, loyalty and payoffs. Nothing describes how anything is
// actually done.
#pragma once

#include "Sim/GameTheory.h"
#include "Sim/SimCommon.h"
#include "Sim/Wanted.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	/** The kind of insider a scheme needs; each one changes what the scheme can do and what it risks. */
	enum class InsiderRole : int
	{
		LabTech,       // access to the sample: without one there is nothing to release
		Courier,       // moves it quietly; cuts the trail investigators pick up
		HealthOfficial,// slows the official response, so the outbreak runs longer
		Fixer,         // buries loose ends: lowers how fast the case builds
		Count
	};
	const char* InsiderRoleName(InsiderRole Role);
	const char* InsiderRoleDescription(InsiderRole Role);

	struct Insider
	{
		std::string NpcId;
		std::string Name;
		InsiderRole Role = InsiderRole::LabTech;
		double Cut = 0.0;        // share of the scheme's take they bargained for
		Money Retainer = 0;      // paid up front
		double Loyalty = 0.5;    // 0..1 chance-weighting toward staying quiet
		bool bFlipped = false;   // turned state's witness
	};

	/** What an insider would want to come aboard, worked out before the player commits. */
	struct RecruitQuote
	{
		bool bAvailable = false;
		std::string Reason;
		double Cut = 0.0;
		Money Retainer = 0;
		double Loyalty = 0.5;
		double FlipRisk = 0.0;   // chance per day they go to investigators once the scheme is live
	};

	enum class SchemeStage : int
	{
		None,
		Planning,   // recruiting, nothing released
		Released,   // the strain is loose
		Unraveling, // investigators have a case and are closing
		Exposed,    // it came apart: charges filed
		GotAway,    // the outbreak ended and the case went cold
		Count
	};
	const char* SchemeStageName(SchemeStage Stage);

	/** One day of the scheme: how much closer investigators got and who talked. */
	struct SchemeDay
	{
		double Heat = 0.0;            // 0..1 how complete the investigators' case is
		bool bFlipped = false;
		std::string FlippedName;
		bool bExposed = false;
		std::string Headline;
	};

	class ConspiracySystem
	{
	public:
		SchemeStage Stage() const { return StageValue; }
		bool IsActive() const { return StageValue == SchemeStage::Planning || StageValue == SchemeStage::Released || StageValue == SchemeStage::Unraveling; }
		bool IsLive() const { return StageValue == SchemeStage::Released || StageValue == SchemeStage::Unraveling; }
		const std::vector<Insider>& Insiders() const { return Crew; }
		bool HasRole(InsiderRole Role) const;
		double Heat() const { return HeatValue; }
		int DaysLive() const { return Days; }
		Money Spent() const { return Outlay; }
		Money Take() const { return Proceeds; }
		/** Everything the scheme is expected to earn, which is what the crew bargains over. */
		static Money ExpectedTake(double MedicineQuality, Money Treasury);

		/** What this NPC would want to join, given their loyalty to the player and the scheme's exposure so far. */
		RecruitQuote QuoteInsider(InsiderRole Role, const std::string& Name, double Affinity, double Awareness, int BusinessSkill,
			double MedicineQuality, Money Treasury) const;
		/** Brings them in (the caller charges the retainer). */
		void Recruit(const std::string& NpcId, const std::string& Name, InsiderRole Role, const RecruitQuote& Terms);
		/** A lab tech is the one role the scheme can't do without. */
		bool CanRelease(std::string* OutReason) const;
		void BeginPlanning();
		void Release();
		/** Payment after a treatment contract: a bigger take keeps the crew quiet, skimming them does not. */
		void SharePayout(Money PlayerProfit, bool bPaidTheCrew);

		/**
		 * One day. Investigators work the case; each insider plays the prisoner's dilemma (flip first for immunity,
		 * or stay quiet and keep their cut). Price gouging and a loud outbreak both speed this up.
		 */
		SchemeDay DailyUpdate(bool bOutbreakRunning, double PublicAnger, Rng& Random);
		/** The payoff matrix an insider is actually looking at: rows you, columns them; payoffs in dollars. */
		MatrixGame InsiderDilemma(const Insider& Who, double MedicineQuality, Money Treasury) const;
		/** Charges if it comes apart. */
		std::vector<Crime> Charges() const;
		void MarkExposed();
		void Close();
		/** How much the health official slows the official response (0 = none). */
		double ResponseDrag() const;

		struct Snapshot
		{
			SchemeStage Stage = SchemeStage::None;
			std::vector<Insider> Crew;
			double Heat = 0.0;
			int Days = 0;
			Money Outlay = 0;
			Money Proceeds = 0;
			bool bShared = false;
		};
		Snapshot Save() const;
		void Load(const Snapshot& Data);

	private:
		SchemeStage StageValue = SchemeStage::None;
		std::vector<Insider> Crew;
		double HeatValue = 0.0;
		int Days = 0;
		Money Outlay = 0;
		Money Proceeds = 0;
		bool bShared = true;
	};
}
