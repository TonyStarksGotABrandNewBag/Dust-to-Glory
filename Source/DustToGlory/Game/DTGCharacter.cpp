#include "Game/DTGCharacter.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Game/DTGGameSubsystem.h"
#include "Game/DTGInterior.h"
#include "Game/DTGVehicle.h"

using namespace dtg::sim;

ADTGCharacter::ADTGCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.0f, 90.0f);
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->MaxWalkSpeed = 420.0f;
		Movement->JumpZVelocity = 480.0f;
		Movement->AirControl = 0.25f;
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 320.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.0f, 40.0f, 60.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
}

void ADTGCharacter::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
}

void ADTGCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFocus();
}

void ADTGCharacter::UpdateFocus()
{
	Current = FDTGInteraction();
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	if (InsideBuildingId >= 0)
	{
		// Indoors there is one thing in front of you: the door you came in through.
		if (const BuildingPlan* Inside = Game->Sim().City.FindBuilding(InsideBuildingId))
		{
			Current.BuildingId = InsideBuildingId;
			Current.Title = UTF8_TO_TCHAR(Inside->Name.c_str());
			const bool bAtDoor = Interior && FVector::Dist(GetActorLocation(), Interior->ExitPoint()) < 300.0;
			Current.Prompt = bAtDoor ? TEXT("Step outside") : TEXT("Look around");
		}
		return;
	}
	const CityPlan& City = Game->Sim().City;
	const FVector2D Here = Game->ToSim(GetActorLocation());

	// Every building has a door and a service list, so any of them can be walked into.
	const BuildingPlan* Nearest = nullptr;
	double Best = ReachM * ReachM;
	for (const BuildingPlan& Building : City.Buildings)
	{
		const double Dx = Building.DoorX - Here.X;
		const double Dy = Building.DoorY - Here.Y;
		const double Distance = Dx * Dx + Dy * Dy;
		if (Distance < Best)
		{
			Best = Distance;
			Nearest = &Building;
		}
	}
	if (Nearest)
	{
		Current.BuildingId = Nearest->Id;
		Current.Title = UTF8_TO_TCHAR(Nearest->Name.c_str());
		const std::vector<std::string> Services = ServicesForRole(Nearest->Role);
		Current.Prompt = Services.empty() ? TEXT("Enter") : UTF8_TO_TCHAR(Services.front().c_str());
		return;
	}

	// A person, if one is closer than any car. Their mind id is on the actor as a tag.
	ACharacter* NearestPerson = nullptr;
	FString NearestMind;
	double PersonBest = FMath::Square(ReachM * DTGMetresToUnreal);
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->IsPlayerControlled())
		{
			continue;
		}
		const double Distance = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (Distance >= PersonBest)
		{
			continue;
		}
		for (const FName& Tag : It->Tags)
		{
			const FString Name = Tag.ToString();
			if (Name.StartsWith(TEXT("mind:")))
			{
				PersonBest = Distance;
				NearestPerson = *It;
				NearestMind = Name.RightChop(5);
			}
		}
	}
	if (NearestPerson)
	{
		const NpcMind& Mind = Game->Sim().Npcs.GetOrCreate(TCHAR_TO_UTF8(*NearestMind));
		Current.Actor = NearestPerson;
		Current.MindId = NearestMind;
		Current.Title = UTF8_TO_TCHAR(Mind.Name.c_str());
		Current.Prompt = TEXT("Talk");
		return;
	}

	// Otherwise: a car parked at a house. Any of them can be asked after.
	ADTGVehicle* NearestCar = nullptr;
	double CarBest = FMath::Square(ReachM * DTGMetresToUnreal);
	for (TActorIterator<ADTGVehicle> It(GetWorld()); It; ++It)
	{
		if (*It == Driving)
		{
			continue;
		}
		const double Distance = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (Distance < CarBest)
		{
			CarBest = Distance;
			NearestCar = *It;
		}
	}
	if (NearestCar)
	{
		Current.Actor = NearestCar;
		Current.CarId = NearestCar->ParkedCarId();
		const VehicleModel* Model = Game->Sim().Vehicles.FindModel(TCHAR_TO_UTF8(*NearestCar->ModelId()));
		Current.Title = Model ? UTF8_TO_TCHAR(Model->DisplayName.c_str()) : NearestCar->ModelId();
		Current.Prompt = NearestCar->OwnedUid().IsEmpty() ? TEXT("Make an offer") : TEXT("Get in");
	}
}

ADTGVehicle* ADTGCharacter::ToggleVehicle()
{
	if (Driving)
	{
		// Step out beside the driver's door.
		const FVector Side = Driving->GetActorRightVector() * -140.0f;
		SetActorLocation(Driving->GetActorLocation() + Side + FVector(0.0f, 0.0f, 100.0f));
		SetActorHiddenInGame(false);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Driving->bPlayerDriven = false;
		ADTGVehicle* Left = Driving;
		Driving = nullptr;
		return Left;
	}
	// Only a car the player owns is simply got into; anyone else's is an offer (or a theft), not a free ride.
	ADTGVehicle* Target = Cast<ADTGVehicle>(Current.Actor);
	if (!Target || Target->OwnedUid().IsEmpty())
	{
		return nullptr;
	}
	Driving = Target;
	Driving->bPlayerDriven = true;
	SetActorHiddenInGame(true);
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	return Driving;
}

