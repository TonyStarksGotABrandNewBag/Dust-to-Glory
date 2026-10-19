#pragma once
#include "CoreMinimal.h"
struct FPlatformMisc
{
	static void RequestExit(bool Force, const TCHAR* CallSite = nullptr);
};
