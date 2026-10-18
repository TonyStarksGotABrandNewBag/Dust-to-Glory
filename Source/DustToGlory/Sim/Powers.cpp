#include "Sim/Powers.h"

namespace dtg::sim
{
	namespace detail_powers
	{
		struct NamedEffect
		{
			PowerEffect Value;
			const char* Name;
		};

		constexpr NamedEffect EffectNames[] = {
			{PowerEffect::Flight, "Flight"},
			{PowerEffect::SuperStrength, "SuperStrength"},
			{PowerEffect::SuperSpeed, "SuperSpeed"},
			{PowerEffect::Telekinesis, "Telekinesis"},
			{PowerEffect::EnergyBlast, "EnergyBlast"},
			{PowerEffect::Invisibility, "Invisibility"},
			{PowerEffect::HealingFactor, "HealingFactor"},
			{PowerEffect::Teleport, "Teleport"},
			{PowerEffect::Phasing, "Phasing"},
			{PowerEffect::Cryokinesis, "Cryokinesis"},
			{PowerEffect::Pyrokinesis, "Pyrokinesis"},
			{PowerEffect::Electrokinesis, "Electrokinesis"},
			{PowerEffect::WeatherControl, "WeatherControl"},
			{PowerEffect::TimeSlow, "TimeSlow"},
			{PowerEffect::Grapple, "Grapple"},
			{PowerEffect::SonicBoom, "SonicBoom"},
			{PowerEffect::Shapeshift, "Shapeshift"},
			{PowerEffect::ForceField, "ForceField"},
		};

		// Original codename parts. Deliberately avoids words tied to existing comic characters.
		const std::vector<std::string>& Adjectives()
		{
			static const std::vector<std::string> Words = {
				"Crimson", "Static", "Hollow", "Ember", "Glacier", "Vortex", "Quantum", "Phantom", "Solar", "Lunar",
				"Rapid", "Granite", "Echo", "Cobalt", "Scarlet", "Neon", "Obsidian", "Tidal", "Prism", "Rust",
				"Velvet", "Thunder", "Gravity", "Copper", "Aurora", "Hex", "Nova", "Cinder", "Frost", "Volt"};
			return Words;
		}

		const std::vector<std::string>& Nouns()
		{
			static const std::vector<std::string> Words = {
				"Warden", "Specter", "Comet", "Sentinel", "Viper", "Tempest", "Rook", "Paladin", "Wraith", "Harrier",
				"Colossus", "Harbinger", "Drifter", "Vector", "Lancer", "Shade", "Bastion", "Mirage", "Pulse", "Monarch",
				"Jackal", "Beacon", "Courier", "Golem", "Siren", "Nomad", "Ranger", "Gambit", "Anvil", "Kestrel"};
			return Words;
		}

		const std::vector<std::string>& VillainMotives()
		{
			static const std::vector<std::string> Lines = {
				"Wants control of the city power grid.",
				"Steals experimental tech to build a weather machine.",
				"Holds a grudge against the city council.",
				"Runs a smuggling ring out of the docks.",
				"Believes superhumans should rule the city.",
				"Is trying to blackmail the mayor with stolen data.",
				"Plans to rig the championship with sabotage.",
				"Wants the museum's meteorite collection."};
			return Lines;
		}

		const std::vector<std::string>& HeroMotives()
		{
			static const std::vector<std::string> Lines = {
				"Protects the harbor district.",
				"Mentors kids at the youth center between patrols.",
				"Hunts the villain who caused their mutation.",
				"Keeps the night shift safe for nurses and drivers.",
				"Rescues people from fires and floods.",
				"Is trying to prove superhumans can be trusted."};
			return Lines;
		}

		void ApplyMods(EffectivePower& Power, const PowerStats& Mods)
		{
			Power.Potency *= Mods.Potency;
			Power.RangeM *= Mods.Range;
			Power.CooldownS *= Mods.Cooldown;
			Power.EnergyCost *= Mods.EnergyCost;
			Power.DurationS *= Mods.Duration;
		}

		bool HasMutation(const PowerInstance& Instance, const std::string& Id)
		{
			for (const std::string& Existing : Instance.Mutations)
			{
				if (Existing == Id)
				{
					return true;
				}
			}
			return false;
		}
	}

	const char* PowerEffectName(PowerEffect Value)
	{
		for (const auto& Entry : detail_powers::EffectNames)
		{
			if (Entry.Value == Value)
			{
				return Entry.Name;
			}
		}
		return "EnergyBlast";
	}

	bool ParsePowerEffect(const std::string& Text, PowerEffect& Out)
	{
		const std::string Lower = ToLowerAscii(Text);
		for (const auto& Entry : detail_powers::EffectNames)
		{
			if (ToLowerAscii(Entry.Name) == Lower)
			{
				Out = Entry.Value;
				return true;
			}
		}
		return false;
	}

