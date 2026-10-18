// The player on foot: walking, interacting with any building, and using the three chosen powers.
//
// Interaction is deliberately uniform -- every building in the plan has services (ServicesForRole), so walking
// up to any door and pressing use opens that building's menu rather than only the handful that were special
// cased. Powers read their numbers from the simulation's PowerSet, so a mutation that changed a power's range
// changes what happens here too.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "Sim/CityLayout.h"
#include "Sim/Powers.h"

#include "DTGCharacter.generated.h"

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

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	bool IsValid() const { return BuildingId >= 0 || !CarId.IsEmpty() || Actor != nullptr; }
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

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Interaction")
	float ReachM = 3.0f;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

private:
	void UpdateFocus();

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TObjectPtr<ADTGVehicle> Driving;

	UPROPERTY()
	FDTGInteraction Current;
	double NextPowerTime[dtg::sim::PowerSet::SlotCount] = {};
};
