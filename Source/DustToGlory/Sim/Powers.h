#pragma once

#include "Sim/SimCommon.h"

#include <string>
#include <utility>
#include <vector>

namespace dtg::sim
{
	/** What the engine does when a power fires. Data files pick one effect per power. */
	enum class PowerEffect : int
	{
		Flight,
		SuperStrength,
		SuperSpeed,
		Telekinesis,
		EnergyBlast,
		Invisibility,
		HealingFactor,
		Teleport,
		Phasing,
		Cryokinesis,
		Pyrokinesis,
		Electrokinesis,
		WeatherControl,
		TimeSlow,
		Grapple,
		SonicBoom,
		Shapeshift,
		ForceField,
		Count
	};

	const char* PowerEffectName(PowerEffect Value);
	bool ParsePowerEffect(const std::string& Text, PowerEffect& Out);

	/** Multipliers applied on top of a power's base numbers. */
	struct PowerStats
	{
		double Potency = 1.0;
		double Range = 1.0;
		double Cooldown = 1.0;
		double EnergyCost = 1.0;
		double Duration = 1.0;
	};

	struct PowerDef
	{
		std::string Id;
		std::string Name;
		std::string Archetype;    // inspiration, e.g. "Flying powerhouse"
		std::string Description;
		PowerEffect Effect = PowerEffect::EnergyBlast;
		double BaseCooldownS = 5.0;
		double BaseEnergy = 20.0;
		double BaseRangeM = 20.0;
		double BaseDurationS = 0.0;
		double BasePotency = 1.0;
		std::vector<std::string> Mutations; // mutation ids this power can roll
		std::string EvolvesTo;              // power id it can evolve into after enough mutations
		bool bStarter = true;               // selectable at character creation
	};

	struct MutationDef
	{
		std::string Id;
		std::string Name;
		std::string Description;
		PowerStats Mods;
		double Weight = 1.0;
		bool bDrawback = false;
	};

	/** One power held by the player or a superhuman NPC. */
	struct PowerInstance
	{
		std::string PowerId;
		int Level = 1;
		int Xp = 0;
		double Instability = 0.0;
		std::vector<std::string> Mutations;
	};

	/** Final numbers after level and mutations. */
	struct EffectivePower
	{
		const PowerDef* Def = nullptr;
		double CooldownS = 0.0;
		double EnergyCost = 0.0;
		double RangeM = 0.0;
		double DurationS = 0.0;
		double Potency = 0.0;
	};

	class PowerRoster
	{
	public:
		void AddPower(PowerDef Def);
		void AddMutation(MutationDef Def);
		const PowerDef* FindPower(const std::string& Id) const;
		const MutationDef* FindMutation(const std::string& Id) const;
		const std::vector<PowerDef>& Powers() const { return PowerDefs; }
		const std::vector<MutationDef>& Mutations() const { return MutationDefs; }
		std::vector<const PowerDef*> StarterPowers() const;

		EffectivePower Evaluate(const PowerInstance& Instance) const;

	private:
		std::vector<PowerDef> PowerDefs;
		std::vector<MutationDef> MutationDefs;
	};

	struct MutationEvent
	{
		int Slot = -1;
		std::string PowerBefore;
		std::string PowerAfter;
		std::string MutationId; // empty when the event was an evolution only
		bool bEvolved = false;
	};

	/** The player's three chosen powers, which level up with use and mutate unpredictably. */
	class PowerSet
	{
	public:
		static constexpr int SlotCount = 3;
		static constexpr int MaxLevel = 10;

		/** Picks exactly three distinct starter powers. */
		bool Choose(const PowerRoster& Roster, const std::vector<std::string>& Ids, std::string* OutError);
		bool HasPowers() const { return !SlotList.empty(); }
		const std::vector<PowerInstance>& Slots() const { return SlotList; }
		const PowerInstance* Slot(int Index) const;

		/**
		 * Call each time a power is used. Grants XP (bOutLevelUp reports a level gain) and builds instability;
		 * returns true and fills Out when the power mutated or evolved.
		 */
		bool RecordUse(int SlotIndex, const PowerRoster& Roster, Rng& Random, MutationEvent& Out, bool& bOutLevelUp);
		/** A random world event (radiation leak, meteor shower) mutates one power immediately. */
		bool Surge(const PowerRoster& Roster, Rng& Random, MutationEvent& Out);

		void Restore(std::vector<PowerInstance> InSlots) { SlotList = std::move(InSlots); }

		/** Shared by the player and NPC superhumans. */
		static bool Mutate(PowerInstance& Instance, const PowerRoster& Roster, Rng& Random, MutationEvent& Out);

	private:
		std::vector<PowerInstance> SlotList;
	};

	enum class Alignment : int
	{
		Hero,
		Villain
	};

	/** A generated hero or villain who roams the city. */
	struct Superhuman
	{
		std::string Id;
		std::string Codename;
		Alignment Side = Alignment::Villain;
		int Threat = 1; // 1..5
		std::vector<PowerInstance> Powers;
		double MaxHealth = 300.0;
		Money Bounty = 0;
		std::string Motive;
	};

	Superhuman GenerateSuperhuman(const PowerRoster& Roster, Alignment Side, int Threat, Rng& Random);
}
