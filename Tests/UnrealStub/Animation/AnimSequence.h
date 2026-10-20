#pragma once
#include "CoreMinimal.h"
class UAnimationAsset : public UObject
{
};
struct FAnimExtractContext
{
	double CurrentTime = 0.0;
	bool bExtractRootMotion = false;
	bool bLooping = false;
};
class UAnimSequenceBase : public UAnimationAsset
{
public:
	virtual float GetPlayLength() const;
	virtual FTransform ExtractRootMotionFromRange(double StartTime, double EndTime, const FAnimExtractContext& ExtractionContext) const;
};
class UAnimSequence : public UAnimSequenceBase
{
};
