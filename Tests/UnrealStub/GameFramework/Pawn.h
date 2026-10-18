#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class AController;
class UInputComponent;

class APawn : public AActor
{
public:
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) { (void)InputComponent; }
	virtual void PossessedBy(AController* NewController) { (void)NewController; }
	virtual void UnPossessed() {}
	void AddMovementInput(const FVector& Direction, float Scale = 1.0f, bool bForce = false)
	{
		(void)Direction;
		(void)Scale;
		(void)bForce;
	}
	AController* GetController() const { return Controller; }
	class APlayerController* GetLocalViewingPlayerController() const { return nullptr; }
	bool IsPlayerControlled() const { return false; }
	bool IsLocallyControlled() const { return true; }
	FVector GetVelocity() const { return FVector(); }
	FRotator GetControlRotation() const { return FRotator(); }

	AController* Controller = nullptr;
	bool bUseControllerRotationYaw = false;
	bool bUseControllerRotationPitch = false;
	bool bUseControllerRotationRoll = false;
};

class AController : public AActor
{
public:
	APawn* GetPawn() const { return Pawn; }
	void Possess(APawn* InPawn) { Pawn = InPawn; }
	void UnPossess() { Pawn = nullptr; }
	FRotator GetControlRotation() const { return ControlRotation; }
	void SetControlRotation(const FRotator& Value) { ControlRotation = Value; }
	APawn* Pawn = nullptr;
	FRotator ControlRotation;
};
