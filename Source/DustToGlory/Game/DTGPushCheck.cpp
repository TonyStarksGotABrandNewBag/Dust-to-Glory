#include "Game/DTGPushCheck.h"

#include "DustToGlory.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "Game/DTGGameSubsystem.h"
#include "Game/DTGVehicle.h"

namespace detail_pushcheck
{
	/** For the traffic director to have put cars on the street. */
	constexpr double WaitSeconds = 4.0;
	/** Standing beside the car first, to see whether it creeps on its own before anyone touches it. */
	constexpr double IdleSeconds = 4.0;
	/** How long to keep walking into the car. */
	constexpr double WalkSeconds = 4.0;
	/** Off the car's side to start from. */
	constexpr double StrideCm = 150.0;
	/** The player's capsule radius: standing against the side, their centre is this far off it. */
	constexpr double CapsuleCm = 38.0;
}

ADTGPushCheck::ADTGPushCheck()
{
	PrimaryActorTick.bCanEverTick = true;
}

bool ADTGPushCheck::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("DTGPushCheck"));
}

void ADTGPushCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Car)
	{
		if (Now < detail_pushcheck::WaitSeconds)
		{
			return;
		}
		APlayerController* const Controller = GetWorld()->GetFirstPlayerController();
		Walker = Controller ? Controller->GetPawn() : nullptr;
		if (!Walker)
		{
			UE_LOG(LogDTG, Error, TEXT("Push check: no player pawn to walk."));
			FPlatformMisc::RequestExit(false);
			return;
		}
		// The nearest car that is standing still: a parked one, or traffic waiting at a junction.
		double Nearest = 1e18;
		for (TActorIterator<ADTGVehicle> It(GetWorld()); It; ++It)
		{
			const dtg::sim::Vec3 Velocity = It->Chassis().State.Velocity;
			if (Velocity.X * Velocity.X + Velocity.Y * Velocity.Y > 0.01)
			{
				continue;
			}
			const double Distance = FVector::DistSquared(It->GetActorLocation(), Walker->GetActorLocation());
			if (Distance < Nearest)
			{
				Nearest = Distance;
				Car = *It;
			}
		}
		if (!Car)
		{
			if (Now < detail_pushcheck::WaitSeconds + 20.0)
			{
				return;
			}
			UE_LOG(LogDTG, Error, TEXT("Push check: no standing car within 24 seconds."));
			FPlatformMisc::RequestExit(false);
			return;
		}
		const UBoxComponent* const Body = Cast<UBoxComponent>(Car->GetRootComponent());
		const FVector Extent = Body ? Body->GetScaledBoxExtent() : FVector(250.0, 100.0, 70.0);
		HalfWidthM = Extent.Y / DTGMetresToUnreal;
		Side = Car->GetActorRightVector().GetSafeNormal2D();
		CarStart = Car->GetActorLocation();
		const FVector Stand = CarStart + Side * (Extent.Y + detail_pushcheck::StrideCm) + FVector(0.0, 0.0, 100.0);
		Walker->SetActorLocation(Stand, /*bSweep*/ false);
		StartedAt = Now;
		UE_LOG(LogDTG, Log, TEXT("Push check: standing by %s (%.1f m wide), %.1f m off its side."), *Car->GetName(), 2.0 * HalfWidthM,
			detail_pushcheck::StrideCm / DTGMetresToUnreal);
		return;
	}
	if (!bWalking)
	{
		if (Now - StartedAt < detail_pushcheck::IdleSeconds)
		{
			return;
		}
		// The baseline: how far the car went with nobody touching it.
		DriftM = FVector::Dist2D(Car->GetActorLocation(), CarStart) / DTGMetresToUnreal;
		CarStart = Car->GetActorLocation();
		StartedAt = Now;
		bWalking = true;
		UE_LOG(LogDTG, Log, TEXT("Push check: the car drifted %.2f m on its own in %.0f s; now walking into it."), DriftM, detail_pushcheck::IdleSeconds);
		return;
	}
	if (Now - StartedAt < detail_pushcheck::WalkSeconds)
	{
		Walker->AddMovementInput(-Side, 1.0f);
		return;
	}
	const double CarMovedM = FVector::Dist2D(Car->GetActorLocation(), CarStart) / DTGMetresToUnreal;
	// Where the player stands off the car's centre line, along the side they walked in from: at the side
	// is the half width plus their capsule; less is inside the car; negative is out the other side.
	const double OffM = FVector::DotProduct(Walker->GetActorLocation() - Car->GetActorLocation(), Side) / DTGMetresToUnreal;
	const double AtSideM = HalfWidthM + detail_pushcheck::CapsuleCm / DTGMetresToUnreal;
	const bool bStopped = OffM > AtSideM - 0.15;
	// Moved no more than it drifted by itself, and the player stopped outside it.
	const bool bPass = CarMovedM < DriftM + 0.05 && bStopped;
	UE_LOG(LogDTG, Log, TEXT("Push check: %s. The car moved %.2f m while walked into (%.2f m on its own before); the player stands %.2f m off its centre line (its side is at %.2f m): %s."),
		bPass ? TEXT("PASS") : TEXT("FAIL"), CarMovedM, DriftM, OffM, AtSideM, bStopped ? TEXT("stopped at the side") : (OffM < 0.0 ? TEXT("walked through it") : TEXT("inside it")));
	FPlatformMisc::RequestExit(false);
}
