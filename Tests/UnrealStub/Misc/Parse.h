#pragma once
#include "CoreMinimal.h"
struct FParse
{
	/** True when Stream contains -Param as a switch. */
	static bool Param(const TCHAR* Stream, const TCHAR* Param);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FString& Value, bool bShouldStopOnSeparator = true);
};
