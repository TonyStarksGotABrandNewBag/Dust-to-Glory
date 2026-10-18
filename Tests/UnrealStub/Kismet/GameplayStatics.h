#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"

class AGameModeBase;

class UGameplayStatics : public UObject
{
public:
	static APlayerController* GetPlayerController(const UObject* WorldContext, int32 PlayerIndex);
	static APawn* GetPlayerPawn(const UObject* WorldContext, int32 PlayerIndex);
	static ACharacter* GetPlayerCharacter(const UObject* WorldContext, int32 PlayerIndex);
	static AGameModeBase* GetGameMode(const UObject* WorldContext);
	static AActor* GetActorOfClass(const UObject* WorldContext, TSubclassOf<AActor> Class);
	static void GetAllActorsOfClass(const UObject* WorldContext, TSubclassOf<AActor> Class,
		TArray<AActor*>& OutActors);
	static void PlaySoundAtLocation(const UObject* WorldContext, class USoundBase* Sound,
		const FVector& Location, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);
};

class USoundBase : public UObject
{
};
