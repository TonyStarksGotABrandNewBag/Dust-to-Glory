#pragma once
#include "Components/DirectionalLightComponent.h"
enum ESkyLightSourceType
{
	SLS_CapturedScene,
	SLS_SpecifiedCubemap,
};

class USkyLightComponent : public ULightComponentBase
{
public:
	void SetIntensity(float Value) { Intensity = Value; }
	void RecaptureSky() {}
	void SetLightColor(const FLinearColor& Colour) { (void)Colour; }
	ESkyLightSourceType SourceType = SLS_CapturedScene;
	bool bRealTimeCapture = false;
};
