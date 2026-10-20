#pragma once
#include "Components/DirectionalLightComponent.h"
enum class ELightUnits : uint8
{
	Unitless,
	Candelas,
	Lumens,
	EV,
};
class UPointLightComponent : public ULightComponent
{
public:
	void SetIntensityUnits(ELightUnits NewIntensityUnits) { (void)NewIntensityUnits; }
	void SetAttenuationRadius(float Value) { AttenuationRadius = Value; }
	void SetSourceRadius(float Value) { (void)Value; }
	float AttenuationRadius = 1000.0f;
};
