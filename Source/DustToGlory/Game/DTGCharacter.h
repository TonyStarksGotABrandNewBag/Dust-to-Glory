// The player on foot: walking, interacting with any building, and using the three chosen powers.
//
// Interaction is deliberately uniform -- every building in the plan has services (ServicesForRole), so walking
// up to any door and pressing use opens that building's menu rather than only the handful that were special
// cased. Powers read their numbers from the simulation's PowerSet, so a mutation that changed a power's range
// changes what happens here too.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "Sim/Powers.h"

#include "DTGCharacter.generated.h"

class ADTGInterior;
class UDTGBody;
class ADTGVehicle;
class UCameraComponent;
class UDTGGameSubsystem;
class USpringArmComponent;

/** What the player is currently standing in front of. */
USTRUCT(BlueprintType)
struct FDTGInteraction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dust to Glory")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Dust to Glory")
	FString Prompt;

	/** Building id in the city plan, or -1. */
	int32 BuildingId = -1;

	/** Private-car id when the player is looking at someone's parked car. */
	FString CarId;

	/** NPC mind id when the player is looking at a person. */
	FString MindId;

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	bool IsValid() const { return BuildingId >= 0 || !CarId.IsEmpty() || !MindId.IsEmpty() || Actor != nullptr; }
};

UCLASS()
class DUSTTOGLORY_API ADTGCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADTGCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** What's in front of the player right now (building door, car, person). */
	const FDTGInteraction& Focus() const { return Current; }

	/** Gets in the nearest car, or out of the one being driven. Returns the vehicle, if any. */
	ADTGVehicle* ToggleVehicle();
	ADTGVehicle* DrivingNow() const { return Driving; }

	/** Uses one of the three power slots. Mutations and level-ups are reported through the subsystem. */
	void UsePower(int32 Slot);

	/** Throws a punch at whoever is in front. Powers are used through UsePower and hit the same way. */
	void Punch();
	/** Anyone in reach, as a fighter the simulation knows about. Empty when there's nobody. */
	FString EngageNearest();

	/** Steps inside a building (or back out of one). Returns true if the player moved. */
	bool EnterBuilding(int32 BuildingId);
	bool LeaveBuilding();
	bool IsIndoors() const { return InsideBuildingId >= 0; }
	UDTGBody* GetBody() const { return Body; }
	int32 IndoorBuildingId() const { return InsideBuildingId; }

	/**
	 * Where the world should be built and populated around. That is wherever the player is, except
	 * while they are indoors: interiors sit in a pocket half a million units below the map, so using
	 * the raw location there would stream the whole state out, build a ring of empty chunks around the
	 * pocket, and destroy every car and pedestrian in town -- all for stepping through a door.
	 */
	FVector InterestPoint() const { return InsideBuildingId >= 0 ? OutsideLocation : GetActorLocation(); }

	/** The same, for whoever is holding the camera: the car when driving, otherwise the player. */
	static FVector InterestPointFor(const APawn* Pawn);

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Interaction")
	float ReachM = 3.0f;

	/** How far the camera sits behind the player on foot, and behind the car when driving. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Camera")
	float WalkingBoomLength = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Camera")
	float DrivingBoomLength = 780.0f;

	/** Ordinary walking speed, which super-speed goes back to when it runs out. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Movement")
	float WalkSpeed = 420.0f;

private:
	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	void UpdateFocus();

	/** One per power effect that wears off, so using it again restarts the clock rather than racing it. */
	FTimerHandle FlightTimer;
	FTimerHandle SpeedTimer;
	FTimerHandle InvisibilityTimer;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TObjectPtr<ADTGVehicle> Driving;

	UPROPERTY()
	TObjectPtr<ADTGInterior> Interior;

	UPROPERTY()
	TObjectPtr<UDTGBody> Body;

	UPROPERTY()
	FDTGInteraction Current;
	int32 InsideBuildingId = -1;
	double NextSwing = 0.0;
	FVector OutsideLocation = FVector::ZeroVector;
	double NextPowerTime[dtg::sim::PowerSet::SlotCount] = {};
};
