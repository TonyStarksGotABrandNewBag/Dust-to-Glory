// Tiny test harness (no exceptions, so it works with the same flags Unreal uses).
#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace dtg_test
{
	struct TestCase
	{
		const char* Name;
		void (*Fn)();
	};

	std::vector<TestCase>& Registry();
	int& Failures();
	const char*& CurrentTest();

	struct Registrar
	{
		Registrar(const char* Name, void (*Fn)()) { Registry().push_back(TestCase{Name, Fn}); }
	};

	inline void Fail(const char* File, int Line, const std::string& Message)
	{
		++Failures();
		std::printf("  FAIL [%s] %s:%d: %s\n", CurrentTest(), File, Line, Message.c_str());
	}
}

#define DTG_TEST(Name)                                                   \
	static void Name();                                                  \
	static dtg_test::Registrar Registrar_##Name(#Name, &Name);          \
	static void Name()

#define EXPECT(Condition)                                                \
	do                                                                   \
	{                                                                    \
		if (!(Condition))                                                \
		{                                                                \
			dtg_test::Fail(__FILE__, __LINE__, #Condition);              \
		}                                                                \
	} while (0)

#define EXPECT_MSG(Condition, Message)                                   \
	do                                                                   \
	{                                                                    \
		if (!(Condition))                                                \
		{                                                                \
			dtg_test::Fail(__FILE__, __LINE__, std::string(#Condition) + " -- " + (Message)); \
		}                                                                \
	} while (0)

#define EXPECT_NEAR(A, B, Tolerance)                                     \
	do                                                                   \
	{                                                                    \
		const double ValueA_ = static_cast<double>(A);                   \
		const double ValueB_ = static_cast<double>(B);                   \
		if (std::fabs(ValueA_ - ValueB_) > (Tolerance))                  \
		{                                                                \
			dtg_test::Fail(__FILE__, __LINE__, std::string(#A " ~= " #B ": ") + std::to_string(ValueA_) + " vs " + std::to_string(ValueB_)); \
		}                                                                \
	} while (0)
