// A check that nobody can shove a car by walking into it.
//
// With -DTGPushCheck on the command line, the game waits for the traffic director to put cars on the
// street, stands the player a stride off the nearest standing car's side, walks them into it for a few
// seconds, then logs how far the car moved and where the player ended up, and quits. A car must not move,
// and the player must stop at its side rather than pass through it. Before the collision fix of 20 October
// the car backed away from the player and the player walked through the space it left.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DTGPushCheck.generated.h"

class ADTGVehicle;
class APawn;

UCLASS()
class DUSTTOGLORY_API ADTGPushCheck : public AActor
{
	GENERATED_BODY()

public:
	ADTGPushCheck();

	/** Whether -DTGPushCheck is on the command line. */
	static bool IsRequested();

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<ADTGVehicle> Car;

	UPROPERTY()
	TObjectPtr<APawn> Walker;

	FVector CarStart = FVector::ZeroVector;
	/** Out of the car's side, towards where the player started; they walk back along it. */
	FVector Side = FVector::ZeroVector;
	double HalfWidthM = 1.0;
	double StartedAt = -1.0;
	/** How far the car went by itself while the player only stood by it. */
	double DriftM = 0.0;
	bool bWalking = false;
};
