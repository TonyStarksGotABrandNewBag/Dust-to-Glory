// The bridge between the engine and the simulation core.
//
// One GameState lives here for the whole session: the engine layer never owns game data, it reads this and
// calls into it. Content JSON is loaded from the staged Data/ folder, the clock is advanced every frame, and
// saves are written next to the project. Everything in Sim/ stays free of Unreal types; everything in Game/
// goes through this subsystem.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Sim/GameState.h"

#include "DTGGameSubsystem.generated.h"

/** Centimetres per simulation metre: the sim works in metres, Unreal in centimetres. */
static constexpr double DTGMetresToUnreal = 100.0;

/** Broadcast whenever the simulation posts a notification the player should see. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDTGNotice, const FString&, Text);

UCLASS()
class DUSTTOGLORY_API UDTGGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The simulation. Game-side actors read it and call its actions; nothing else owns game data. */
	dtg::sim::GameState& Sim() { return State; }
	const dtg::sim::GameState& Sim() const { return State; }
	bool IsContentLoaded() const { return bContentLoaded; }

	/** Starts a fresh game and returns false if content failed to load. */
	bool StartNewGame(const FString& PlayerName, int64 Seed = 0);

	/** The state file in use ("state.json"), and the others found next to it. */
	const FString& StateFile() const { return CurrentState; }
	const TArray<FString>& AvailableStates() const;
	/** Loads a different state and starts a new game in it; a save belongs to the state it was made in. */
	bool SwitchState(const FString& FileName, const FString& PlayerName);

	/** Mods that replaced one or more data files, in the order they were applied. */
	const TArray<FString>& LoadedMods() const { return Mods; }
	bool SaveGame(const FString& SlotName = TEXT("dust-to-glory"));
	bool LoadGame(const FString& SlotName = TEXT("dust-to-glory"));
	static FString SavePath(const FString& SlotName);

	/** Advances the simulation clock and drains notifications. Called by the game mode each frame. */
	void TickSim(float DeltaSeconds);

	/** Notifications the simulation raised since the last call (also broadcast through OnNotice). */
	UPROPERTY(BlueprintAssignable)
	FDTGNotice OnNotice;

	/** Minutes of game time per real second (the sim's own clock scale is applied inside GameState). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dust to Glory")
	float TimeScale = 1.0f;

	/** Last tick's report: deliveries, messages, elections, power surges. */
	const dtg::sim::TickReport& LastReport() const { return Report; }

	/** World-space (Unreal) location of a simulation point on the ground. */
	FVector ToUnreal(double SimX, double SimY, double SimZ) const
	{
		return FVector(SimX * DTGMetresToUnreal, SimY * DTGMetresToUnreal, SimZ * DTGMetresToUnreal);
	}
	FVector2D ToSim(const FVector& World) const
	{
		return FVector2D(World.X / DTGMetresToUnreal, World.Y / DTGMetresToUnreal);
	}

private:
	bool LoadContent(const FString& StateFileName);
	static FString DataDir();
	static FString ModsDir();
	/** A data file's contents, from the last mod that replaces it or from the shipped data. */
	std::string ReadData(const FString& Name);

	dtg::sim::GameState State;
	dtg::sim::TickReport Report;
	bool bContentLoaded = false;
	FString CurrentState = TEXT("state.json");
	mutable TArray<FString> StateList;
	TArray<FString> Mods;
};
