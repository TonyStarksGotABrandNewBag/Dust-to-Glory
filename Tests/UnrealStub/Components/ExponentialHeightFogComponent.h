#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class UExponentialHeightFogComponent : public USceneComponent
{
public:
	void SetFogDensity(float Value) { FogDensity = Value; }
	void SetFogHeightFalloff(float Value) { FogHeightFalloff = Value; }
	void SetFogInscatteringColor(const FLinearColor& Colour) { (void)Colour; }
	void SetStartDistance(float Value) { (void)Value; }
	void SetVolumetricFog(bool bEnabled) { (void)bEnabled; }
	void SetVolumetricFogExtinctionScale(float Value) { (void)Value; }
	float FogDensity = 0.02f;
	float FogHeightFalloff = 0.2f;
};
