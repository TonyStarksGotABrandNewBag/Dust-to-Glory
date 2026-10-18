#pragma once
#include "Components/DirectionalLightComponent.h"
class UPointLightComponent : public ULightComponent
{
public:
	void SetAttenuationRadius(float Value) { AttenuationRadius = Value; }
	void SetSourceRadius(float Value) { (void)Value; }
	float AttenuationRadius = 1000.0f;
};
