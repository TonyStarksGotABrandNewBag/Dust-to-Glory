#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class UCharacterMovementComponent : public UActorComponent
{
public:
	void SetMovementMode(EMovementMode Mode, uint8 CustomMode = 0) { (void)Mode; (void)CustomMode; }
	void SetDefaultMovementMode() {}
	void SetAvoidanceEnabled(bool bEnable) { (void)bEnable; }
	EMovementMode MovementMode = MOVE_Walking;
	bool IsFalling() const { return false; }
	bool IsMovingOnGround() const { return true; }
	FVector Velocity;
	float MaxWalkSpeed = 600.0f;
	float MaxFlySpeed = 600.0f;
	float MaxSwimSpeed = 300.0f;
	float MaxCustomMovementSpeed = 600.0f;
	float MaxAcceleration = 2048.0f;
	float JumpZVelocity = 420.0f;
	float AirControl = 0.05f;
	float GravityScale = 1.0f;
	float BrakingDecelerationWalking = 2048.0f;
	float GroundFriction = 8.0f;
	bool bOrientRotationToMovement = false;
	bool bRunPhysicsWithNoController = false;
	float AvoidanceConsiderationRadius = 500.0f;
	FRotator RotationRate;
};
