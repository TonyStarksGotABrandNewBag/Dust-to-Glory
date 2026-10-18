// A drivable car: the simulation's chassis given a body in the world.
//
// ChassisSim does the physics (tires, suspension, drivetrain, aero) at a fixed 240 Hz; this pawn feeds it
// where the ground is -- line traces against the streamed terrain and anything else solid -- then copies the
// resulting pose onto the actor. Unreal's own physics never touches the car, so the handling is identical on
// every machine and in the unit tests. Collisions come back the other way: a body sweep turns a hit into an
// impulse the chassis resolves.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "Sim/ChassisSim.h"
#include "Sim/Garage.h"

#include "DTGVehicle.generated.h"

class ADTGWorldBuilder;
class UBoxComponent;
class UDTGEngineAudio;
class UDTGGameSubsystem;
class UStaticMeshComponent;

UCLASS()
class DUSTTOGLORY_API ADTGVehicle : public APawn
{
	GENERATED_BODY()

public:
	ADTGVehicle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;

	/** Builds this pawn from a car in the player's garage (or any model id for traffic). */
	void ConfigureFromOwned(const dtg::sim::OwnedVehicle& Car);
	void ConfigureFromModel(const FString& ModelId);
	/** Drops the car on the ground at a spot, pointing somewhere, at rest. */
	void PlaceAt(const FVector& World, double YawDegrees);

	/** Driver controls, set by the player controller or by traffic AI. */
	void SetDriverInput(const dtg::sim::DriverInput& Input) { Controls = Input; }
	const dtg::sim::DriverInput& DriverInput() const { return Controls; }
	/** Manual shifts: the gearbox only changes when asked. */
	void ShiftUp() { PendingShift = 1; }
	void ShiftDown() { PendingShift = -1; }

	const dtg::sim::ChassisSim& Chassis() const { return Sim; }
	dtg::sim::ChassisSim& Chassis() { return Sim; }
	double SpeedKph() const { return Sim.SpeedKph(); }
	double EngineRpm() const;
	int CurrentGear() const;
	const FString& ModelId() const { return Model; }
	/** The garage uid this pawn was built from, empty for traffic. */
	const FString& OwnedUid() const { return Uid; }
	/** When this is a car parked at someone's house, the simulation's id for that spot ("home-42"). */
	const FString& ParkedCarId() const { return ParkedId; }
	void SetParkedCarId(const FString& Id) { ParkedId = Id; }

	/** True while the car is in water deep enough to drown it. */
	bool IsSwamped() const { return bSwamped; }

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Vehicle")
	bool bPlayerDriven = false;

	/** Physics sub-step rate; distant traffic runs slower to save ray casts. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Vehicle")
	float StepHz = 240.0f;

private:
	void ApplyVisuals();
	void StepPhysics(double Dt);
	void ResolveCollisions(const FVector& PreviousLocation);

	UPROPERTY()
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	UPROPERTY()
	TObjectPtr<UDTGEngineAudio> Audio;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TObjectPtr<ADTGWorldBuilder> World;

	dtg::sim::ChassisSim Sim;
	dtg::sim::VehicleSetup Setup;
	dtg::sim::DriverInput Controls;
	FString Model;
	FString Uid;
	FString ParkedId;
	int32 PendingShift = 0;
	bool bSwamped = false;
	double LengthM = 4.5;
	double WidthM = 1.8;
	double HeightM = 1.4;
};
