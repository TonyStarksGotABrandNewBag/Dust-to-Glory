#pragma once

#include "Sim/SimCommon.h"

#include <string>

namespace dtg::sim
{
	/**
	 * Game time. Day 0 is a Monday and the game starts at 08:00.
	 * Default pace matches open-world convention: one game minute every two real seconds (48-minute days).
	 */
	class GameClock
	{
	public:
		static constexpr std::int64_t MinutesPerDay = 24 * 60;

		/** Advances by real seconds; returns how many whole game minutes elapsed. */
		std::int64_t Advance(double RealSeconds);
		/** Skips ahead a number of game minutes (sleeping, working a shift). */
		void Skip(std::int64_t SkipMinutes);

		std::int64_t TotalMinutes() const { return static_cast<std::int64_t>(Minutes); }
		int Day() const { return static_cast<int>(TotalMinutes() / MinutesPerDay); }
		int Hour() const { return static_cast<int>((TotalMinutes() % MinutesPerDay) / 60); }
		int Minute() const { return static_cast<int>(TotalMinutes() % 60); }
		/** 0 = Monday ... 6 = Sunday. */
		int DayOfWeek() const { return Day() % 7; }
		/** 0..1 through the current day (0.5 = noon). Continuous, for sun movement. */
		double DayFraction() const;

		std::string Format() const;
		static const char* DayName(int DayOfWeek);

		void SetTotalMinutes(double InMinutes) { Minutes = InMinutes < 0.0 ? 0.0 : InMinutes; }
		double ExactMinutes() const { return Minutes; }

		/** Game minutes per real second. */
		double TimeScale = 0.5;

	private:
		double Minutes = 8.0 * 60.0;
	};

	enum class Weather : int
	{
		Clear,
		PartlyCloudy,
		Overcast,
		Rain,
		Storm,
		Fog,
		Count
	};

	const char* WeatherName(Weather Value);
	/** Tire grip multiplier for a surface in this weather: dry 1, standing water much less. */
	double WeatherGrip(Weather Value);
	/** How much of a wet road is still wet: rain that has just stopped is the worst of it. */
	double WeatherVisibilityM(Weather Value);

	/** Markov-chain weather that changes every few game hours. */
	class WeatherSystem
	{
	public:
		Weather Current() const { return CurrentWeather; }
		/** Call with the current game minute; returns true when the weather changed. */
		bool Update(std::int64_t NowMinute, Rng& Random);
		/** Weather control powers and debug menus force a state for a while. */
		void Force(Weather Value, std::int64_t NowMinute, std::int64_t HoldMinutes = 180);

		std::int64_t NextChangeMinute() const { return NextChange; }
		void Restore(Weather Value, std::int64_t InNextChange)
		{
			CurrentWeather = Value;
			NextChange = InNextChange;
		}

	private:
		Weather CurrentWeather = Weather::Clear;
		std::int64_t NextChange = 8 * 60 + 180;
	};
}
