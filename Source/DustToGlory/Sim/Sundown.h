// Sundown: the state after the lights go out, and the things that live in the dark.
//
// THE ONE RULE. The Pale cannot bear direct sunlight. Everything else here follows from that. Day
// is not "safe time" handed to the player as a gift -- it is a budget. The sun is up for a fixed
// number of minutes and every one of them is spent deciding how far from shelter you are willing to
// be when it goes down. Dusk is the clock running out, and the game is what you chose to do with
// the hours before it.
//
// That rule is also why the buildings matter. This world already has interiors everywhere, and in
// Sundown every dark one is somewhere the Pale can wait out the day. A warehouse at noon is more
// dangerous than a street at dusk, which inverts the usual instinct to get inside, and turns a
// feature that was scenery into the central risk.
//
// WHAT THIS IS NOT. There is no gore in this file and there is no mechanic that rewards it. Nothing
// here tracks wounds, dismemberment or damage dealt to a body. The pressure comes from the light
// failing, from distance, from sound carrying, and from the knowledge that something has been
// watching which streets you keep going back to. A `Dread` value rises in the dark and near them,
// and the engine layer turns it into sound and shadow. That is the whole horror budget, on purpose.
//
// THE THING THAT HUNTS YOU IS NOT STUPID. Stalkers chase. Shriekers call others. The Alpha keeps
// count: forage the same block three days running and it will be waiting the fourth. That is the
// inspection game out of Sim/GameTheory -- the player picks where to forage, the Alpha picks where
// to watch -- and it is the difference between an obstacle and an opponent.
#pragma once

