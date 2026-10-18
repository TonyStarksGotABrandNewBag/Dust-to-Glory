#include "Sim/SimCommon.h"

#include <cstdio>

namespace dtg::sim
{
	std::string FormatMoney(Money Cents)
	{
		const bool bNegative = Cents < 0;
		const std::uint64_t Abs = bNegative ? static_cast<std::uint64_t>(-(Cents + 1)) + 1u : static_cast<std::uint64_t>(Cents);
		const std::uint64_t Whole = Abs / 100u;
		const std::uint64_t Fraction = Abs % 100u;

		std::string Digits = std::to_string(Whole);
		std::string Grouped;
		Grouped.reserve(Digits.size() + Digits.size() / 3);
		const std::size_t Lead = Digits.size() % 3;
		for (std::size_t Index = 0; Index < Digits.size(); ++Index)
		{
			if (Index != 0 && (Index + (3 - Lead)) % 3 == 0)
			{
				Grouped.push_back(',');
			}
			Grouped.push_back(Digits[Index]);
		}

		std::string Result = bNegative ? "-$" : "$";
		Result += Grouped;
		if (Fraction != 0)
		{
			char Buffer[8];
			std::snprintf(Buffer, sizeof(Buffer), ".%02u", static_cast<unsigned>(Fraction));
			Result += Buffer;
		}
		return Result;
	}

	std::uint64_t HashString(const std::string& Text)
	{
		std::uint64_t Hash = 1469598103934665603ULL;
		for (const char Character : Text)
		{
			Hash ^= static_cast<std::uint8_t>(Character);
			Hash *= 1099511628211ULL;
		}
		return Hash;
	}

	std::string ToLowerAscii(const std::string& Text)
	{
		std::string Result = Text;
		for (char& Character : Result)
		{
			if (Character >= 'A' && Character <= 'Z')
			{
				Character = static_cast<char>(Character - 'A' + 'a');
			}
		}
		return Result;
	}

	std::string Slugify(const std::string& Text)
	{
		std::string Result;
		Result.reserve(Text.size());
		bool bLastWasDash = true;
		for (const char Raw : Text)
		{
			char Character = Raw;
			if (Character >= 'A' && Character <= 'Z')
			{
				Character = static_cast<char>(Character - 'A' + 'a');
			}
			const bool bAlnum = (Character >= 'a' && Character <= 'z') || (Character >= '0' && Character <= '9');
			if (bAlnum)
			{
				Result.push_back(Character);
				bLastWasDash = false;
			}
			else if (!bLastWasDash)
			{
				Result.push_back('-');
				bLastWasDash = true;
			}
		}
		while (!Result.empty() && Result.back() == '-')
		{
			Result.pop_back();
		}
		return Result;
	}
}
