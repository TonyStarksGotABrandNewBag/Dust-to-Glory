#include "Sim/Wanted.h"

namespace dtg::sim
{
	namespace detail_wanted
	{
		double HeatFor(Crime Value)
		{
			switch (Value)
			{
			case Crime::Speeding: return 30.0;
			case Crime::RecklessDriving: return 50.0;
			case Crime::VehicleTheft: return 100.0;
			case Crime::PropertyDamage: return 40.0;
			case Crime::HitPoliceCar: return 120.0;
			case Crime::EvadingPolice: return 60.0;
			case Crime::UnauthorizedHack: return 100.0;
			case Crime::Assault: return 80.0;
			case Crime::Fraud: return 150.0;
			case Crime::Bribery: return 120.0;
			case Crime::Escape: return 400.0;
			case Crime::Conspiracy: return 500.0;
			case Crime::Count: break;
			}
			return 0.0;
		}

		double CivilianReportChance(Crime Value)
		{
			switch (Value)
			{
			case Crime::VehicleTheft: return 0.6;
			case Crime::PropertyDamage: return 0.3;
			case Crime::RecklessDriving: return 0.15;
			case Crime::Assault: return 0.7;
			default: return 0.0;
			}
		}
	}

	const char* CrimeName(Crime Value)
	{
		switch (Value)
		{
		case Crime::Speeding: return "Speeding";
		case Crime::RecklessDriving: return "Reckless driving";
		case Crime::VehicleTheft: return "Vehicle theft";
		case Crime::PropertyDamage: return "Property damage";
		case Crime::HitPoliceCar: return "Hitting a police car";
		case Crime::EvadingPolice: return "Evading police";
		case Crime::UnauthorizedHack: return "Unauthorized hacking";
		case Crime::Assault: return "Assault";
		case Crime::Fraud: return "Fraud";
		case Crime::Bribery: return "Bribing an official";
		case Crime::Escape: return "Escape from custody";
		case Crime::Conspiracy: return "Criminal conspiracy";
		case Crime::Count: break;
		}
		return "Crime";
	}

	int WantedSystem::Stars() const
	{
		return Clamp(static_cast<int>(HeatValue / 100.0), 0, MaxStars);
	}

	void WantedSystem::ReportCrime(Crime Value, bool bPoliceWitness, bool bCivilianWitness, Rng& Random)
	{
		bool bReported = bPoliceWitness;
		if (!bReported && bCivilianWitness)
		{
			bReported = Random.Chance(detail_wanted::CivilianReportChance(Value));
		}
		if (!bReported)
		{
			return;
		}
		// Speeding alone never escalates past one star.
		double Added = detail_wanted::HeatFor(Value);
		if (Value == Crime::Speeding)
		{
			Added = HeatValue >= 100.0 ? 5.0 : Added;
		}
		HeatValue = Clamp(HeatValue + Added, 0.0, MaxStars * 100.0 + 99.0);
		if (bPoliceWitness)
		{
			SearchTimer = SearchDuration();
		}
	}

	void WantedSystem::AddStars(int Count)
	{
		if (Count > 0)
		{
			HeatValue = Clamp(HeatValue + 100.0 * Count, 0.0, MaxStars * 100.0 + 99.0);
			SearchTimer = SearchDuration();
		}
	}

	double WantedSystem::SearchDuration() const
	{
		return 12.0 + 8.0 * Stars();
	}

	void WantedSystem::Update(double Dt, bool bPoliceHaveVisual)
	{
		if (!IsWanted())
		{
			// Sub-star heat cools off on its own.
			HeatValue = std::max(0.0, HeatValue - 4.0 * Dt);
			SearchTimer = 0.0;
			return;
		}
		if (bPoliceHaveVisual)
		{
			SearchTimer = SearchDuration();
			return;
		}
		SearchTimer -= Dt;
		if (SearchTimer <= 0.0)
		{
			Clear();
		}
	}

	void WantedSystem::Clear()
	{
		HeatValue = 0.0;
		SearchTimer = 0.0;
	}

	int WantedSystem::UnitsWanted() const
	{
		const int StarCount = Stars();
		return StarCount == 0 ? 0 : 1 + 2 * StarCount;
	}

	Money WantedSystem::BustFine(Money Balance) const
	{
		const Money Share = Balance > 0 ? Balance / 20 : 0;
		return Share + Dollars(500) * std::max(1, Stars());
	}
}