	void PowerRoster::AddPower(PowerDef Def)
	{
		PowerDefs.push_back(std::move(Def));
	}

	void PowerRoster::AddMutation(MutationDef Def)
	{
		MutationDefs.push_back(std::move(Def));
	}

	const PowerDef* PowerRoster::FindPower(const std::string& Id) const
	{
		for (const PowerDef& Def : PowerDefs)
		{
			if (Def.Id == Id)
			{
				return &Def;
			}
		}
		return nullptr;
	}

	const MutationDef* PowerRoster::FindMutation(const std::string& Id) const
	{
		for (const MutationDef& Def : MutationDefs)
		{
			if (Def.Id == Id)
			{
				return &Def;
			}
		}
		return nullptr;
	}

	std::vector<const PowerDef*> PowerRoster::StarterPowers() const
	{
		std::vector<const PowerDef*> Result;
		for (const PowerDef& Def : PowerDefs)
		{
			if (Def.bStarter)
			{
				Result.push_back(&Def);
			}
		}
		return Result;
	}

	EffectivePower PowerRoster::Evaluate(const PowerInstance& Instance) const
	{
		EffectivePower Result;
		const PowerDef* Def = FindPower(Instance.PowerId);
		if (!Def)
		{
			return Result;
		}
		const double L = static_cast<double>(Clamp(Instance.Level, 1, PowerSet::MaxLevel) - 1);
		Result.Def = Def;
		Result.Potency = Def->BasePotency * (1.0 + 0.08 * L);
		Result.RangeM = Def->BaseRangeM * (1.0 + 0.05 * L);
		Result.CooldownS = Def->BaseCooldownS * std::max(0.4, 1.0 - 0.04 * L);
		Result.EnergyCost = Def->BaseEnergy * std::max(0.5, 1.0 - 0.03 * L);
		Result.DurationS = Def->BaseDurationS * (1.0 + 0.06 * L);
		for (const std::string& MutationId : Instance.Mutations)
		{
			if (const MutationDef* Mutation = FindMutation(MutationId))
			{
				detail_powers::ApplyMods(Result, Mutation->Mods);
			}
		}
		Result.CooldownS = std::max(0.25, Result.CooldownS);
		Result.EnergyCost = std::max(1.0, Result.EnergyCost);
		return Result;
	}

	bool PowerSet::Choose(const PowerRoster& Roster, const std::vector<std::string>& Ids, std::string* OutError)
	{
		if (static_cast<int>(Ids.size()) != SlotCount)
		{
			if (OutError)
			{
				*OutError = "Choose exactly three powers.";
			}
			return false;
		}
		for (std::size_t Index = 0; Index < Ids.size(); ++Index)
		{
			const PowerDef* Def = Roster.FindPower(Ids[Index]);
			if (!Def || !Def->bStarter)
			{
				if (OutError)
				{
					*OutError = "Unknown power: " + Ids[Index];
				}
				return false;
			}
			for (std::size_t Other = 0; Other < Index; ++Other)
			{
				if (Ids[Other] == Ids[Index])
				{
					if (OutError)
					{
						*OutError = "Each power can only be chosen once.";
					}
					return false;
				}
			}
		}
		SlotList.clear();
		for (const std::string& Id : Ids)
		{
			PowerInstance Instance;
			Instance.PowerId = Id;
			SlotList.push_back(Instance);
		}
		return true;
	}

	const PowerInstance* PowerSet::Slot(int Index) const
	{
		if (Index < 0 || Index >= static_cast<int>(SlotList.size()))
		{
			return nullptr;
		}
		return &SlotList[static_cast<std::size_t>(Index)];
	}

