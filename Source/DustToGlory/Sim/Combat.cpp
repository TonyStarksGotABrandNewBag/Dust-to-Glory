#include "Sim/Combat.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	const char* StrikeKindName(StrikeKind Kind)
	{
		switch (Kind)
		{
		case StrikeKind::Fist: return "Punch";
		case StrikeKind::Power: return "Power";
		case StrikeKind::Vehicle: return "Vehicle";
		case StrikeKind::Count: break;
		}
		return "Hit";
	}

	const Fighter* CombatSystem::Find(const std::string& Id) const
	{
		for (const Fighter& Who : Active)
		{
			if (Who.Id == Id)
			{
				return &Who;
			}
		}
		return nullptr;
	}

	Fighter& CombatSystem::Engage(Fighter Who)
	{
		for (Fighter& Existing : Active)
		{
			if (Existing.Id == Who.Id)
			{
				return Existing;
			}
		}
		// Keep the list short: a brawl is a handful of people, not a crowd.
		if (Active.size() >= 12)
		{
			Active.erase(Active.begin());
		}
		Active.push_back(std::move(Who));
		return Active.back();
	}

	void CombatSystem::Disengage(const std::string& Id)
	{
		Active.erase(std::remove_if(Active.begin(), Active.end(), [&Id](const Fighter& Who) { return Who.Id == Id; }), Active.end());
	}

	double CombatSystem::PlayerDamage(StrikeKind Kind, int FitnessLevel, double PowerPotency)
	{
		const double Fitness = Clamp(FitnessLevel / 10.0, 0.0, 1.0);
		switch (Kind)
		{
		case StrikeKind::Fist:
			// Bare hands: training is most of it.
			return 6.0 + 14.0 * Fitness;
		case StrikeKind::Power:
			// A power hits for what the power is worth, with a little of the body behind it.
			return 12.0 + 38.0 * Clamp(PowerPotency, 0.0, 2.0) * 0.5 + 6.0 * Fitness;
		case StrikeKind::Vehicle:
			// Being hit by a car is being hit by a car.
			return 45.0;
		case StrikeKind::Count:
			break;
		}
		return 0.0;
	}

	StrikeResult CombatSystem::Strike(const std::string& Id, StrikeKind Kind, int FitnessLevel, double PowerPotency, Rng& Random)
	{
		StrikeResult Result;
		Fighter* Target = nullptr;
		for (Fighter& Who : Active)
		{
			if (Who.Id == Id)
			{
				Target = &Who;
			}
		}
		if (!Target || Target->IsDown())
		{
			Result.Summary = "There's nobody there to hit.";
			return Result;
		}
		// Someone trained is harder to land a clean hit on, and a superhuman more so.
		const double Dodge = Clamp(0.1 + 0.3 * Target->Skill + (Target->bSuperhuman ? 0.15 : 0.0) - 0.02 * FitnessLevel, 0.02, 0.6);
		if (Random.Chance(Dodge))
		{
			Result.Summary = Target->Name + " saw it coming.";
			Target->bHostile = true;
			return Result;
		}
		Result.bLanded = true;
		const double Base = PlayerDamage(Kind, FitnessLevel, PowerPotency);
		// Armour of a sort: the tougher they are, the less gets through.
		Result.Damage = Base * (1.0 - 0.35 * Target->Skill) * Random.Range(0.85, 1.15);
		Target->Health = std::max(0.0, Target->Health - Result.Damage);
		Target->bHostile = true;
		Result.bWasInnocent = !Target->bSuperhuman;
		if (Target->IsDown())
		{
			Result.bKnockedOut = true;
			Result.Bounty = Target->bSuperhuman ? Target->Bounty : 0;
			Result.Summary = Target->Name + " goes down.";
		}
		else
		{
			Result.Summary = std::string(StrikeKindName(Kind)) + " lands on " + Target->Name + ".";
		}
		return Result;
	}

	bool CombatSystem::Hurt(double Amount)
	{
		Health = std::max(0.0, Health - std::max(0.0, Amount));
		return Health <= 0.0;
	}

	double CombatSystem::OpponentsAct(int FitnessLevel, Rng& Random, std::vector<std::string>& OutMessages)
	{
		double Total = 0.0;
		for (Fighter& Who : Active)
		{
			if (Who.IsDown() || !Who.bHostile)
			{
				continue;
			}
			// A fit player is harder to hit and takes less when they are.
			const double Miss = Clamp(0.2 + 0.025 * FitnessLevel - 0.15 * Who.Skill, 0.05, 0.7);
			if (Random.Chance(Miss))
			{
				continue;
			}
			double Damage = (4.0 + 16.0 * Who.Skill) * Random.Range(0.8, 1.2);
			if (Who.bSuperhuman)
			{
				Damage *= 1.0 + 0.35 * Who.Threat;
			}
			Damage *= 1.0 - 0.3 * Clamp(FitnessLevel / 10.0, 0.0, 1.0);
			Total += Damage;
			OutMessages.push_back(Who.Name + " catches you.");
		}
		Hurt(Total);
		return Total;
	}

	void CombatSystem::Recover(int Minutes, int FitnessLevel)
	{
		if (Minutes <= 0 || Health >= PlayerMaxHealth)
		{
			return;
		}
		// Roughly a full recovery in a day's rest, sooner if you're in shape.
		const double PerMinute = (PlayerMaxHealth / (24.0 * 60.0)) * (0.7 + 0.06 * Clamp(FitnessLevel, 0, 10));
		Health = std::min(PlayerMaxHealth, Health + PerMinute * Minutes);
	}
}
