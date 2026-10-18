#pragma once
#include "Components/PointLightComponent.h"
class USpotLightComponent : public UPointLightComponent
{
public:
	void SetInnerConeAngle(float Value) { InnerConeAngle = Value; }
	void SetOuterConeAngle(float Value) { OuterConeAngle = Value; }
	float InnerConeAngle = 0.0f;
	float OuterConeAngle = 44.0f;
};
