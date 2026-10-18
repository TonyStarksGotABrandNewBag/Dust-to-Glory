// Police heat. Crimes add heat (one star per 100 points, five stars max). Out of police sight the search
// timer runs; when it expires the heat clears, as in classic open-world games.
#pragma once

#include "Sim/SimCommon.h"

namespace dtg::sim
{
	enum class Crime : int
	{
		Speeding,
		RecklessDriving,
		VehicleTheft,
		PropertyDamage,
		HitPoliceCar,
		EvadingPolice,
		UnauthorizedHack,
		Count
	};

	const char* CrimeName(Crime Value);

	class WantedSystem
	{
	public:
		static constexpr int MaxStars = 5;

		int Stars() const;
		double Heat() const { return HeatValue; }
		bool IsWanted() const { return HeatValue >= 100.0; }

		/** Police witnesses always count; civilians report some crimes some of the time. */
		void ReportCrime(Crime Value, bool bPoliceWitness, bool bCivilianWitness, Rng& Random);
		/** Adds whole stars directly (grey-market hacks). */
		void AddStars(int Count);
		void Update(double Dt, bool bPoliceHaveVisual);
		void Clear();

		bool IsSearching() const { return IsWanted() && SearchTimer > 0.0; }
		double SearchRemaining() const { return SearchTimer; }
		/** Seconds police need to lose you before heat clears. */
		double SearchDuration() const;
		/** How many police units should be hunting the player. */
		int UnitsWanted() const;
		/** Fine when busted: a share of cash plus a per-star amount. */
		Money BustFine(Money Balance) const;

	private:
		double HeatValue = 0.0;
		double SearchTimer = 0.0;
	};
}
