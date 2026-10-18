#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class APlayerController;
class UGameInstance;
class FTimerManager;

class UWorld : public UObject
{
public:
	double GetTimeSeconds() const { return 0.0; }
	double GetRealTimeSeconds() const { return 0.0; }
	float GetDeltaSeconds() const { return 0.0f; }
	APlayerController* GetFirstPlayerController() const { return nullptr; }
	UGameInstance* GetGameInstance() const { return nullptr; }
	FTimerManager& GetTimerManager() const;

	bool LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
		ECollisionChannel Channel, const FCollisionQueryParams& Params = FCollisionQueryParams()) const
	{
		(void)OutHit;
		(void)Start;
		(void)End;
		(void)Channel;
		(void)Params;
		return false;
	}
	bool SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
		const FQuat& Rotation, ECollisionChannel Channel, const FCollisionShape& Shape,
		const FCollisionQueryParams& Params = FCollisionQueryParams()) const
	{
		(void)OutHit; (void)Start; (void)End; (void)Rotation; (void)Channel; (void)Shape; (void)Params;
		return false;
	}

	template <typename T>
	T* SpawnActor(UClass* Class, const FVector* Location = nullptr, const FRotator* Rotation = nullptr,
		const FActorSpawnParameters& Params = FActorSpawnParameters())
	{
		(void)Class; (void)Location; (void)Rotation; (void)Params;
		return nullptr;
	}
	template <typename T>
	T* SpawnActor(UClass* Class, const FTransform& Transform,
		const FActorSpawnParameters& Params = FActorSpawnParameters())
	{
		(void)Class; (void)Transform; (void)Params;
		return nullptr;
	}
	template <typename T>
	T* SpawnActor(const FActorSpawnParameters& Params = FActorSpawnParameters())
	{
		(void)Params;
		return nullptr;
	}
};
