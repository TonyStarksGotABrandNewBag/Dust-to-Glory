#include "Sim/Clock.h"

#include <cstdio>

namespace dtg::sim
{
	std::int64_t GameClock::Advance(double RealSeconds)
	{
		if (RealSeconds <= 0.0)
		{
			return 0;
		}
		const std::int64_t Before = TotalMinutes();
		Minutes += RealSeconds * TimeScale;
		return TotalMinutes() - Before;
	}

	void GameClock::Skip(std::int64_t SkipMinutes)
	{
		if (SkipMinutes > 0)
		{
			Minutes += static_cast<double>(SkipMinutes);
		}
	}

	double GameClock::DayFraction() const
	{
		const double DayLength = static_cast<double>(MinutesPerDay);
		const double Within = Minutes - std::floor(Minutes / DayLength) * DayLength;
		return Within / DayLength;
	}

	const char* GameClock::DayName(int DayOfWeekIndex)
	{
		static const char* const Names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
		const int Index = ((DayOfWeekIndex % 7) + 7) % 7;
		return Names[Index];
	}

	std::string GameClock::Format() const
	{
		char Buffer[32];
		std::snprintf(Buffer, sizeof(Buffer), "Day %d %s %02d:%02d", Day() + 1, DayName(DayOfWeek()), Hour(), Minute());
		return Buffer;
	}

	double WeatherGrip(Weather Value)
	{
		// What the tires actually find under them. Rain is the big one; fog is a visibility problem rather
		// than a grip one, and storms add standing water on top of the rain.
		switch (Value)
		{
		case Weather::Clear: return 1.0;
		case Weather::PartlyCloudy: return 1.0;
		case Weather::Overcast: return 0.98;
		case Weather::Rain: return 0.72;
		case Weather::Storm: return 0.6;
		case Weather::Fog: return 0.93;
		case Weather::Count: break;
		}
		return 1.0;
	}

	double WeatherVisibilityM(Weather Value)
	{
		switch (Value)
		{
		case Weather::Clear: return 6000.0;
		case Weather::PartlyCloudy: return 5000.0;
		case Weather::Overcast: return 3000.0;
		case Weather::Rain: return 1400.0;
		case Weather::Storm: return 700.0;
		case Weather::Fog: return 220.0;
		case Weather::Count: break;
		}
		return 6000.0;
	}

	const char* WeatherName(Weather Value)
	{
		switch (Value)
		{
		case Weather::Clear: return "Clear";
		case Weather::PartlyCloudy: return "Partly Cloudy";
		case Weather::Overcast: return "Overcast";
		case Weather::Rain: return "Rain";
		case Weather::Storm: return "Thunderstorm";
		case Weather::Fog: return "Fog";
		case Weather::Count: break;
		}
		return "Clear";
	}

	namespace detail_weather
	{
		// Transition weights, rows = from, columns = to (Clear, PartlyCloudy, Overcast, Rain, Storm, Fog).
		constexpr double Transitions[6][6] = {
			{0.45, 0.35, 0.10, 0.04, 0.01, 0.05},
			{0.35, 0.30, 0.20, 0.10, 0.02, 0.03},
			{0.15, 0.25, 0.25, 0.25, 0.07, 0.03},
			{0.10, 0.15, 0.30, 0.30, 0.12, 0.03},
			{0.05, 0.10, 0.30, 0.45, 0.10, 0.00},
			{0.40, 0.30, 0.20, 0.05, 0.00, 0.05},
		};
	}

	bool WeatherSystem::Update(std::int64_t NowMinute, Rng& Random)
	{
		if (NowMinute < NextChange)
		{
			return false;
		}
		const int From = static_cast<int>(CurrentWeather);
		double Roll = Random.NextDouble();
		Weather Next = CurrentWeather;
		for (int To = 0; To < static_cast<int>(Weather::Count); ++To)
		{
			Roll -= detail_weather::Transitions[From][To];
			if (Roll <= 0.0)
			{
				Next = static_cast<Weather>(To);
				break;
			}
		}
		NextChange = NowMinute + Random.RangeInt(120, 360);
		const bool bChanged = Next != CurrentWeather;
		CurrentWeather = Next;
		return bChanged;
	}

	void WeatherSystem::Force(Weather Value, std::int64_t NowMinute, std::int64_t HoldMinutes)
	{
		CurrentWeather = Value;
		NextChange = NowMinute + (HoldMinutes > 0 ? HoldMinutes : 60);
	}
}
