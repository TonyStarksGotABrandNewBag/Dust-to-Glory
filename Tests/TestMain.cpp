#include "TestFramework.h"

#include <cstring>

namespace dtg_test
{
	std::vector<TestCase>& Registry()
	{
		static std::vector<TestCase> Tests;
		return Tests;
	}

	int& Failures()
	{
		static int Count = 0;
		return Count;
	}

	const char*& CurrentTest()
	{
		static const char* Name = "";
		return Name;
	}
}

int main(int argc, char** argv)
{
	const char* Filter = argc > 1 ? argv[1] : nullptr;
	int Ran = 0;
	for (const dtg_test::TestCase& Test : dtg_test::Registry())
	{
		if (Filter && !std::strstr(Test.Name, Filter))
		{
			continue;
		}
		dtg_test::CurrentTest() = Test.Name;
		const int Before = dtg_test::Failures();
		Test.Fn();
		++Ran;
		std::printf("%s %s\n", dtg_test::Failures() == Before ? "  ok  " : "  FAIL", Test.Name);
	}
	std::printf("\n%d tests, %d failed checks\n", Ran, dtg_test::Failures());
	return dtg_test::Failures() == 0 ? 0 : 1;
}
