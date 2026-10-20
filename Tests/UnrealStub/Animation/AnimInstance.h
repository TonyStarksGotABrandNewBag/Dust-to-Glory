#pragma once
#include "CoreMinimal.h"
enum class ERootMotionMode : uint8
{
	NoRootMotionExtraction,
	IgnoreRootMotion,
	RootMotionFromEverything,
	RootMotionFromMontagesOnly,
};
class UAnimInstance : public UObject
{
public:
	ERootMotionMode RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
};