#include "Sim/CityLayout.h"
#include "Sim/SimCommon.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dtg::sim
{
	class OutbreakSystem;

	enum class PaleKind : int
	{
		Stalker,   // the common one: fast, climbs, runs you down in the open
		Shrieker,  // frail, but its call brings everything within half a mile
		Alpha,     // one per territory. It remembers, and it plans.
		Carrier,   // an infected animal: dogs mostly, and they hunt in daylight shade
		Count
	};
	const char* PaleKindName(PaleKind Kind);

	/** What a scavenging run turned up, and what it cost to stay that long. */
	struct Haul
	{
		int Food = 0;
		int Water = 0;
		int Fuel = 0;
		int Ammo = 0;
		int Medical = 0;
		int Parts = 0;
		/** Minutes the search took. Time is the real price of everything in this mode. */
		int Minutes = 0;
		/** True when something was in there with you. */
		bool bDisturbed = false;
		/** True when it was a nest and the whole pack woke. */
		bool bNest = false;
		std::string Story;

		int Total() const { return Food + Water + Fuel + Ammo + Medical + Parts; }
		bool IsEmpty() const { return Total() == 0; }
	};

	/** Everything the player is carrying or has stockpiled. Cash is inert in Sundown. */
	struct Provisions
	{
		int Food = 0;
		int Water = 0;
		int Fuel = 0;
		int Ammo = 0;
		int Medical = 0;
		int Parts = 0;

		void Add(const Haul& Value);
		int Total() const { return Food + Water + Fuel + Ammo + Medical + Parts; }
		/** A day's eating and drinking for one person. Returns false when there was not enough. */
		bool Consume(int FoodNeeded, int WaterNeeded);
	};

	/** Where the player means to be when the sun goes down. */
	struct Refuge
	{
		int BuildingId = -1;
		/** 0 nothing, 1 every window and door covered. Decays when something leans on it. */
		double Barricade = 0.0;
		/** A generator and the fuel to run it: light through the night, and they will not come in. */
		bool bGenerator = false;
		double GeneratorFuel = 0.0;
		bool IsReal() const { return BuildingId >= 0; }
	};

	/** What a night came to. */
	struct NightReport
	{
		bool bSheltered = false;
		bool bBreached = false;
		int Approaches = 0;       // how many times something came to the door
		double BarricadeLost = 0.0;
		double DreadPeak = 0.0;
		/** Hours spent outside after dark, which is the thing that actually kills people. */
		double HoursExposed = 0.0;
		std::string Story;
	};

	/** A nest: a dark interior the Pale wait out the day in. */
	struct Nest
	{
		int BuildingId = 0;
		int Occupants = 0;
		bool bAlpha = false;
		/** The day it was last cleared; nests repopulate after a while. */
		int ClearedOnDay = -1;
		bool bCleared = false;
	};

	/** How a block has been treated, which is what the Alpha is keeping track of. */
	struct Pattern
	{
		int BlockKey = 0;
		int Visits = 0;
		int LastVisitDay = -1;
		/** How much the Alpha expects the player here, 0..1. */
		double Expectation = 0.0;
	};

	struct SundownRules
	{
		/** Sunrise and sunset as a fraction through the day. Outside these the sun is down. */
		double SunriseFraction = 0.27;   // about 06:30
		double SunsetFraction = 0.81;    // about 19:30
		/** Minutes either side where the sun is too low to stop them. Dusk is not a cliff edge. */
		double TwilightMinutes = 35.0;
		/** How much of the state's population turned. */
		double TurnedShare = 0.93;
		/** Nests per hundred buildings. */
		double NestsPerHundred = 22.0;
		/** A nest repopulates this many days after being cleared. */
		int RepopulateDays = 9;
		/** Side of one "block" for the Alpha's bookkeeping, in metres. */
		double BlockM = 220.0;
		/** How fast the Alpha learns a habit, and how fast it forgets one. */
		double LearnRate = 0.34;
		double ForgetPerDay = 0.11;
		/** Doses of the cure needed before it can be deployed. */
		int DosesForCure = 6;
	};

	/**
	 * The mode. Begin() can be called at any moment in any save -- the player dials SUNDOWN and the
	 * world is simply different from the next minute on -- and End() puts it back.
	 */
	class SundownState
	{
	public:
		bool IsActive() const { return bActive; }
		int Day() const { return DayNumber; }
		const Provisions& Supplies() const { return Held; }
		Provisions& Supplies() { return Held; }
		const Refuge& CurrentRefuge() const { return Home; }
		int Doses() const { return DoseCount; }
		double CureProgress() const { return Cure; }
		bool CureReady() const { return DoseCount >= Rules.DosesForCure; }
		bool CureDeployed() const { return bDeployed; }
		/** 0..1. The engine turns this into darkness, sound and heartbeat. Never into blood. */
		double Dread() const { return DreadValue; }
		const std::vector<Nest>& Nests() const { return NestList; }
		const SundownRules& Settings() const { return Rules; }

		/**
		 * Switches the mode on. Seeds nests across the world's buildings and sets the population
		 * from the outbreak if one is running, so dialling it after engineering a pandemic starts
		 * from the infection that was actually loose.
		 */
		void Begin(const CityPlan& World, const OutbreakSystem* Outbreak, int OnDay, Rng& Random);
		void End();

		// ------------------------------------------------------------------ light
		//
		// The whole mode hangs off these three.

		/**
		 * How strong the sun has to be before open ground is safe. One constant, used by every
		 * question about the light, because the first version had the countdown measuring against
		 * one threshold and the danger against another: at seven in the evening the watch said the
		 * light had run out nine minutes ago while the rules still treated it as full daylight.
		 */
		static constexpr double SafeSun = 0.55;

		/** Sun strength at a moment, 0 (full dark) to 1 (high sun). */
		double SunAt(std::int64_t Minute) const;
		/** True when the sun is high enough that the open ground is safe. */
		bool IsDaylight(std::int64_t Minute) const { return SunAt(Minute) >= SafeSun; }
		/** There is light in the sky, but not necessarily enough of it to stand out in. */
		bool AnyLight(std::int64_t Minute) const { return SunAt(Minute) > 0.02; }
		/**
		 * Minutes of safe light left, or 0 when there is none. Walks the same SunAt curve against
		 * the same threshold the danger uses, so the number on the player's watch cannot contradict
		 * what the world is doing.
		 */
		double MinutesOfLightLeft(std::int64_t Minute) const;
		/**
		 * Minutes until open ground becomes safe again, or 0 when it already is. The other half of
		 * the same question, kept separate because one number trying to mean both "you have two
		 * hours" and "you must wait two hours" is a number nobody can read in a hurry.
		 */
		double MinutesUntilSafe(std::int64_t Minute) const;

		/**
		 * How exposed the player is right now: 0 safe, 1 being hunted. Daylight outdoors is 0. A
		 * dark interior is never 0, whatever the hour, because that is where they sleep.
		 */
		double Exposure(std::int64_t Minute, bool bIndoors, bool bLit, double MetresFromShelter) const;

		// ------------------------------------------------------------------ the day
		/** Searches a building. Depletes it, and may wake whatever is inside. */
		Haul Scavenge(const CityPlan& World, int BuildingId, std::int64_t Minute, Rng& Random);
		/** How picked over a building already is, 0 untouched to 1 stripped. */
		double Picked(int BuildingId) const;
		/** True when that building is a nest the player has not cleared. */
		bool IsNest(int BuildingId) const;
		/** Clears a nest. Costs ammo, and the Alpha notices. */
		bool ClearNest(int BuildingId, int AmmoSpent, Rng& Random);

		/** Makes a building the night's shelter and spends Parts on boarding it up. */
		bool Fortify(int BuildingId, int PartsSpent, bool bGenerator, double FuelPoured);

		// ------------------------------------------------------------------ the night
		/**
		 * Runs one night. HoursOutside is how long the player stayed out after dark -- the single
		 * biggest thing that decides whether they see morning.
		 */
		NightReport Night(double HoursOutside, Rng& Random);

		/** Ends the day: eat, drink, let the Alpha forget a little, repopulate cleared nests. */
		void NewDay(Rng& Random);

		// ------------------------------------------------------------------ the long game
		/** Works on the cure. Needs medical supplies and somewhere to work. */
		bool Research(int MedicalSpent, double Hours, Rng& Random);
		/** Deploys it. This is the ending. */
		bool Deploy(OutbreakSystem* Outbreak);

		// ------------------------------------------------------------------ the Alpha
		/** What the Alpha currently expects of a point, 0..1. */
		double Expectation(double X, double Y) const;
		/** Records that the player worked a place, which is how the Alpha learns. */
		void Noticed(double X, double Y, int OnDay);
		int BlockKeyFor(double X, double Y) const;

		struct Snapshot
		{
			bool bActive = false;
			int DayNumber = 0;
			Provisions Held;
			Refuge Home;
			double Cure = 0.0;
			int DoseCount = 0;
			bool bDeployed = false;
			double DreadValue = 0.0;
			double Population = 0.0;
			std::vector<Nest> NestList;
			std::vector<Pattern> Patterns;
			std::vector<std::pair<int, double>> PickedOver;
		};
		Snapshot Save() const;
		void Load(const Snapshot& Data);

	private:
		SundownRules Rules;
		bool bActive = false;
		int DayNumber = 0;
		Provisions Held;
		Refuge Home;
		double Cure = 0.0;
		int DoseCount = 0;
		bool bDeployed = false;
		double DreadValue = 0.0;
		/** How many of them are out there. Falls as nests are cleared, rises if left alone. */
		double Population = 0.0;
		std::vector<Nest> NestList;
		std::vector<Pattern> Patterns;
		std::vector<std::pair<int, double>> PickedOver;

		Nest* FindNest(int BuildingId);
		const Nest* FindNest(int BuildingId) const;
		Pattern* FindPattern(int BlockKey);
		double& PickedRef(int BuildingId);
	};
}
