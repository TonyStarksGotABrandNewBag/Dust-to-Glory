#include "Sim/Cheats.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace dtg::sim
{
	namespace detail_cheats
	{
		/** A letter's digit on a telephone keypad. Digits pass through; everything else is dropped. */
		char Keypad(char Letter)
		{
			const char Upper = (Letter >= 'a' && Letter <= 'z')
				? static_cast<char>(Letter - 'a' + 'A')
				: Letter;
			switch (Upper)
			{
			case 'A': case 'B': case 'C': return '2';
			case 'D': case 'E': case 'F': return '3';
			case 'G': case 'H': case 'I': return '4';
			case 'J': case 'K': case 'L': return '5';
			case 'M': case 'N': case 'O': return '6';
			case 'P': case 'Q': case 'R': case 'S': return '7';
			case 'T': case 'U': case 'V': return '8';
			case 'W': case 'X': case 'Y': case 'Z': return '9';
			default: break;
			}
			if (Upper >= '0' && Upper <= '9')
			{
				return Upper;
			}
			return '\0';
		}

		/** Strips everything a person might type between digits or letters, and upper-cases. */
		std::string Tidy(const std::string& Text)
		{
			std::string Out;
			Out.reserve(Text.size());
			for (const char Letter : Text)
			{
				if (Letter == ' ' || Letter == '-' || Letter == '.' || Letter == '(' || Letter == ')' ||
					Letter == '+' || Letter == '_')
				{
					continue;
				}
				Out.push_back((Letter >= 'a' && Letter <= 'z')
					? static_cast<char>(Letter - 'a' + 'A')
					: Letter);
			}
			return Out;
		}

		/** The digits of a code, with no prefix. */
		std::string Digits(const std::string& Code)
		{
			const std::string Clean = Tidy(Code);
			std::string Out;
			Out.reserve(Clean.size());
			for (const char Letter : Clean)
			{
				const char Digit = Keypad(Letter);
				if (Digit != '\0')
				{
					Out.push_back(Digit);
				}
			}
			return Out;
		}

		const std::vector<CheatDef>& Table()
		{
			// Order matches the enum, and AllCheats() is checked against that in the tests.
			static const std::vector<CheatDef> Cheats = {
				{Cheat::Payday, "PAYDAY", "Payday",
					"A quarter of a million dollars, which in this economy buys a street.", false},
				{Cheat::BlueChip, "BLUECHIP", "Blue Chip",
					"Tomorrow's market moves, today.", true},
				{Cheat::GroundFloor, "GROUNDFLOOR", "Ground Floor",
					"Any business sells at book value, however much they like it.", true},
				{Cheat::Landslide, "LANDSLIDE", "Landslide",
					"Straight to the next office on the ladder.", false},

				{Cheat::KeysToTheCity, "KEYSTOTHECITY", "Keys to the City",
					"Every make and model, on every lot, right now.", true},
				{Cheat::Showroom, "SHOWROOM", "Showroom",
					"The car you are in is perfect again: condition, fuel, every fault gone.", false},
				{Cheat::Titanium, "TITANIUM", "Titanium",
					"The car you are driving stops taking damage.", true},
				{Cheat::StickyTires, "STICKYTIRES", "Sticky Tires",
					"Far more grip than any tyre has, for when the physics is winning.", true},
				{Cheat::BarnFind, "BARNFIND", "Barn Find",
					"A classic turns up. Rough, complete, yours.", false},

				{Cheat::ColdCase, "COLDCASE", "Cold Case",
					"Wanted level and open charges, gone.", false},
				{Cheat::MostWanted, "MOSTWANTED", "Most Wanted",
					"Five stars, immediately, because sometimes the chase is the point.", false},
				{Cheat::TimeServed, "TIMESERVED", "Time Served",
					"Out of jail. The conviction stays on the record.", false},
				{Cheat::CleanSlate, "CLEANSLATE", "Clean Slate",
					"The whole record sealed, with no waiting and no fee.", false},

				{Cheat::AllThree, "ALLTHREE", "All Three",
					"All three power slots, filled.", false},
				{Cheat::FullCharge, "FULLCHARGE", "Full Charge",
					"Energy back and every cooldown cleared.", false},
				{Cheat::Reroll, "REROLL", "Reroll",
					"Re-rolls the mutations on the powers you have.", false},

				{Cheat::HighNoon, "HIGHNOON", "High Noon",
					"Holds the clock where it stands. The sun stops moving.", true},
				{Cheat::FourSeasons, "FOURSEASONS", "Four Seasons",
					"Rolls the weather on to the next kind.", false},
				{Cheat::FeatherFall, "FEATHERFALL", "Feather Fall",
					"Gravity at a fifth. Cars included.", true},
				{Cheat::HalfSpeed, "HALFSPEED", "Half Speed",
					"Everything at half pace.", true},
				{Cheat::GhostTown, "GHOSTTOWN", "Ghost Town",
					"No traffic, nobody on the pavements. Just the state.", true},
				{Cheat::RushHour, "RUSHHOUR", "Rush Hour",
					"Three times the traffic, all of it in a hurry.", true},

				{Cheat::Sundown, "SUNDOWN", "Sundown",
					"The lights go out. See what is left.", true},
			};
			return Cheats;
		}
	}

	const char* CheatName(Cheat Which)
	{
		const CheatDef* Def = FindCheatDef(Which);
		return Def ? Def->Title : "cheat";
	}

	const std::vector<CheatDef>& AllCheats()
	{
		return detail_cheats::Table();
	}

	const CheatDef* FindCheatDef(Cheat Which)
	{
		for (const CheatDef& Def : detail_cheats::Table())
		{
			if (Def.Which == Which)
			{
				return &Def;
			}
		}
		return nullptr;
	}

	std::string DialFor(const std::string& Code)
	{
		const std::string Numbers = detail_cheats::Digits(Code);
		if (Numbers.empty())
		{
			return std::string();
		}
		return "1-999-" + Numbers;
	}

	const CheatDef* FindCheat(const std::string& Typed)
	{
		const std::string Clean = detail_cheats::Tidy(Typed);
		if (Clean.empty())
		{
			return nullptr;
		}
		// Dialled: optionally the 1999 prefix, then the digits of the code.
		std::string AsDigits = Clean;
		if (AsDigits.rfind("1999", 0) == 0)
		{
			AsDigits = AsDigits.substr(4);
		}
		const bool bAllDigits = std::all_of(AsDigits.begin(), AsDigits.end(),
			[](char Letter) { return Letter >= '0' && Letter <= '9'; });

		for (const CheatDef& Def : detail_cheats::Table())
		{
			if (Clean == detail_cheats::Tidy(Def.Code))
			{
				return &Def;
			}
			if (bAllDigits && !AsDigits.empty() && AsDigits == detail_cheats::Digits(Def.Code))
			{
				return &Def;
			}
		}
		return nullptr;
	}

	bool CheatBook::IsOn(Cheat Which) const
	{
		const std::size_t Index = static_cast<std::size_t>(Which);
		return Index < OnFlags.size() && OnFlags[Index];
	}

	bool CheatBook::Toggle(Cheat Which)
	{
		const std::size_t Index = static_cast<std::size_t>(Which);
		if (Index >= OnFlags.size())
		{
			return false;
		}
		OnFlags[Index] = !OnFlags[Index];
		bEverUsed = true;
		++Entered;
		return OnFlags[Index];
	}

	void CheatBook::Fired(Cheat Which)
	{
		(void)Which;
		bEverUsed = true;
		++Entered;
	}

	void CheatBook::TurnOff(Cheat Which)
	{
		const std::size_t Index = static_cast<std::size_t>(Which);
		if (Index < OnFlags.size())
		{
			OnFlags[Index] = false;
		}
	}

	std::vector<Cheat> CheatBook::Active() const
	{
		std::vector<Cheat> Out;
		for (std::size_t Index = 0; Index < OnFlags.size(); ++Index)
		{
			if (OnFlags[Index])
			{
				Out.push_back(static_cast<Cheat>(Index));
			}
		}
		return Out;
	}

	CheatBook::Snapshot CheatBook::Save() const
	{
		Snapshot Data;
		Data.bEverUsed = bEverUsed;
		Data.Entered = Entered;
		for (std::size_t Index = 0; Index < OnFlags.size(); ++Index)
		{
			if (OnFlags[Index])
			{
				Data.On.push_back(static_cast<int>(Index));
			}
		}
		return Data;
	}

	void CheatBook::Load(const Snapshot& Data)
	{
		OnFlags.assign(static_cast<std::size_t>(Cheat::Count), false);
		for (const int Index : Data.On)
		{
			if (Index >= 0 && static_cast<std::size_t>(Index) < OnFlags.size())
			{
				OnFlags[static_cast<std::size_t>(Index)] = true;
			}
		}
		bEverUsed = Data.bEverUsed;
		Entered = Data.Entered;
	}
}