	bool PowerSet::Mutate(PowerInstance& Instance, const PowerRoster& Roster, Rng& Random, MutationEvent& Out)
	{
		const PowerDef* Def = Roster.FindPower(Instance.PowerId);
		if (!Def)
		{
			return false;
		}
		Out.PowerBefore = Instance.PowerId;
		Out.PowerAfter = Instance.PowerId;
		Out.MutationId.clear();
		Out.bEvolved = false;

		// Evolution: a heavily mutated power can transform into its advanced form.
		if (!Def->EvolvesTo.empty() && Instance.Mutations.size() >= 2 && Roster.FindPower(Def->EvolvesTo) && Random.Chance(0.4))
		{
			Instance.PowerId = Def->EvolvesTo;
			Instance.Instability = 0.0;
			Out.PowerAfter = Instance.PowerId;
			Out.bEvolved = true;
			return true;
		}

		std::vector<const MutationDef*> Candidates;
		double TotalWeight = 0.0;
		for (const std::string& MutationId : Def->Mutations)
		{
			const MutationDef* Mutation = Roster.FindMutation(MutationId);
			if (Mutation && !detail_powers::HasMutation(Instance, MutationId))
			{
				Candidates.push_back(Mutation);
				TotalWeight += std::max(0.0, Mutation->Weight);
			}
		}
		if (Candidates.empty() || TotalWeight <= 0.0)
		{
			return false;
		}
		double Roll = Random.Range(0.0, TotalWeight);
		const MutationDef* Chosen = Candidates.back();
		for (const MutationDef* Candidate : Candidates)
		{
			Roll -= std::max(0.0, Candidate->Weight);
			if (Roll <= 0.0)
			{
				Chosen = Candidate;
				break;
			}
		}
		Instance.Mutations.push_back(Chosen->Id);
		Instance.Instability = 0.0;
		Out.MutationId = Chosen->Id;
		return true;
	}

	bool PowerSet::RecordUse(int SlotIndex, const PowerRoster& Roster, Rng& Random, MutationEvent& Out, bool& bOutLevelUp)
	{
		bOutLevelUp = false;
		if (SlotIndex < 0 || SlotIndex >= static_cast<int>(SlotList.size()))
		{
			return false;
		}
		PowerInstance& Instance = SlotList[static_cast<std::size_t>(SlotIndex)];
		Instance.Xp += 10;
		while (Instance.Level < MaxLevel && Instance.Xp >= 40 * Instance.Level)
		{
			Instance.Xp -= 40 * Instance.Level;
			Instance.Level += 1;
			bOutLevelUp = true;
		}
		Instance.Instability += 0.05 + 0.05 * Random.NextDouble();
		if (Instance.Instability >= 1.0 || Random.Chance(0.01))
		{
			if (Mutate(Instance, Roster, Random, Out))
			{
				Out.Slot = SlotIndex;
				return true;
			}
			Instance.Instability = 0.5;
		}
		return false;
	}

	bool PowerSet::Surge(const PowerRoster& Roster, Rng& Random, MutationEvent& Out)
	{
		if (SlotList.empty())
		{
			return false;
		}
		const int SlotIndex = static_cast<int>(Random.Index(SlotList.size()));
		if (Mutate(SlotList[static_cast<std::size_t>(SlotIndex)], Roster, Random, Out))
		{
			Out.Slot = SlotIndex;
			return true;
		}
		return false;
	}

	Superhuman GenerateSuperhuman(const PowerRoster& Roster, Alignment Side, int Threat, Rng& Random)
	{
		Superhuman Result;
		Result.Side = Side;
		Result.Threat = Clamp(Threat, 1, 5);
		Result.Codename = Random.Pick(detail_powers::Adjectives()) + " " + Random.Pick(detail_powers::Nouns());
		Result.Id = Slugify(Result.Codename) + "-" + std::to_string(Random.NextU32() % 100000u);
		Result.MaxHealth = 200.0 + 150.0 * Result.Threat;
		Result.Bounty = Side == Alignment::Villain ? ToMoney(std::round(2500.0 * std::pow(Result.Threat, 1.7) / 50.0) * 50.0) : 0;
		Result.Motive = Side == Alignment::Villain ? Random.Pick(detail_powers::VillainMotives()) : Random.Pick(detail_powers::HeroMotives());

		const auto& All = Roster.Powers();
		if (All.empty())
		{
			return Result;
		}
		const int PowerCount = std::min(static_cast<int>(All.size()), 1 + (Result.Threat >= 3 ? 1 : 0) + (Result.Threat >= 5 ? 1 : 0));
		std::vector<std::size_t> Picked;
		int Attempts = 0;
		while (static_cast<int>(Picked.size()) < PowerCount && Attempts < 64)
		{
			++Attempts;
			const std::size_t Index = Random.Index(All.size());
			bool bDuplicate = false;
			for (const std::size_t Existing : Picked)
			{
				bDuplicate = bDuplicate || Existing == Index;
			}
			if (!bDuplicate)
			{
				Picked.push_back(Index);
			}
		}
		for (const std::size_t Index : Picked)
		{
			PowerInstance Instance;
			Instance.PowerId = All[Index].Id;
			Instance.Level = Clamp(Result.Threat * 2, 1, PowerSet::MaxLevel);
			for (int Roll = 1; Roll < Result.Threat; ++Roll)
			{
				if (Random.Chance(0.5))
				{
					MutationEvent Ignored;
					PowerSet::Mutate(Instance, Roster, Random, Ignored);
				}
			}
			Result.Powers.push_back(Instance);
		}
		return Result;
	}
}
