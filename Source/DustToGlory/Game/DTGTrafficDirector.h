// Everyone else on the road and the sidewalk.
//
// Traffic and pedestrians are spawned around the player and recycled when they fall behind, so a whole state
// can be populated without a whole state's worth of actors. Traffic cars are the same ADTGVehicle the player
// drives, steered by a simple follow-the-road-graph driver, so a cab corners on the same tires the player's
// car does. Pedestrians carry the simulation's NpcMinds: what they have seen of the player is remembered, and
// enough of it wakes them up. Police are traffic that comes looking when the player is wanted.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DTGTrafficDirector.generated.h"

class ADTGVehicle;
class UDTGGameSubsystem;

/** One AI car and where it is going. */
USTRUCT()
struct FDTGDriver
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ADTGVehicle> Car = nullptr;

	int32 FromNode = 0;
	int32 ToNode = 0;
	bool bPolice = false;
	double Patience = 1.0;
};

/** A walking NPC tied to a mind in the simulation. */
USTRUCT()
struct FDTGWalker
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	FString MindId;
	FVector Target = FVector::ZeroVector;
	double NextThink = 0.0;
	/** The block this person lives around, in simulation metres: they stroll its pavement between errands. */
	FVector2D BlockMin = FVector2D::ZeroVector;
	FVector2D BlockMax = FVector2D::ZeroVector;
	/** The corner to round on the way to Target, when a stroll turns onto the next side of the block. */
	FVector Via = FVector::ZeroVector;
	bool bVia = false;
};

UCLASS()
class DUSTTOGLORY_API ADTGTrafficDirector : public AActor
{
	GENERATED_BODY()

public:
	ADTGTrafficDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Cars kept alive around the player. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Traffic")
	int32 TrafficCars = 24;

	/** Pedestrians kept alive around the player (scaled down while an outbreak keeps people home). */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Traffic")
	int32 Pedestrians = 40;

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Traffic")
	float SpawnRadiusM = 220.0f;

	/** How close two people have to be for one to count as somebody the other knows. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Traffic")
	float NeighbourRangeM = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Traffic")
	float DespawnRadiusM = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Traffic")
	int32 MaxParkedCars = 14;

private:
	void UpdateTraffic(const FVector& Around, float DeltaSeconds);
	void UpdatePolice(const FVector& Around);
	/** Walkers think on world time rather than frame delta, so this needs no DeltaSeconds. */
	void UpdatePedestrians(const FVector& Around);
	void UpdateParkedCars(const FVector& Around);

public:
	/** Forgets and destroys everything it has spawned: the world underneath has been replaced. */
	void Reset();

private:
	void DriveToward(FDTGDriver& Driver, const FVector& Target, float DeltaSeconds);
	ADTGVehicle* SpawnCar(const FVector& At, double YawDegrees, bool bPolice);
	/** A node on the road graph near a point, preferring one the player can't already see. */
	int32 PickNode(const FVector& Around, bool bFar) const;
	FVector NodeLocation(int32 NodeId) const;
	/** True when a traffic car is already standing near this point, so another must not spawn into it. */
	bool IsOccupied(const FVector& At) const;

	UPROPERTY()
	TArray<FDTGDriver> Drivers;

	UPROPERTY()
	TArray<FDTGWalker> Walkers;

	/** Cars sitting outside houses, each one an offer waiting to be made. */
	UPROPERTY()
	TArray<TObjectPtr<ADTGVehicle>> Parked;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	double NextSpawn = 0.0;
	double NextPoliceSpawn = 0.0;
};
