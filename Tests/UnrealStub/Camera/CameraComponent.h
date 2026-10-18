#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class UCameraComponent : public USceneComponent
{
public:
	void SetFieldOfView(float Value) { FieldOfView = Value; }
	float FieldOfView = 90.0f;
	bool bUsePawnControlRotation = false;
};
