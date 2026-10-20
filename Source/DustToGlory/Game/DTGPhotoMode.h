// Photo mode: a fixed tour of viewpoints, a screenshot at each, then quit.
//
// Launched with -DTGPhoto. The viewpoints are worked out from the simulation's own map -- a street in the
// starting town, the town from above, a highway, a beach, the starter car -- so they stay meaningful as
// the world changes, and a visual change can be checked by looking at it rather than by reasoning about
// it. Shots go to Saved/Photos/<name>.png. While it runs the player is unpossessed and hidden, so the
// terrain streams around the camera instead of around the player.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DTGPhotoMode.generated.h"

class UDTGGameSubsystem;

/** One viewpoint: a name for the file, where the camera stands, and which way it looks. */
struct FDTGPhotoShot
{
	FString Name;
	FVector At = FVector::ZeroVector;
	FRotator Facing = FRotator::ZeroRotator;
	/** Minutes past midnight to set the game clock to first, or -1 to leave it. */
	double ClockMinutes = -1.0;
};

UCLASS()
class DUSTTOGLORY_API ADTGPhotoMode : public AActor
{
	GENERATED_BODY()

public:
	ADTGPhotoMode();

	/** True when the command line asks for photo mode (-DTGPhoto). */
	static bool IsRequested();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** Works out the tour from the simulation's map. Viewpoints that cannot be found are left out. */
	void PlanShots();
	/** Moves the camera to a shot and makes sure there is ground under it. */
	void Frame(const FDTGPhotoShot& Shot);

	TArray<FDTGPhotoShot> Shots;
	int32 Current = INDEX_NONE;
	double NextAt = 0.0;
	bool bCaptured = false;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;
};
