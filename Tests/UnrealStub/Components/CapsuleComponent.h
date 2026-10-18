#pragma once
#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
class UCapsuleComponent : public UShapeComponent
{
public:
	void SetCapsuleSize(float Radius, float HalfHeight, bool bUpdateOverlaps = true)
	{
		(void)Radius;
		(void)HalfHeight;
		(void)bUpdateOverlaps;
	}
	void InitCapsuleSize(float Radius, float HalfHeight) { (void)Radius; (void)HalfHeight; }
	float GetScaledCapsuleRadius() const { return 34.0f; }
	float GetScaledCapsuleHalfHeight() const { return 88.0f; }
};
