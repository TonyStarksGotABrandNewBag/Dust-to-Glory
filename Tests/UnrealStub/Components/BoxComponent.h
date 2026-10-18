#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class UShapeComponent : public UPrimitiveComponent
{
};
class UBoxComponent : public UShapeComponent
{
public:
	void SetBoxExtent(const FVector& Extent, bool bUpdateOverlaps = true)
	{
		(void)Extent;
		(void)bUpdateOverlaps;
	}
	FVector GetScaledBoxExtent() const { return FVector(); }
};
