#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class ULightComponentBase : public USceneComponent
{
public:
	void SetIntensity(float Value) { Intensity = Value; }
	void SetLightColor(const FLinearColor& Colour, bool bSrgb = true) { (void)Colour; (void)bSrgb; }
	void SetCastShadows(bool bCast) { (void)bCast; }
	float Intensity = 1.0f;
	FLinearColor LightColor;
	bool bAffectsWorld = true;
};
class ULightComponent : public ULightComponentBase
{
public:
	void SetTemperature(float Value) { (void)Value; }
	void SetUseTemperature(bool bUse) { (void)bUse; }
};
class UDirectionalLightComponent : public ULightComponent
{
public:
	void SetDynamicShadowDistanceMovableLight(float Value) { (void)Value; }
	void SetLightSourceAngle(float Value) { (void)Value; }
	float DynamicShadowDistanceMovableLight = 20000.0f;
	bool bAtmosphereSunLight = false;
	bool bUsedAsAtmosphereSunLight = false;
	int32 AtmosphereSunLightIndex = 0;
};
