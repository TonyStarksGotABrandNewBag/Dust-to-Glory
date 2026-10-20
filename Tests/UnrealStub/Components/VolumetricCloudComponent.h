#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class UMaterialInterface;
/** Checked against UE 5.8.3's VolumetricCloudComponent.h. */
class UVolumetricCloudComponent : public USceneComponent
{
public:
	void SetMaterial(UMaterialInterface* NewValue);
	void SetLayerBottomAltitude(float NewValue);
	void SetLayerHeight(float NewValue);
};
