// Inside the buildings.
//
// Every building in the plan has a door, a role and a service list, so every one of them can be walked
// into. Rather than hollowing out 1,160 shells, an interior is built on demand in a pocket of space well
// below the world and the player is moved into it; stepping back through the door puts them where they
// were. A home is furnished with the furniture the player actually bought.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DTGInterior.generated.h"

class UBoxComponent;
class UDTGGameSubsystem;
class UProceduralMeshComponent;

UCLASS()
class DUSTTOGLORY_API ADTGInterior : public AActor
{
	GENERATED_BODY()

public:
	ADTGInterior();

	virtual void BeginPlay() override;

	/**
	 * Builds the inside of a building and returns where the player should stand. Reuses this actor, so
	 * walking in and out of shops doesn't leave a trail of rooms behind.
	 */
	FVector Open(int32 BuildingId);
	void Close();
	int32 CurrentBuilding() const { return BuildingId; }
	/** Where the door is, so the player can be sent back out through it. */
	FVector ExitPoint() const { return Exit; }

	/** Rooms are built this far below the world, where nothing else lives. */
	static constexpr double PocketZ = -500000.0;

private:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Room;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	int32 BuildingId = -1;
	FVector Exit = FVector::ZeroVector;
};
