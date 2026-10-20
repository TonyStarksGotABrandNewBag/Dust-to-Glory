#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
/** Checked against UE 5.8.3's InstancedStaticMeshComponent.h. */
class UInstancedStaticMeshComponent : public UStaticMeshComponent
{
public:
	virtual int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false);
	virtual TArray<int32> AddInstances(const TArray<FTransform>& InstanceTransforms, bool bShouldReturnIndices,
		bool bWorldSpace = false, bool bUpdateNavigation = true);
	virtual void ClearInstances();
	int32 GetInstanceCount() const;
};
