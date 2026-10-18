// Dust to Glory - simulation core.
//
// Everything under Source/DustToGlory/Sim is plain, engine-independent C++20. Unreal compiles it as
// part of the game module, and Tests/ compiles the same files with g++/clang/MSVC so the game rules
// can be unit-tested without the engine. Rules for this folder:
//   * no Unreal headers, no exceptions, no RTTI;
//   * no ALL_CAPS identifiers (Unreal and Windows define many such macros);
//   * file-local helpers live in a uniquely named detail namespace (Unreal uses unity builds).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dtg::sim
{
	/** Money is stored in whole cents to avoid floating-point drift. */
	using Money = std::int64_t;

	constexpr Money Dollars(std::int64_t WholeDollars) { return WholeDollars * 100; }

	/** "$1,234" for whole-dollar values, "$1,234.56" otherwise, "-$40" for negatives. */
	std::string FormatMoney(Money Cents);

	/** Stable 64-bit FNV-1a hash, used to derive per-entity seeds from ids. */
	std::uint64_t HashString(const std::string& Text);

	template <typename T>
	constexpr T Clamp(T Value, T Lo, T Hi)
	{
		return Value < Lo ? Lo : (Value > Hi ? Hi : Value);
	}

	inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }

	/** Rounds a dollar amount (as double) to cents. */
	inline Money ToMoney(double DollarsValue)
	{
		return static_cast<Money>(std::llround(DollarsValue * 100.0));
	}

	inline double ToDollars(Money Cents) { return static_cast<double>(Cents) / 100.0; }

	/** PCG32 random generator: small, fast, deterministic across platforms, and saveable. */
	class Rng
	{
	public:
		explicit Rng(std::uint64_t Seed = 0x853c49e6748fea9bULL, std::uint64_t Stream = 0xda3e39cb94b95bdbULL)
		{
			Reseed(Seed, Stream);
		}

		void Reseed(std::uint64_t Seed, std::uint64_t Stream = 0xda3e39cb94b95bdbULL)
		{
			StateValue = 0;
			Increment = (Stream << 1u) | 1u;
			NextU32();
			StateValue += Seed;
			NextU32();
		}

		std::uint32_t NextU32()
		{
			const std::uint64_t Old = StateValue;
			StateValue = Old * 6364136223846793005ULL + Increment;
			const std::uint32_t XorShifted = static_cast<std::uint32_t>(((Old >> 18u) ^ Old) >> 27u);
			const std::uint32_t Rot = static_cast<std::uint32_t>(Old >> 59u);
			return (XorShifted >> Rot) | (XorShifted << ((0u - Rot) & 31u));
		}

		/** Uniform double in [0, 1). */
		double NextDouble() { return static_cast<double>(NextU32()) / 4294967296.0; }

		/** Uniform integer in [Lo, HiInclusive]. Returns Lo when the range is empty. */
		int RangeInt(int Lo, int HiInclusive)
		{
			if (HiInclusive <= Lo)
			{
				return Lo;
			}
			const std::uint32_t Span = static_cast<std::uint32_t>(HiInclusive - Lo) + 1u;
			return Lo + static_cast<int>(NextU32() % Span);
		}

		double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * NextDouble(); }

		bool Chance(double Probability) { return NextDouble() < Probability; }

		/** Index into a container of the given size; callers must ensure Size > 0. */
		std::size_t Index(std::size_t Size)
		{
			return Size <= 1 ? 0 : static_cast<std::size_t>(NextU32() % static_cast<std::uint32_t>(Size));
		}

		template <typename T>
		const T& Pick(const std::vector<T>& Items)
		{
			return Items[Index(Items.size())];
		}

		std::uint64_t GetState() const { return StateValue; }
		std::uint64_t GetIncrement() const { return Increment; }
		void Restore(std::uint64_t InState, std::uint64_t InIncrement)
		{
			StateValue = InState;
			Increment = InIncrement | 1u;
		}

	private:
		std::uint64_t StateValue = 0;
		std::uint64_t Increment = 1;
	};

	/** Lower-case copy (ASCII only). */
	std::string ToLowerAscii(const std::string& Text);

	/** Turns "Ardent Motors Comet" into "ardent-motors-comet". */
	std::string Slugify(const std::string& Text);
}
