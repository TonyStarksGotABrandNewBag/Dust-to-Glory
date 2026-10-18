#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class USpringArmComponent : public USceneComponent
{
public:
	/** The socket a camera attaches to at the end of the arm. */
	static const FName SocketName;
	float TargetArmLength = 300.0f;
	FVector SocketOffset;
	FVector TargetOffset;
	bool bUsePawnControlRotation = false;
	bool bDoCollisionTest = true;
	bool bEnableCameraLag = false;
	bool bEnableCameraRotationLag = false;
	float CameraLagSpeed = 10.0f;
	float CameraRotationLagSpeed = 10.0f;
};
