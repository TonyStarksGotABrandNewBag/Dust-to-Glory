#include "Game/DTGGameMode.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#include "Game/DTGCharacter.h"
#include "Game/DTGEventDirector.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGHud.h"
#include "Game/DTGPlayerController.h"
#include "Game/DTGSky.h"
#include "Game/DTGTrafficDirector.h"
#include "Game/DTGWeatherFx.h"
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

void ADTGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Everything the player's first frame depends on happens here rather than in StartPlay, because
	// StartPlay runs *after* the pawn has been spawned. The engine's order is InitGame, then the actors
	// are initialised, then Login and ChoosePlayerStart, and only then BeginPlay and StartPlay. Deciding
	// where to put the player in StartPlay means ChoosePlayerStart has already read an empty SpawnPoint
	// and dropped them at the world origin, which is sea level and generally under the ground.
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

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Builder = GetWorld()->SpawnActor<ADTGWorldBuilder>(ADTGWorldBuilder::StaticClass(), FTransform::Identity, Params);
	if (Builder)
	{
		// And the ground immediately under that point is built now, synchronously. The streamer fills
		// the rest a couple of chunks a frame, which takes the best part of a second -- long enough for
		// a player spawned a metre above nothing to fall through the world before it arrives.
		Builder->EnsureGroundAt(SpawnPoint);
	}
}

void ADTGGameMode::StartPlay()
{
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
	// Rain, which reads the same weather the sky and the tires do.
	WeatherFx = GetWorld()->SpawnActor<ADTGWeatherFx>(ADTGWeatherFx::StaticClass(), FTransform::Identity, Params);
}

void ADTGGameMode::SpawnStarterCar()
{
	PlaceOwnedCar(FString(), SpawnPoint);
}

ADTGVehicle* ADTGGameMode::PlaceOwnedCar(const FString& Uid, const FVector& Near)
{
	if (!Game || !Game->IsContentLoaded() || Game->Sim().Garage.empty())
	{
		return nullptr;
	}
	// The one asked for, or the first they own.
	const OwnedVehicle* Car = &Game->Sim().Garage.front();
	if (!Uid.IsEmpty())
	{
		const std::string Want = TCHAR_TO_UTF8(*Uid);
		for (const OwnedVehicle& Owned : Game->Sim().Garage)
		{
			if (Owned.Uid == Want)
			{
				Car = &Owned;
				break;
			}
		}
	}
	// Whatever of theirs is standing here already makes way: this is the same car being fetched, or
	// the one it replaces, and two of the player's cars on one kerb helps nobody.
	const FString Keep = UTF8_TO_TCHAR(Car->Uid.c_str());
	for (TActorIterator<ADTGVehicle> It(GetWorld()); It; ++It)
	{
		if (!It->OwnedUid().IsEmpty() && It->OwnedUid() != Keep && !It->bPlayerDriven)
		{
			It->Destroy();
		}
	}
	ADTGVehicle* Existing = nullptr;
	for (TActorIterator<ADTGVehicle> It(GetWorld()); It; ++It)
	{
		if (It->OwnedUid() == Keep)
		{
			Existing = *It;
			break;
		}
	}
	// At the kerb, a few metres from the door.
	const FVector At = Near + FVector(500.0f, 0.0f, 0.0f);
	const double GroundZ = Builder ? Builder->GroundHeightAt(At) : At.Z;
	if (Existing)
	{
		Existing->PlaceAt(FVector(At.X, At.Y, GroundZ), 90.0);
		return Existing;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADTGVehicle* Pawn = GetWorld()->SpawnActor<ADTGVehicle>(ADTGVehicle::StaticClass(), FTransform::Identity, Params);
	if (!Pawn)
	{
		return nullptr;
	}
	Pawn->ConfigureFromOwned(*Car);
	Pawn->PlaceAt(FVector(At.X, At.Y, GroundZ), 90.0);
	return Pawn;
}

void ADTGGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bStarted && Game)
	{
		Game->TickSim(DeltaSeconds);
	}
}