bool ADTGCharacter::EnterBuilding(int32 TargetBuildingId)
{
	if (!Game || !Game->IsContentLoaded() || InsideBuildingId >= 0 || Driving)
	{
		return false;
	}
	if (!Interior)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Interior = GetWorld()->SpawnActor<ADTGInterior>(ADTGInterior::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, ADTGInterior::PocketZ)), Params);
	}
	if (!Interior)
	{
		return false;
	}
	const FVector Stand = Interior->Open(TargetBuildingId);
	if (Stand.IsNearlyZero())
	{
		return false;
	}
	OutsideLocation = GetActorLocation();
	InsideBuildingId = TargetBuildingId;
	SetActorLocation(Stand, /*bSweep*/ false);
	return true;
}

bool ADTGCharacter::LeaveBuilding()
{
	if (InsideBuildingId < 0)
	{
		return false;
	}
	InsideBuildingId = -1;
	if (Interior)
	{
		Interior->Close();
	}
	SetActorLocation(OutsideLocation, /*bSweep*/ false);
	return true;
}

void ADTGCharacter::UsePower(int32 Slot)
{
	if (!Game || !Game->IsContentLoaded() || Slot < 0 || Slot >= PowerSet::SlotCount)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now < NextPowerTime[Slot])
	{
		return;
	}
	// Anyone close enough to see it is a witness, which is how NPCs start to notice the world is odd. The
	// traffic director tags each pedestrian with the id of the mind it is driving, so the same passer-by
	// keeps what they saw.
	std::vector<std::string> Witnesses;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->IsPlayerControlled())
		{
			continue;
		}
		if (FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) >= FMath::Square(3000.0f))
		{
			continue;
		}
		for (const FName& Tag : It->Tags)
		{
			const FString Name = Tag.ToString();
			if (Name.StartsWith(TEXT("mind:")))
			{
				Witnesses.push_back(TCHAR_TO_UTF8(*Name.RightChop(5)));
			}
		}
	}
	const GameState::PowerUse Used = Game->Sim().UsePower(Slot, Witnesses, /*bInPublic*/ true);
	if (!Used.bOk)
	{
		return;
	}
	NextPowerTime[Slot] = Now + FMath::Max(0.2, Used.Power.CooldownS);

	// What the power does in the world. The simulation owns the numbers; this is the body of it.
	const FVector Forward = GetActorForwardVector();
	switch (Used.Power.Def->Effect)
	{
	case PowerEffect::Flight:
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->MaxFlySpeed = static_cast<float>(600.0 * Used.Power.Potency);
		break;
	case PowerEffect::SuperSpeed:
		GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(420.0 * (1.0 + Used.Power.Potency));
		break;
	case PowerEffect::SuperStrength:
	case PowerEffect::Telekinesis:
	{
		// Shove whatever is in front: cars included, through the chassis simulation.
		const FVector Centre = GetActorLocation() + Forward * 150.0f;
		for (TActorIterator<ADTGVehicle> It(GetWorld()); It; ++It)
		{
			if (FVector::DistSquared(It->GetActorLocation(), Centre) < FMath::Square(Used.Power.RangeM * DTGMetresToUnreal))
			{
				const FVector Push = (It->GetActorLocation() - GetActorLocation()).GetSafeNormal() + FVector(0.0f, 0.0f, 0.3f);
				const Vec3 Impulse(Push.X * 4000.0 * Used.Power.Potency, Push.Y * 4000.0 * Used.Power.Potency, Push.Z * 4000.0 * Used.Power.Potency);
				const FVector At = It->GetActorLocation();
				It->Chassis().ApplyImpulse(Vec3(At.X / DTGMetresToUnreal, At.Y / DTGMetresToUnreal, At.Z / DTGMetresToUnreal), Impulse);
			}
		}
		break;
	}
	case PowerEffect::Teleport:
	{
		const FVector Target = GetActorLocation() + Forward * static_cast<float>(Used.Power.RangeM * DTGMetresToUnreal);
		SetActorLocation(Target, /*bSweep*/ true);
		break;
	}
	case PowerEffect::SonicBoom:
	case PowerEffect::EnergyBlast:
	case PowerEffect::Pyrokinesis:
	case PowerEffect::Cryokinesis:
	case PowerEffect::Electrokinesis:
	{
		// A line of force: what it hits takes it.
		FHitResult Hit;
		const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		const FVector End = Start + Forward * static_cast<float>(Used.Power.RangeM * DTGMetresToUnreal);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DTGPower), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			if (ADTGVehicle* Car = Cast<ADTGVehicle>(Hit.GetActor()))
			{
				const Vec3 Impulse(Forward.X * 3000.0 * Used.Power.Potency, Forward.Y * 3000.0 * Used.Power.Potency, 800.0);
				Car->Chassis().ApplyImpulse(Vec3(Hit.ImpactPoint.X / DTGMetresToUnreal, Hit.ImpactPoint.Y / DTGMetresToUnreal,
										   Hit.ImpactPoint.Z / DTGMetresToUnreal), Impulse);
			}
		}
		break;
	}
	case PowerEffect::Invisibility:
	{
		SetActorHiddenInGame(true);
		// Visible again when the effect runs out.
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { SetActorHiddenInGame(false); }),
			static_cast<float>(FMath::Max(0.5, Used.Power.DurationS)), false);
		break;
	}
	default:
		break;
	}
}
