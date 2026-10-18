// Fights.
//
// Powers could already shove cars around, but nothing could be hurt by them. This is the other half: the
// player and whoever they are fighting have health, hits land for an amount that depends on skill, powers
// and what you hit them with, and someone eventually goes down. Beating a villain pays their bounty;
// hitting a bystander is assault, and the police are told. Losing means waking up at the hospital.
//
// None of it is a combo system -- it is the bookkeeping a brawl needs, with the drama left to the engine.
#pragma once

#include "Sim/Careers.h"
#include "Sim/SimCommon.h"
#include "Sim/Wanted.h"

#include <cstddef>
#include <string>
#include <vector>

namespace dtg::sim
{
	/** Why a hit landed the way it did, which is also what the engine shows. */
	enum class StrikeKind : int
	{
		Fist,
		Power,     // whichever power is in hand
		Vehicle,   // run down, deliberately or otherwise
		Count
	};
	const char* StrikeKindName(StrikeKind Kind);

	/** Anyone who can be hurt: a bystander, a hero, a villain, the player. */
	struct Fighter
	{
		std::string Id;
		std::string Name;
		double Health = 100.0;
		double MaxHealth = 100.0;
		double Skill = 0.3;        // 0..1: how hard they hit and how well they take it
		bool bSuperhuman = false;
		bool bHostile = false;     // already fighting the player
		Money Bounty = 0;          // paid for putting a wanted villain down
		int Threat = 0;            // 1..5 for superhumans, 0 for everyone else

		bool IsDown() const { return Health <= 0.0; }
	};

	/** What one exchange did. */
	struct StrikeResult
	{
		bool bLanded = false;
		double Damage = 0.0;
		bool bKnockedOut = false;
		bool bWasInnocent = false;  // a bystander, which makes it assault
		Money Bounty = 0;
		std::string Summary;
	};

	class CombatSystem
	{
	public:
		static constexpr double PlayerMaxHealth = 100.0;

		double PlayerHealth() const { return Health; }
		bool PlayerIsDown() const { return Health <= 0.0; }
		const std::vector<Fighter>& Fighters() const { return Active; }
		const Fighter* Find(const std::string& Id) const;
		/** Starts tracking someone; returns the entry so the caller can adjust it. */
		Fighter& Engage(Fighter Who);
		void Disengage(const std::string& Id);
		void Clear() { Active.clear(); }

		/** How much damage the player does with this kind of hit. */
		static double PlayerDamage(StrikeKind Kind, int FitnessLevel, double PowerPotency);
		/** The player hits someone. */
		StrikeResult Strike(const std::string& Id, StrikeKind Kind, int FitnessLevel, double PowerPotency, Rng& Random);
		/** Someone hits the player back; returns true if the player goes down. */
		bool Hurt(double Amount);
		/** Everyone hostile takes their turn. Returns the damage the player took. */
		double OpponentsAct(int FitnessLevel, Rng& Random, std::vector<std::string>& OutMessages);

		/** Health comes back slowly with rest, faster for the fit. */
		void Recover(int Minutes, int FitnessLevel);
		void ReviveAtHospital() { Health = PlayerMaxHealth; }

		struct Snapshot
		{
			double Health = PlayerMaxHealth;
			std::vector<Fighter> Active;
		};
		Snapshot Save() const { return Snapshot{Health, Active}; }
		void Load(const Snapshot& Data)
		{
			Health = Data.Health;
			Active = Data.Active;
		}

	private:
		double Health = PlayerMaxHealth;
		std::vector<Fighter> Active;
	};
}
