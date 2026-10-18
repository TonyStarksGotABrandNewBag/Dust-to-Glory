// Starts a session: load or begin a game, build the world, put the player outside their apartment in Longs,
// and drive the simulation clock. Everything else is spawned from here so there is no map asset to maintain.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "DTGGameMode.generated.h"

class ADTGEventDirector;
class ADTGTrafficDirector;
class ADTGWorldBuilder;
class UDTGGameSubsystem;

UCLASS()
class DUSTTOGLORY_API ADTGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADTGGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** Loads the named save instead of starting fresh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dust to Glory")
	FString LoadSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dust to Glory")
	FString PlayerName = TEXT("Newcomer");

private:
	void SpawnDirectors();
	/** Spawns the player's own car outside the apartment, as the simulation says they own one. */
	void SpawnStarterCar();

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TObjectPtr<ADTGWorldBuilder> World;

	UPROPERTY()
	TObjectPtr<ADTGTrafficDirector> Traffic;

	UPROPERTY()
	TObjectPtr<ADTGEventDirector> Events;

	FVector SpawnPoint = FVector::ZeroVector;
	bool bStarted = false;
};
