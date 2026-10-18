#include "Sim/Outbreak.h"

#include "Sim/GameTheory.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	namespace detail_outbreak
	{
		const char* const Names[] = {"Gray Fever", "Lowcountry Flu", "Harbor Cough", "Piedmont Chills", "Palmetto Sniffles", "Tidewater Fever"};
	}

	void OutbreakSystem::Begin(Rng& Random)
	{
		bActive = true;
		Days = 0;
		Peak = 0.0;
		bTreated = false;
		TreatmentBoost = 0.0;
		const char* Name = detail_outbreak::Names[Random.Index(6)];
		Current.Name = Name;
		Current.Id = Slugify(Name) + "-" + std::to_string(Random.NextU32() % 10000u);
		Current.Spread = Random.Range(0.22, 0.4);
		Current.Recovery = Random.Range(0.08, 0.14);
		Current.Severity = Random.Range(0.2, 0.8);
		// Last season left some immunity behind.
		S = 1.0 - std::min(0.3, R * 0.5);
		I = 0.0005;
		R = 1.0 - S - I;
	}

	int OutbreakSystem::DailyUpdate(Rng& Random, double OnsetChance)
	{
		if (!bActive)
		{
			DaysSinceLast += 1;
			R = std::max(0.0, R * 0.995); // immunity fades
			S = 1.0 - R;
			if (DaysSinceLast > 60 && Random.Chance(OnsetChance))
			{
				Begin(Random);
				return 1;
			}
			return 0;
		}
		Days += 1;
		// SIR in daily steps (split into four for stability).
		for (int Step = 0; Step < 4; ++Step)
		{
			const double NewCases = Current.Spread * S * I * 0.25;
			const double Recoveries = (Current.Recovery + TreatmentBoost) * I * 0.25;
			S = std::max(0.0, S - NewCases);
			I = std::max(0.0, I + NewCases - Recoveries);
			R = std::min(1.0, R + Recoveries);
		}
		Peak = std::max(Peak, I);
		if ((I < 0.0002 && Days > 10) || Days > 240)
		{
			bActive = false;
			DaysSinceLast = 0;
			I = 0.0;
			S = 1.0 - R;
			return -1;
		}
		return 0;
	}

	void OutbreakSystem::DeployTreatment(double Coverage)
	{
		if (!bActive)
		{
			return;
		}
		bTreated = true;
		TreatmentBoost = std::max(TreatmentBoost, 0.25 * Clamp(Coverage, 0.0, 1.0));
	}

	double OutbreakSystem::EconomicDrag() const
	{
		return bActive ? Clamp(I * 2.5 * (0.5 + Current.Severity), 0.0, 0.3) : 0.0;
	}

	double OutbreakSystem::StreetLife() const
	{
		return bActive ? Clamp(1.0 - I * 4.0 * (0.5 + Current.Severity), 0.35, 1.0) : 1.0;
	}

	double OutbreakSystem::ClinicianPayMul() const
	{
		return bActive ? 1.0 + Clamp(I * 10.0, 0.0, 1.0) : 1.0;
	}

	OutbreakSystem::Snapshot OutbreakSystem::Save() const
	{
		Snapshot Data;
		Data.bActive = bActive;
		Data.Strain = Current;
		Data.S = S;
		Data.I = I;
		Data.R = R;
		Data.Days = Days;
		Data.DaysSinceLast = DaysSinceLast;
		Data.Peak = Peak;
		Data.bTreated = bTreated;
		Data.TreatmentBoost = TreatmentBoost;
		return Data;
	}

	void OutbreakSystem::Load(const Snapshot& Data)
	{
		bActive = Data.bActive;
		Current = Data.Strain;
		S = Data.S;
		I = Data.I;
		R = Data.R;
		Days = Data.Days;
		DaysSinceLast = Data.DaysSinceLast;
		Peak = Data.Peak;
		bTreated = Data.bTreated;
		TreatmentBoost = Data.TreatmentBoost;
	}

	const char* TreatmentPricingName(TreatmentPricing Pricing)
	{
		switch (Pricing)
		{
		case TreatmentPricing::AtCost: return "At cost";
		case TreatmentPricing::Fair: return "Fair price";
		case TreatmentPricing::Market: return "Negotiated price";
		case TreatmentPricing::Gouge: return "Maximum price";
		case TreatmentPricing::Count: break;
		}
		return "Price";
	}

	TreatmentQuote QuoteTreatmentContract(const OutbreakSystem& Outbreak, double TreatmentQuality, int BusinessSkill, double PublicTrust)
	{
		TreatmentQuote Quote;
		if (!Outbreak.IsActive())
		{
			Quote.Reason = "There's no outbreak to treat right now.";
			return Quote;
		}
		if (Outbreak.TreatmentDeployed())
		{
			Quote.Reason = "The state already has a treatment contract for this outbreak.";
			return Quote;
		}
		const double Patients = std::max(Outbreak.Infected(), Outbreak.PeakInfected() * 0.5) * OutbreakSystem::Population;
		Quote.Doses = std::max(1000, static_cast<int>(std::lround(Patients * 0.6)));
		const double Cost = 8.0;
		const double Value = (40.0 + 160.0 * Outbreak.Strain().Severity) * (0.6 + 0.4 * Clamp(TreatmentQuality, 0.0, 1.0));
		// The surplus between production cost and the state's value per dose is split by Nash bargaining.
		Quote.PlayerPower = Clamp(0.35 + 0.03 * BusinessSkill + 0.1 * PublicTrust, 0.1, 0.9);
		const Bargain Even = NashBargain(Value - Cost, 0.0, 0.0, 0.5);
		const Bargain Negotiated = NashBargain(Value - Cost, 0.0, 0.0, Quote.PlayerPower);
		Quote.CostPerDose = ToMoney(Cost);
		Quote.ValuePerDose = ToMoney(Value);
		Quote.Prices[static_cast<int>(TreatmentPricing::AtCost)] = ToMoney(Cost);
		Quote.Prices[static_cast<int>(TreatmentPricing::Fair)] = ToMoney(Cost + Even.PayoffA);
		Quote.Prices[static_cast<int>(TreatmentPricing::Market)] = ToMoney(Cost + Negotiated.PayoffA);
		Quote.Prices[static_cast<int>(TreatmentPricing::Gouge)] = ToMoney(Value * 1.5);
		Quote.bValid = true;
		return Quote;
	}
}
