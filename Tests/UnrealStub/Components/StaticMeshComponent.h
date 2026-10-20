#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class UStaticMesh;
/** Checked against UE 5.8.3's StaticMeshComponent.h. */
class UStaticMeshComponent : public UMeshComponent
{
public:
	virtual bool SetStaticMesh(UStaticMesh* NewMesh);
};
