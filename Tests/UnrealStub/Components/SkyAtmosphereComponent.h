#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class USkyAtmosphereComponent : public USceneComponent
{
public:
	void SetRayleighScattering(const FLinearColor& Colour) { (void)Colour; }
	void SetMieScatteringScale(float Value) { (void)Value; }
	void SetMultiScatteringFactor(float Value) { (void)Value; }
	void SetAtmosphereHeight(float Value) { (void)Value; }
};
