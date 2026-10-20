#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class UGroomAsset;
class UGroomBindingAsset;
/** Checked against UE 5.8.3's HairStrandsCore GroomComponent.h. */
class UGroomComponent : public UMeshComponent
{
public:
	void SetGroomAsset(UGroomAsset* Asset);
	void SetBindingAsset(UGroomBindingAsset* InBinding);
	void SetGroomAsset(UGroomAsset* Asset, UGroomBindingAsset* InBinding, const bool bUpdateSimulation = true);
};
