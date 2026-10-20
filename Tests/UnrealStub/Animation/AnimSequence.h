#pragma once
#include "CoreMinimal.h"
class UAnimationAsset : public UObject
{
};
class UAnimSequenceBase : public UAnimationAsset
{
public:
	virtual float GetPlayLength() const;
};
class UAnimSequence : public UAnimSequenceBase
{
};
