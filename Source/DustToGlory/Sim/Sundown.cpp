#include "Sim/Sundown.h"

#include "Sim/Outbreak.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dtg::sim
{
	namespace detail_sundown
	{
		constexpr double MinutesPerDay = 24.0 * 60.0;

		double Smooth(double T)
		{
			const double X = Clamp(T, 0.0, 1.0);
			return X * X * (3.0 - 2.0 * X);
		}

		/** Where in the day a minute falls, 0 at midnight. */
		double DayFraction(std::int64_t Minute)
		{
			const double Within = static_cast<double>(Minute % static_cast<std::int64_t>(MinutesPerDay));
			return (Within < 0.0 ? Within + MinutesPerDay : Within) / MinutesPerDay;
		}
	}

	const char* PaleKindName(PaleKind Kind)
	{
		switch (Kind)
		{
		case PaleKind::Stalker: return "stalker";
		case PaleKind::Shrieker: return "shrieker";
		case PaleKind::Alpha: return "alpha";
		case PaleKind::Carrier: return "carrier";
		case PaleKind::Count: break;
		}
		return "pale";
	}

	void Provisions::Add(const Haul& Value)
	{
		Food += Value.Food;
		Water += Value.Water;
		Fuel += Value.Fuel;
		Ammo += Value.Ammo;
		Medical += Value.Medical;
		Parts += Value.Parts;
	}

	bool Provisions::Consume(int FoodNeeded, int WaterNeeded)
	{
		if (Food < FoodNeeded || Water < WaterNeeded)
		{
			Food = std::max(0, Food - FoodNeeded);
			Water = std::max(0, Water - WaterNeeded);
			return false;
		}
		Food -= FoodNeeded;
		Water -= WaterNeeded;
		return true;
	}

	// ---------------------------------------------------------------------- beginning and ending

	void SundownState::Begin(const CityPlan& World, const OutbreakSystem* Outbreak, int OnDay, Rng& Random)
	{
		bActive = true;
		DayNumber = OnDay;
		Cure = 0.0;
		DoseCount = 0;
		bDeployed = false;
		DreadValue = 0.0;
		Home = Refuge();
		NestList.clear();
		Patterns.clear();
		PickedOver.clear();

		// If an outbreak was already running -- which it will be if the player engineered one and it
		// got away from them -- the population that turned is the population that was infected.
		// Dialling the code cold instead starts from the whole state.
		const double Base = static_cast<double>(OutbreakSystem::Population);
		Population = (Outbreak != nullptr && Outbreak->IsActive())
			? std::max(Base * 0.25, Outbreak->Infected() + Outbreak->Susceptible() * 0.6)
			: Base * Rules.TurnedShare;

		// Nests, spread over the world's buildings. A nest is somewhere dark and enclosed, so the
		// bigger the footprint the likelier it is one.
		const std::size_t Wanted = static_cast<std::size_t>(
			static_cast<double>(World.Buildings.size()) * Rules.NestsPerHundred / 100.0);
		if (World.Buildings.empty() || Wanted == 0)
		{
			return;
		}
		NestList.reserve(Wanted);
		// Walk the buildings on a stride so nests are spread across the whole state rather than
		// clumped wherever the random draws happened to land early.
		const std::size_t Stride = std::max<std::size_t>(1, World.Buildings.size() / Wanted);
		bool bAlphaPlaced = false;
		for (std::size_t Index = 0; Index < World.Buildings.size() && NestList.size() < Wanted;
			Index += Stride)
		{
			const BuildingPlan& Building = World.Buildings[Index];
			Nest Made;
			Made.BuildingId = Building.Id;
			const double Size = Building.Footprint.Width() * Building.Footprint.Depth();
			const double Scale = Clamp(Size / 900.0, 0.35, 3.0);
			Made.Occupants = 2 + Random.RangeInt(0, static_cast<int>(6.0 * Scale));
			// One Alpha, in the biggest nest found reasonably early. There is only ever one, because
			// the point of it is that it is a specific thing that knows who you are.
			if (!bAlphaPlaced && Made.Occupants >= 6)
			{
				Made.bAlpha = true;
				bAlphaPlaced = true;
			}
			NestList.push_back(Made);
		}
		if (!bAlphaPlaced && !NestList.empty())
		{
			NestList.front().bAlpha = true;
		}
	}

	void SundownState::End()
	{
		bActive = false;
		DreadValue = 0.0;
	}

	// ---------------------------------------------------------------------- light

	double SundownState::SunAt(std::int64_t Minute) const
	{
		using namespace detail_sundown;
		const double Fraction = DayFraction(Minute);
		const double Edge = Rules.TwilightMinutes / MinutesPerDay;
		const double First = Rules.SunriseFraction - Edge;
		const double Last = Rules.SunsetFraction + Edge;
		if (Fraction <= First || Fraction >= Last)
		{
			return 0.0;
		}
		// One arc from first light to last, which is roughly what the sun's elevation does and --
		// more importantly -- is continuous. The first version of this had separate formulas for
		// the twilight ramps and the middle of the day, and they did not meet: the sun jumped from
		// 1.0 down to 0.51 just after sunrise and spent the afternoon dipping below the threshold
		// that decides whether open ground is safe. Rendering a dusk is what showed it up.
		const double T = (Fraction - First) / std::max(1e-9, Last - First);
		return std::sin(T * 3.14159265358979);
	}

	double SundownState::MinutesOfLightLeft(std::int64_t Minute) const
	{
		using namespace detail_sundown;
		const int Limit = static_cast<int>(MinutesPerDay);
		if (SunAt(Minute) < SafeSun)
		{
			return 0.0;
		}
		for (int Ahead = 1; Ahead <= Limit; ++Ahead)
		{
			if (SunAt(Minute + Ahead) < SafeSun)
			{
				return static_cast<double>(Ahead);
			}
		}
		return static_cast<double>(Limit);
	}

	double SundownState::MinutesUntilSafe(std::int64_t Minute) const
	{
		using namespace detail_sundown;
		const int Limit = static_cast<int>(MinutesPerDay);
		if (SunAt(Minute) >= SafeSun)
		{
			return 0.0;
		}
		for (int Ahead = 1; Ahead <= Limit; ++Ahead)
		{
			if (SunAt(Minute + Ahead) >= SafeSun)
			{
				return static_cast<double>(Ahead);
			}
		}
		return static_cast<double>(Limit);
	}

	double SundownState::Exposure(std::int64_t Minute, bool bIndoors, bool bLit,
		double MetresFromShelter) const
	{
		if (!bActive)
		{
			return 0.0;
		}
		const double Sun = SunAt(Minute);

		if (bIndoors)
		{
			// Inside, the sun is no help at all unless the place is lit. This is the inversion the
			// mode turns on: at noon the safest place to stand is the middle of a field.
			if (bLit)
			{
				return 0.05;
			}
			return 0.55 + 0.45 * (1.0 - Sun) * 0.5;
		}

		// Outdoors. Full sun is safety; everything after that is a slope, not a step.
		if (Sun >= SafeSun)
		{
			return 0.0;
		}
		const double Dark = 1.0 - (Sun / SafeSun);
		// Distance from shelter only matters once the light is going.
		const double Far = Clamp(MetresFromShelter / 1200.0, 0.0, 1.0);
		return Clamp(Dark * (0.55 + 0.45 * Far), 0.0, 1.0);
	}

	// ---------------------------------------------------------------------- scavenging

	double& SundownState::PickedRef(int BuildingId)
	{
		for (std::pair<int, double>& Entry : PickedOver)
		{
			if (Entry.first == BuildingId)
			{
				return Entry.second;
			}
		}
		PickedOver.emplace_back(BuildingId, 0.0);
		return PickedOver.back().second;
	}

	double SundownState::Picked(int BuildingId) const
	{
		for (const std::pair<int, double>& Entry : PickedOver)
		{
			if (Entry.first == BuildingId)
			{
				return Entry.second;
			}
		}
		return 0.0;
	}

	Nest* SundownState::FindNest(int BuildingId)
	{
		for (Nest& Value : NestList)
		{
			if (Value.BuildingId == BuildingId)
			{
				return &Value;
			}
		}
		return nullptr;
	}

	const Nest* SundownState::FindNest(int BuildingId) const
	{
		for (const Nest& Value : NestList)
		{
			if (Value.BuildingId == BuildingId)
			{
				return &Value;
			}
		}
		return nullptr;
	}

	bool SundownState::IsNest(int BuildingId) const
	{
		const Nest* Found = FindNest(BuildingId);
		return Found != nullptr && !Found->bCleared && Found->Occupants > 0;
	}

	Haul SundownState::Scavenge(const CityPlan& World, int BuildingId, std::int64_t Minute, Rng& Random)
	{
		Haul Out;
		if (!bActive)
		{
			return Out;
		}
		const BuildingPlan* Building = World.FindBuilding(BuildingId);
		if (Building == nullptr)
		{
			Out.Story = "There is nothing there.";
			return Out;
		}

		double& Stripped = PickedRef(BuildingId);
		const double Left = Clamp(1.0 - Stripped, 0.0, 1.0);
		Out.Minutes = 20 + Random.RangeInt(0, 40);

		// What a place holds depends on what it was. A diner has food; a hardware store has the
		// parts you board a window with; a hospital has the only medicine left in the state.
		int FoodWeight = 1;
		int WaterWeight = 1;
		int FuelWeight = 0;
		int AmmoWeight = 0;
		int MedicalWeight = 0;
		int PartsWeight = 1;
		switch (Building->Role)
		{
		case LotRole::Diner:
		case LotRole::Shop:
			FoodWeight = 6;
			WaterWeight = 5;
			break;
		case LotRole::Hospital:
			MedicalWeight = 7;
			WaterWeight = 2;
			break;
		case LotRole::PoliceStation:
			AmmoWeight = 7;
			PartsWeight = 2;
			break;
		case LotRole::ModShop:
		case LotRole::GardenCenter:
		case LotRole::FurnitureStore:
			PartsWeight = 7;
			FuelWeight = 2;
			break;
		case LotRole::Dealership:
		case LotRole::UsedCarLot:
		case LotRole::ClassicDealer:
			FuelWeight = 7;
			PartsWeight = 4;
			break;
		case LotRole::Home:
		case LotRole::Apartment:
			FoodWeight = 3;
			WaterWeight = 3;
			MedicalWeight = 1;
			break;
		default:
			break;
		}

		auto Roll = [&](int Weight)
		{
			if (Weight <= 0)
			{
				return 0;
			}
			const double Amount = static_cast<double>(Weight) * Left * (0.4 + 0.8 * Random.NextDouble());
			return static_cast<int>(Amount);
		};
		Out.Food = Roll(FoodWeight);
		Out.Water = Roll(WaterWeight);
		Out.Fuel = Roll(FuelWeight);
		Out.Ammo = Roll(AmmoWeight);
		Out.Medical = Roll(MedicalWeight);
		Out.Parts = Roll(PartsWeight);

		// Searching it strips it. Coming back tomorrow is worth much less than finding somewhere new,
		// which is exactly the pressure that pushes the player into unfamiliar streets.
		Stripped = Clamp(Stripped + 0.34 + 0.2 * Random.NextDouble(), 0.0, 1.0);

		// And what was already in there. A nest is a nest at any hour -- that is the point of them.
		if (IsNest(BuildingId))
		{
			Out.bNest = true;
			Out.bDisturbed = true;
			const Nest* Found = FindNest(BuildingId);
			Out.Story = Found != nullptr && Found->bAlpha
				? "Something much larger than the others was in the dark at the back, and it did not rush you."
				: "They were sleeping in the dark at the back, and now they are not.";
		}
		else
		{
			// Even somewhere empty can have one of them sheltering, and the darker the hour the
			// likelier that is.
			const double Night = 1.0 - SunAt(Minute);
			const double Chance = 0.06 + 0.34 * Night;
			if (Random.NextDouble() < Chance)
			{
				Out.bDisturbed = true;
				Out.Story = "Something moved two rooms away and then stopped moving.";
			}
			else
			{
				Out.Story = Out.IsEmpty()
					? "Picked clean long before you got here."
					: "Quiet. You took what was worth carrying.";
			}
		}

		// The Alpha keeps count of where you work.
		Noticed(Building->DoorX, Building->DoorY, DayNumber);
		DreadValue = Clamp(DreadValue + (Out.bDisturbed ? 0.25 : 0.03), 0.0, 1.0);
		return Out;
	}

	bool SundownState::ClearNest(int BuildingId, int AmmoSpent, Rng& Random)
	{
		Nest* Found = FindNest(BuildingId);
		if (!bActive || Found == nullptr || Found->bCleared)
		{
			return false;
		}
		// Roughly two rounds each, and luck for the rest.
		const double Needed = static_cast<double>(Found->Occupants) * 2.0;
		const double Brought = static_cast<double>(AmmoSpent);
		if (Brought < Needed * 0.6)
		{
			return false;
		}
		const double Odds = Clamp(Brought / std::max(1.0, Needed), 0.0, 1.6) - 0.25 * Random.NextDouble();
		if (Odds < 0.75)
		{
			return false;
		}
		Found->bCleared = true;
		Found->ClearedOnDay = DayNumber;
		Population = std::max(0.0, Population - static_cast<double>(Found->Occupants));
		Found->Occupants = 0;
		// Clearing a nest is loud and it is permanent, and the Alpha treats that as a message.
		if (!Found->bAlpha)
		{
			for (Pattern& Value : Patterns)
			{
				Value.Expectation = Clamp(Value.Expectation + 0.08, 0.0, 1.0);
			}
		}
		return true;
	}

	// ---------------------------------------------------------------------- shelter and night

	bool SundownState::Fortify(int BuildingId, int PartsSpent, bool bGenerator, double FuelPoured)
	{
		if (!bActive || BuildingId < 0 || PartsSpent < 0)
		{
			return false;
		}
		if (Held.Parts < PartsSpent)
		{
			return false;
		}
		Held.Parts -= PartsSpent;
		if (Home.BuildingId != BuildingId)
		{
			Home = Refuge();
			Home.BuildingId = BuildingId;
		}
		// Twelve good pieces of timber is a boarded-up house. After that there is nothing left to
		// cover.
		Home.Barricade = Clamp(Home.Barricade + static_cast<double>(PartsSpent) / 12.0, 0.0, 1.0);
		if (bGenerator)
		{
			Home.bGenerator = true;
			const double Fuel = std::min(FuelPoured, static_cast<double>(Held.Fuel));
			Home.GeneratorFuel += Fuel;
			Held.Fuel -= static_cast<int>(Fuel);
		}
		return true;
	}

	NightReport SundownState::Night(double HoursOutside, Rng& Random)
	{
		NightReport Out;
		if (!bActive)
		{
			return Out;
		}
		Out.HoursExposed = std::max(0.0, HoursOutside);
		Out.bSheltered = Home.IsReal();

		// A generator with fuel in it is light, and light is the whole defence. It burns through the
		// night at roughly a unit an hour.
		const bool bLit = Home.bGenerator && Home.GeneratorFuel > 0.0;
		if (bLit)
		{
			Home.GeneratorFuel = std::max(0.0, Home.GeneratorFuel - 11.0);
		}

		// How much attention the player has. Being outside after dark is the dominant term by a
		// long way -- an hour out there is worth more than any amount of boarding.
		const double Attention = Clamp(
			0.12 + 0.42 * Out.HoursExposed + 0.3 * (Home.IsReal() ? 0.0 : 1.0), 0.0, 1.0);
		const double Density = Clamp(Population / static_cast<double>(OutbreakSystem::Population), 0.0, 1.0);
		const double Pressure = Attention * (0.55 + 0.45 * Density);

		Out.Approaches = static_cast<int>(Pressure * 7.0 + Random.NextDouble() * 2.0);
		if (bLit)
		{
			Out.Approaches = Out.Approaches / 3;
		}

		double Barricade = Home.Barricade;
		for (int Attempt = 0; Attempt < Out.Approaches; ++Attempt)
		{
			const double Push = 0.06 + 0.13 * Random.NextDouble();
			Barricade -= Push;
			Out.BarricadeLost += Push;
			if (Barricade <= 0.0 && !bLit)
			{
				Out.bBreached = true;
				break;
			}
		}
		Home.Barricade = Clamp(Barricade, 0.0, 1.0);

		// Standing in the open all night with nowhere to go is its own answer.
		if (!Home.IsReal() && Out.HoursExposed > 1.0)
		{
			Out.bBreached = true;
		}

		Out.DreadPeak = Clamp(0.3 + 0.7 * Pressure, 0.0, 1.0);
		DreadValue = Out.DreadPeak;

		if (Out.bBreached)
		{
			Out.Story = Out.bSheltered
				? "The boards came off the back window somewhere around three in the morning."
				: "There was nowhere to be, and the night was long enough for them to find you.";
		}
		else if (Out.Approaches == 0)
		{
			Out.Story = "Nothing came. That is not the same as nothing being there.";
		}
		else if (bLit)
		{
			Out.Story = "They came as far as the edge of the light, several times, and went no further.";
		}
		else
		{
			Out.Story = "Something tested the door more than once. The boards held.";
		}
		return Out;
	}

	void SundownState::NewDay(Rng& Random)
	{
		if (!bActive)
		{
			return;
		}
		++DayNumber;
		Held.Consume(2, 2);

		// The Alpha forgets a habit slowly if it stops paying off.
		for (Pattern& Value : Patterns)
		{
			if (Value.LastVisitDay != DayNumber)
			{
				Value.Expectation = std::max(0.0, Value.Expectation - Rules.ForgetPerDay);
			}
		}

		// Nests left alone fill back up.
		for (Nest& Value : NestList)
		{
			if (Value.bCleared && DayNumber - Value.ClearedOnDay >= Rules.RepopulateDays)
			{
				Value.bCleared = false;
				Value.Occupants = 2 + Random.RangeInt(0, 5);
				Population += static_cast<double>(Value.Occupants);
			}
		}
		DreadValue = std::max(0.0, DreadValue - 0.4);
	}

	// ---------------------------------------------------------------------- the cure

	bool SundownState::Research(int MedicalSpent, double Hours, Rng& Random)
	{
		if (!bActive || bDeployed || MedicalSpent <= 0 || Hours <= 0.0)
		{
			return false;
		}
		if (Held.Medical < MedicalSpent)
		{
			return false;
		}
		Held.Medical -= MedicalSpent;
		// Supplies and time both matter, and neither substitutes for the other.
		const double Step = std::min(static_cast<double>(MedicalSpent) / 14.0, Hours / 9.0) *
			(0.7 + 0.6 * Random.NextDouble());
		Cure += Step;
		while (Cure >= 1.0)
		{
			Cure -= 1.0;
			++DoseCount;
		}
		return true;
	}

	bool SundownState::Deploy(OutbreakSystem* Outbreak)
	{
		if (!bActive || bDeployed || !CureReady())
		{
			return false;
		}
		bDeployed = true;
		if (Outbreak != nullptr && Outbreak->IsActive())
		{
			Outbreak->DeployTreatment(Clamp(static_cast<double>(DoseCount) /
				static_cast<double>(Rules.DosesForCure), 0.0, 1.0));
		}
		return true;
	}

	// ---------------------------------------------------------------------- the Alpha

	int SundownState::BlockKeyFor(double X, double Y) const
	{
		const double Size = std::max(1.0, Rules.BlockM);
		const int Bx = static_cast<int>(std::floor(X / Size));
		const int By = static_cast<int>(std::floor(Y / Size));
		// Packed so two blocks cannot collide anywhere inside a state-sized world.
		return (Bx + 4096) * 16384 + (By + 4096);
	}

	Pattern* SundownState::FindPattern(int BlockKey)
	{
		for (Pattern& Value : Patterns)
		{
			if (Value.BlockKey == BlockKey)
			{
				return &Value;
			}
		}
		return nullptr;
	}

	double SundownState::Expectation(double X, double Y) const
	{
		const int Key = BlockKeyFor(X, Y);
		for (const Pattern& Value : Patterns)
		{
			if (Value.BlockKey == Key)
			{
				return Value.Expectation;
			}
		}
		return 0.0;
	}

	void SundownState::Noticed(double X, double Y, int OnDay)
	{
		const int Key = BlockKeyFor(X, Y);
		Pattern* Found = FindPattern(Key);
		if (Found == nullptr)
		{
			Patterns.push_back(Pattern{Key, 0, -1, 0.0});
			Found = &Patterns.back();
		}
		++Found->Visits;
		Found->LastVisitDay = OnDay;
		// Approaches certainty without reaching it: there is always a chance it guessed wrong, which
		// is what keeps the player willing to gamble.
		Found->Expectation = Clamp(
			Found->Expectation + (1.0 - Found->Expectation) * Rules.LearnRate, 0.0, 0.95);
	}

	// ---------------------------------------------------------------------- saving

	SundownState::Snapshot SundownState::Save() const
	{
		Snapshot Data;
		Data.bActive = bActive;
		Data.DayNumber = DayNumber;
		Data.Held = Held;
		Data.Home = Home;
		Data.Cure = Cure;
		Data.DoseCount = DoseCount;
		Data.bDeployed = bDeployed;
		Data.DreadValue = DreadValue;
		Data.Population = Population;
		Data.NestList = NestList;
		Data.Patterns = Patterns;
		Data.PickedOver = PickedOver;
		return Data;
	}

	void SundownState::Load(const Snapshot& Data)
	{
		bActive = Data.bActive;
		DayNumber = Data.DayNumber;
		Held = Data.Held;
		Home = Data.Home;
		Cure = Data.Cure;
		DoseCount = Data.DoseCount;
		bDeployed = Data.bDeployed;
		DreadValue = Data.DreadValue;
		Population = Data.Population;
		NestList = Data.NestList;
		Patterns = Data.Patterns;
		PickedOver = Data.PickedOver;
	}
}
