// Seasonal illness outbreaks. Now and then a fictional bug sweeps the state; a simple SIR model tracks how many
// people are sick, which thins the streets, slows businesses and makes clinicians busier. A player-developed
// treatment shortens the outbreak, and the state buys it at a negotiated price.
#pragma once

#include "Sim/SimCommon.h"

#include <string>

namespace dtg::sim
{
	struct OutbreakStrain
	{
		std::string Id;
		std::string Name;
		double Spread = 0.3;     // daily transmission rate (SIR beta)
		double Recovery = 0.1;   // daily recovery rate (SIR gamma)
		double Severity = 0.5;   // 0..1: how hard it hits people and the economy
	};

	class OutbreakSystem
	{
	public:
		static constexpr int Population = 400000; // the game state's (scaled) population

		bool IsActive() const { return bActive; }
		const OutbreakStrain& Strain() const { return Current; }
		double Susceptible() const { return S; }
		double Infected() const { return I; }
		double Recovered() const { return R; }
		int DaysActive() const { return Days; }
		double PeakInfected() const { return Peak; }
		bool TreatmentDeployed() const { return bTreated; }

		/** One day. May start a new outbreak (returns 1), end one (returns -1), or neither (0). */
		int DailyUpdate(Rng& Random, double OnsetChance = 0.004);
		/** Starts an outbreak now (tests and scripted events). */
		void Begin(Rng& Random);
		/** A treatment reaching Coverage (0..1) of patients speeds up recovery. */
		void DeployTreatment(double Coverage);

		/** Share of business revenue lost while people stay home (0..~0.3). */
		double EconomicDrag() const;
		/** Multiplier on how many pedestrians are out (1 = normal). */
		double StreetLife() const;
		/** Extra pay for clinicians during the surge. */
		double ClinicianPayMul() const;

		struct Snapshot
		{
			bool bActive = false;
			OutbreakStrain Strain;
			double S = 1.0;
			double I = 0.0;
			double R = 0.0;
			int Days = 0;
			int DaysSinceLast = 365;
			double Peak = 0.0;
			bool bTreated = false;
			double TreatmentBoost = 0.0;
		};
		Snapshot Save() const;
		void Load(const Snapshot& Data);

	private:
		bool bActive = false;
		OutbreakStrain Current;
		double S = 1.0;
		double I = 0.0;
		double R = 0.0;
		int Days = 0;
		int DaysSinceLast = 365;
		double Peak = 0.0;
		bool bTreated = false;
		double TreatmentBoost = 0.0;
	};

	/** How the player prices a treatment sold to the state during an outbreak. */
	enum class TreatmentPricing : int
	{
		AtCost,  // no profit: the community remembers
		Fair,    // an even Nash split of the value created
		Market,  // the Nash price at the player's own bargaining power
		Gouge,   // more than it's worth to the state: profitable, and the public notices
		Count
	};
	const char* TreatmentPricingName(TreatmentPricing Pricing);

	struct TreatmentQuote
	{
		bool bValid = false;
		std::string Reason;
		int Doses = 0;
		Money CostPerDose = 0;
		Money ValuePerDose = 0;    // what a dose is worth to the state (care costs and lost work avoided)
		Money Prices[static_cast<int>(TreatmentPricing::Count)] = {};
		double PlayerPower = 0.5;  // Nash bargaining power at the table
	};

	/** The Nash bargaining quote for selling a treatment to the state health department. */
	TreatmentQuote QuoteTreatmentContract(const OutbreakSystem& Outbreak, double TreatmentQuality, int BusinessSkill, double PublicTrust);
}
