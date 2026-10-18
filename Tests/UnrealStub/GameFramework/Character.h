#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"

class UCapsuleComponent;
class USkeletalMeshComponent;

class ACharacter : public APawn
{
public:
	virtual void Jump() {}
	virtual void StopJumping() {}
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
	USkeletalMeshComponent* GetMesh() const { return nullptr; }
	UCharacterMovementComponent* CharacterMovement = nullptr;
	UCapsuleComponent* CapsuleComponent = nullptr;
};
