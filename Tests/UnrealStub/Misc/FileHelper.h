#pragma once
#include "CoreMinimal.h"
struct FFileHelper
{
	static bool LoadFileToString(FString& OutText, const TCHAR* Path);
	static bool SaveStringToFile(const FString& Text, const TCHAR* Path);
	static bool LoadFileToStringArray(TArray<FString>& OutLines, const TCHAR* Path);
};
