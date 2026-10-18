#include "Game/DTGGameMode.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#include "Game/DTGCharacter.h"
#include "Game/DTGEventDirector.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGHud.h"
#include "Game/DTGPlayerController.h"
#include "Game/DTGSky.h"
#include "Game/DTGTrafficDirector.h"
#include "Game/DTGVehicle.h"
#include "Game/DTGWorldBuilder.h"

using namespace dtg::sim;

ADTGGameMode::ADTGGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ADTGCharacter::StaticClass();
	PlayerControllerClass = ADTGPlayerController::StaticClass();
	HUDClass = ADTGHud::StaticClass();
}

void ADTGGameMode::StartPlay()
{
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	if (Game && Game->IsContentLoaded())
	{
		const bool bLoaded = !LoadSlot.IsEmpty() && Game->LoadGame(LoadSlot);
		if (!bLoaded)
		{
			Game->StartNewGame(PlayerName);
		}
		// The simulation decides where a new game begins: outside the starter apartment, in Longs.
		const CityPlan& City = Game->Sim().City;
		const double GroundZ = Game->Sim().Map.HeightAt(City.SpawnX, City.SpawnY);
		SpawnPoint = Game->ToUnreal(City.SpawnX, City.SpawnY, GroundZ + 1.0);
	}

	// The world is built before the player exists, so there is ground under them when they appear.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World = GetWorld()->SpawnActor<ADTGWorldBuilder>(ADTGWorldBuilder::StaticClass(), FTransform::Identity, Params);

	Super::StartPlay();

	SpawnDirectors();
	SpawnStarterCar();
	bStarted = true;
	if (Game && Game->IsContentLoaded())
	{
		const CityInfo* Town = Game->Sim().City.CityAt(Game->Sim().City.SpawnX, Game->Sim().City.SpawnY);
		UE_LOG(LogDTG, Log, TEXT("Session started in %s"), Town ? UTF8_TO_TCHAR(Town->Name.c_str()) : TEXT("the state"));
	}
}

AActor* ADTGGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// No map assets, so the start is a spot the simulation chose; make a PlayerStart there on the fly.
	if (AActor* Existing = UGameplayStatics::GetActorOfClass(GetWorld(), APlayerStart::StaticClass()))
	{
		Existing->SetActorLocation(SpawnPoint);
		return Existing;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FTransform(FRotator::ZeroRotator, SpawnPoint), Params);
}

UClass* ADTGGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	return ADTGCharacter::StaticClass();
}

void ADTGGameMode::SpawnDirectors()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Traffic = GetWorld()->SpawnActor<ADTGTrafficDirector>(ADTGTrafficDirector::StaticClass(), FTransform::Identity, Params);
	// The sun, sky and fog follow the simulation's clock and weather.
	Sky = GetWorld()->SpawnActor<ADTGSky>(ADTGSky::StaticClass(), FTransform::Identity, Params);
	Events = GetWorld()->SpawnActor<ADTGEventDirector>(ADTGEventDirector::StaticClass(), FTransform::Identity, Params);
}

void ADTGGameMode::SpawnStarterCar()
{
	if (!Game || !Game->IsContentLoaded() || Game->Sim().Garage.empty())
	{
		return;
	}
	const OwnedVehicle& Car = Game->Sim().Garage.front();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADTGVehicle* Pawn = GetWorld()->SpawnActor<ADTGVehicle>(ADTGVehicle::StaticClass(), FTransform::Identity, Params);
	if (!Pawn)
	{
		return;
	}
	Pawn->ConfigureFromOwned(Car);
	// At the kerb, a few metres from the door.
	const FVector At = SpawnPoint + FVector(500.0f, 0.0f, 0.0f);
	const double GroundZ = World ? World->GroundHeightAt(At) : At.Z;
	Pawn->PlaceAt(FVector(At.X, At.Y, GroundZ), 90.0);
}

void ADTGGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bStarted && Game)
	{
		Game->TickSim(DeltaSeconds);
	}
}
