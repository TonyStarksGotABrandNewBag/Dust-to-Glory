#pragma once
#include "CoreMinimal.h"
class UStaticMesh : public UObject
{
public:
	FBoxSphereBounds GetBounds() const;
};
