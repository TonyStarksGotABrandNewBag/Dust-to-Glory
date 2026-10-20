#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
/**
 * Only the fields the project sets. NOTE: the real overrides are uint8 bitfields; assigning true/false
 * works the same on both. Names and types checked against UE 5.8.3's Engine/Scene.h.
 */
struct FPostProcessSettings
{
	bool bOverride_ColorSaturation = false;
	FVector4 ColorSaturation;
	bool bOverride_ColorContrast = false;
	FVector4 ColorContrast;
	bool bOverride_ColorGainHighlights = false;
	FVector4 ColorGainHighlights;
	bool bOverride_ColorOffsetShadows = false;
	FVector4 ColorOffsetShadows;
	bool bOverride_VignetteIntensity = false;
	float VignetteIntensity = 0.4f;
	bool bOverride_FilmGrainIntensity = false;
	float FilmGrainIntensity = 0.0f;
	bool bOverride_SceneFringeIntensity = false;
	float SceneFringeIntensity = 0.0f;
	bool bOverride_BloomIntensity = false;
	float BloomIntensity = 0.675f;
	bool bOverride_MotionBlurAmount = false;
	float MotionBlurAmount = 0.5f;
	bool bOverride_Sharpen = false;
	float Sharpen = 0.0f;
	bool bOverride_LumenFinalGatherQuality = false;
	float LumenFinalGatherQuality = 1.0f;
	bool bOverride_LumenSceneDetail = false;
	float LumenSceneDetail = 1.0f;
	bool bOverride_LumenReflectionQuality = false;
	float LumenReflectionQuality = 1.0f;
};

class UPostProcessComponent : public USceneComponent
{
public:
	FPostProcessSettings Settings;
	bool bUnbound = true;
};
