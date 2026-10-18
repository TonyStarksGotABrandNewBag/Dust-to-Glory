// Races, derbies and the people with powers.
//
// The venues are already in the world (StatePlan placed them), so this runs what happens at them: a field of
// AI cars on the same physics as the player's, a flag, laps or rounds, and a finishing order handed back to
// the simulation, which pays the prize and moves reputation. It also keeps a handful of heroes and villains
// roaming -- the simulation generates who they are and what they can do; this gives them somewhere to be.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sim/CityLayout.h"
#include "Sim/Events.h"
#include "Sim/Powers.h"

#include "DTGEventDirector.generated.h"

class ADTGVehicle;
class UDTGGameSubsystem;

UENUM()
enum class EDTGRaceState : uint8
{
	Idle,
	Staging,
	Countdown,
	Running,
	Finished
};

/** One car in a running event. */
USTRUCT()
struct FDTGRacer
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ADTGVehicle> Car = nullptr;

	bool bPlayer = false;
	int32 Checkpoint = 0;
	int32 Lap = 0;
	double Finished = -1.0;
	double Damage = 0.0;
	double LastSpeed = 0.0;   // to notice a hit: speed lost suddenly is speed lost to something
	FString Name;
};

UCLASS()
class DUSTTOGLORY_API ADTGEventDirector : public AActor
{
	GENERATED_BODY()

public:
	ADTGEventDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Starts an event at its venue. The entry fee is taken by the simulation. */
	bool StartEvent(dtg::sim::EventKind Kind, int32 Tier);
	void AbandonEvent();
	/** Forgets and destroys everything it has spawned: the world underneath has been replaced. */
	void Reset();
	EDTGRaceState RaceState() const { return State; }
	dtg::sim::EventKind CurrentKind() const { return Kind; }
	/** Player's position in the field right now (1 = leading). */
	int32 PlayerPosition() const;
	int32 FieldSize() const { return Racers.Num(); }
	int32 LapsToRun() const { return Laps; }
	int32 PlayerLap() const;
	/** The last event's result, for the HUD to show after the flag. */
	int32 LastResultPosition() const { return LastPosition; }
	int32 LastResultField() const { return LastField; }

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Events")
	int32 DefaultField = 8;

	/** However it is going, an event ends by here. Nothing may strand the player in one. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Events")
	float EventLimitSeconds = 420.0f;

private:
	void Stage(const dtg::sim::VenuePlan& Venue, int32 Tier);
	void UpdateRace(float DeltaSeconds);
	void DriveRacer(FDTGRacer& Racer, float DeltaSeconds);
	void Finish();
	void UpdateSuperhumans(const FVector& Around);
	FVector CheckpointLocation(int32 Index) const;

	UPROPERTY()
	TArray<FDTGRacer> Racers;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Superhumans;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	TArray<FVector> Checkpoints;
	dtg::sim::EventKind Kind = dtg::sim::EventKind::DragRace;
	EDTGRaceState State = EDTGRaceState::Idle;
	int32 Tier = 1;
	int32 Laps = 1;
	int32 LastPosition = 0;
	int32 LastField = 0;
	double StateUntil = 0.0;
	double StartedAt = 0.0;
};
