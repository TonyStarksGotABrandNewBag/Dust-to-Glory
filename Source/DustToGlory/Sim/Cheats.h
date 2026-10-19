// Cheat codes, dialled on the player's phone.
//
// There is no console in this game and no debug menu, so cheats go in the way the player already
// talks to the world: they dial a number. Every code is also a word, and the number IS the word on
// a phone keypad -- PAYDAY is 1-999-729329 -- so either one works and neither has to be memorised
// separately. That mapping is computed, not typed out, so the two can never drift apart.
//
// Two rules the rest of the game depends on:
//
//   - A cheat is never silent. Entering one sends a text back, so there is no state the player
//     cannot see.
//   - bEverUsed sticks to the save forever. Not to punish anyone -- it is their game -- but so a
//     save where a quarter of a million dollars was dialled in does not sit indistinguishably
//     beside one where it was earned. Switching a cheat back off does not clear it.
#pragma once

#include "Sim/SimCommon.h"

#include <cstddef>
#include <string>
#include <vector>

namespace dtg::sim
{
	enum class Cheat : int
	{
		// Money and progress
		Payday,         // a quarter million, which in a 1990s economy is a lot
		BlueChip,       // tomorrow's market moves, today
		GroundFloor,    // any business at book value
		Landslide,      // skip to the next office

		// Vehicles
		KeysToTheCity,  // every make and model, every dealer
		Showroom,       // current car perfect again
		Titanium,       // the car you are driving takes no damage
		StickyTires,    // grip multiplier, for when the physics is winning
		BarnFind,       // a random classic, rough, yours

		// Heat and law
		ColdCase,       // wanted level and open charges gone
		MostWanted,     // five stars, right now, because sometimes you want the chase
		TimeServed,     // out of jail
		CleanSlate,     // record expunged

		// Powers
		AllThree,       // all three slots unlocked
		FullCharge,     // energy back, cooldowns cleared
		Reroll,         // re-roll the mutations

		// World
		HighNoon,       // hold the clock where it is
		FourSeasons,    // roll the weather on
		FeatherFall,    // low gravity
		HalfSpeed,      // slow motion
		GhostTown,      // no traffic, no pedestrians
		RushHour,       // three times the traffic

		// And the big one
		Sundown,        // see Sim/Sundown.h

		Count
	};

	const char* CheatName(Cheat Which);

	struct CheatDef
	{
		Cheat Which = Cheat::Payday;
		const char* Code = "";     // what you type, or spell out on the keypad
		const char* Title = "";    // what the confirmation text calls it
		const char* Effect = "";   // one line, shown in the text and in the cheat list
		/** True when entering it again turns it off; false when it fires once and is done. */
		bool bToggle = false;
	};

	/** Every cheat, in the order above. */
	const std::vector<CheatDef>& AllCheats();
	const CheatDef* FindCheatDef(Cheat Which);

	/**
	 * The dialled form of a code: its letters on a phone keypad, with the 1-999 prefix. PAYDAY
	 * becomes "1-999-729329". Computed rather than stored, so a renamed cheat cannot keep an old
	 * number.
	 */
	std::string DialFor(const std::string& Code);

	/**
	 * Looks up a cheat by either form. Spaces, dashes, dots and case are ignored, so "cold case",
	 * "COLDCASE" and "1-999-265-32273" all land on the same cheat. Null when nothing matches.
	 */
	const CheatDef* FindCheat(const std::string& Typed);

	/** Which cheats are on, and whether any was ever entered. */
	class CheatBook
	{
	public:
		bool IsOn(Cheat Which) const;
		/** Turns a toggle on or off; returns the state it ended up in. Marks the save either way. */
		bool Toggle(Cheat Which);
		/** Records a one-shot cheat firing. Marks the save. */
		void Fired(Cheat Which);
		void TurnOff(Cheat Which);

		/** True once any cheat has been entered in this save, and true forever after. */
		bool EverUsed() const { return bEverUsed; }
		/** How many cheats have been entered, counting repeats. */
		int TimesEntered() const { return Entered; }
		/** Which toggles are currently on. */
		std::vector<Cheat> Active() const;

		struct Snapshot
		{
			bool bEverUsed = false;
			int Entered = 0;
			std::vector<int> On;
		};
		Snapshot Save() const;
		void Load(const Snapshot& Data);

	private:
		std::vector<bool> OnFlags = std::vector<bool>(static_cast<std::size_t>(Cheat::Count), false);
		bool bEverUsed = false;
		int Entered = 0;
	};
}
