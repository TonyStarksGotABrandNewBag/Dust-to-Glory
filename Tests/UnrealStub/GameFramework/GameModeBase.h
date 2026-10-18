#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

class APlayerController;

class AGameModeBase : public AActor
{
public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
	{
		(void)MapName;
		(void)Options;
		(void)ErrorMessage;
	}
	virtual void StartPlay() {}
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) { (void)Player; return nullptr; }
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* Player)
	{
		(void)Player;
		return nullptr;
	}
	TSubclassOf<APawn> DefaultPawnClass;
	TSubclassOf<APlayerController> PlayerControllerClass;
	TSubclassOf<class AHUD> HUDClass;
};
