#pragma once
#include "CoreMinimal.h"
/** Checked against UE 5.8.3's UnrealClient.h. */
struct FScreenshotRequest
{
	static void RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddFilenameSuffix, bool bHdrScreenshot = false);
};
