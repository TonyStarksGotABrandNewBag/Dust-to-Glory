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
#include "Game/DTGMeshBuilder.h"
#include "Game/DTGBody.h"
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
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->JumpZVelocity = 480.0f;
		Movement->AirControl = 0.25f;
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = WalkingBoomLength;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.0f, 40.0f, 60.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
}

void ADTGCharacter::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	// The player is a person like any other, and walks like one.
	Body = DTGMeshBuilder::DressPerson(this, 0);
}

void ADTGCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// A car can be taken out from under the player -- a state switch, a new game, a showroom clearing
	// away its demonstrator. Driving is a UPROPERTY, so a destroyed car stays a live-looking pointer
	// and the player would be left hidden, unable to move, driving something that no longer exists.
	if (Driving && (!IsValid(Driving) || Driving->IsActorBeingDestroyed()))
	{
		Driving = nullptr;
		SetActorHiddenInGame(false);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		CameraBoom->TargetArmLength = WalkingBoomLength;
		CameraBoom->SocketOffset = FVector(0.0f, 40.0f, 60.0f);
		CameraBoom->bEnableCameraLag = false;
		CameraBoom->bEnableCameraRotationLag = false;
	}
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
	const double ReachSq = FMath::Square(ReachM * DTGMetresToUnreal);
	// All three candidates are measured and the nearest wins. Taking the building the moment one was
	// in reach meant a parked car -- which the traffic director puts three metres from a door, the same
	// as the reach -- was almost always further away than the door beside it, so "Make an offer" could
	// not be chosen at all, and neither could anyone standing on a shop front.
	double Best = ReachSq;

	// Every building has a door and a service list, so any of them can be walked into. The plan's own
	// index answers this, rather than walking a state's worth of buildings every frame.
	const BuildingPlan* NearestDoor = City.NearestBuilding(Here.X, Here.Y, ReachM);
	if (NearestDoor)
	{
		const FVector Door = Game->ToUnreal(NearestDoor->DoorX, NearestDoor->DoorY, 0.0);
		const double Flat = FVector2D::DistSquared(FVector2D(Door.X, Door.Y),
			FVector2D(GetActorLocation().X, GetActorLocation().Y));
		if (Flat < Best)
		{
			Best = Flat;
			Current.BuildingId = NearestDoor->Id;
			Current.Title = UTF8_TO_TCHAR(NearestDoor->Name.c_str());
			const std::vector<std::string> Services = ServicesForRole(NearestDoor->Role);
			Current.Prompt = Services.empty() ? TEXT("Enter") : UTF8_TO_TCHAR(Services.front().c_str());
		}
	}

	// A person. Their mind id is on the actor as a tag.
	ACharacter* NearestPerson = nullptr;
	FString NearestMind;
	double PersonBest = Best;
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
		Best = PersonBest;
		const NpcMind& Mind = Game->Sim().Npcs.GetOrCreate(TCHAR_TO_UTF8(*NearestMind));
		Current = FDTGInteraction();
		Current.Actor = NearestPerson;
		Current.MindId = NearestMind;
		Current.Title = UTF8_TO_TCHAR(Mind.Name.c_str());
		Current.Prompt = TEXT("Talk");
	}

	// A car: one parked at a house to ask after, or one of your own to get into.
	ADTGVehicle* NearestCar = nullptr;
	double CarBest = Best;
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
		Current = FDTGInteraction();
		Current.Actor = NearestCar;
		Current.CarId = NearestCar->ParkedCarId();
		const VehicleModel* Model = Game->Sim().Vehicles.FindModel(TCHAR_TO_UTF8(*NearestCar->ModelId()));
		Current.Title = Model ? UTF8_TO_TCHAR(Model->DisplayName.c_str()) : NearestCar->ModelId();
		// Traffic and police carry neither a garage uid nor a parked-car id, and there is nothing to
		// do with them -- offering to buy one would be a prompt that does nothing when pressed.
		if (!NearestCar->OwnedUid().IsEmpty() || NearestCar->bDemonstrator)
		{
			Current.Prompt = NearestCar->bDemonstrator ? TEXT("Take it for a lap") : TEXT("Get in");
		}
		else if (!Current.CarId.IsEmpty())
		{
			Current.Prompt = TEXT("Make an offer");
		}
		else
		{
			Current = FDTGInteraction(); // somebody else's car, going somewhere
		}
	}
}

FVector ADTGCharacter::InterestPointFor(const APawn* Pawn)
{
	if (const ADTGCharacter* Player = Cast<ADTGCharacter>(Pawn))
	{
		// While driving the character is hidden and left where they got in, so the car is the one that
		// has actually moved; the world has to be built around it rather than around the kerb.
		if (const ADTGVehicle* Car = Player->DrivingNow())
		{
			return Car->GetActorLocation();
		}
		return Player->InterestPoint();
	}
	return Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
}

ADTGVehicle* ADTGCharacter::ToggleVehicle()
{
	if (Driving)
	{
		// Step out beside the driver's door.
		const FVector Side = Driving->GetActorRightVector() * -140.0f;
		SetActorLocation(Driving->GetActorLocation() + Side + FVector(0.0f, 0.0f, 100.0f));
		SetActorHiddenInGame(false);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		// Back to a walking camera: close in, over the shoulder.
		CameraBoom->TargetArmLength = WalkingBoomLength;
		CameraBoom->SocketOffset = FVector(0.0f, 40.0f, 60.0f);
		CameraBoom->bEnableCameraLag = false;
		Driving->bPlayerDriven = false;
		ADTGVehicle* Left = Driving;
		Driving = nullptr;
		return Left;
	}
	// A car the player owns, or one a showroom has brought round for them to try. Anyone else's is an
	// offer (or a theft), not a free ride.
	ADTGVehicle* Target = Cast<ADTGVehicle>(Current.Actor);
	if (!Target || (Target->OwnedUid().IsEmpty() && !Target->bDemonstrator))
	{
		return nullptr;
	}
	Driving = Target;
	Driving->bPlayerDriven = true;
	SetActorHiddenInGame(true);
	// Hidden is not the same as absent. The driver rides along on the car's own origin, and the Pawn
	// profile blocks the channel a car body sweeps on -- so without this the player's own capsule sits
	// inside their car reporting a blocking hit every frame, and the car cannot be driven at all.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	// The hidden driver rides along inside the car and stays the view target, so the boom it already
	// carries becomes the chase camera: pulled back behind the car, raised, and lagging a little so
	// the view settles rather than snapping with every correction.
	CameraBoom->TargetArmLength = DrivingBoomLength;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 180.0f);
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.0f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 12.0f;
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

FString ADTGCharacter::EngageNearest()
{
	if (!Game || !Game->IsContentLoaded())
	{
		return FString();
	}
	// Whoever is closest and in front: the simulation is told about them so they can be hit and hit back.
	ACharacter* Target = nullptr;
	FString MindId;
	double Best = FMath::Square(350.0);
	const FVector Facing = GetActorForwardVector();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->IsPlayerControlled())
		{
			continue;
		}
		const FVector Toward = It->GetActorLocation() - GetActorLocation();
		if (FVector::DotProduct(Toward.GetSafeNormal2D(), Facing) < 0.3)
		{
			continue; // behind you
		}
		const double Distance = Toward.SizeSquared();
		if (Distance >= Best)
		{
			continue;
		}
		for (const FName& Tag : It->Tags)
		{
			const FString Name = Tag.ToString();
			if (Name.StartsWith(TEXT("mind:")) || Name.StartsWith(TEXT("super:")))
			{
				Best = Distance;
				Target = *It;
				MindId = Name.RightChop(Name.StartsWith(TEXT("mind:")) ? 5 : 6);
			}
		}
	}
	if (!Target || MindId.IsEmpty())
	{
		return FString();
	}
	const bool bSuperhuman = Target->ActorHasTag(FName(*FString::Printf(TEXT("super:%s"), *MindId)));
	return UTF8_TO_TCHAR(Game->Sim().EngageFighter(TCHAR_TO_UTF8(*MindId), std::string(), bSuperhuman,
		bSuperhuman ? 3 : 0).c_str());
}

void ADTGCharacter::Punch()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now < NextSwing || !Game || !Game->IsContentLoaded())
	{
		return;
	}
	NextSwing = Now + 0.55;
	if (Body)
	{
		Body->Swing();
	}
	const FString Target = EngageNearest();
	if (Target.IsEmpty())
	{
		return;
	}
	const StrikeResult Result = Game->Sim().Strike(TCHAR_TO_UTF8(*Target), StrikeKind::Fist);
	if (!Result.Summary.empty())
	{
		Game->OnNotice.Broadcast(UTF8_TO_TCHAR(Result.Summary.c_str()));
	}
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
	// If someone is in front of you the power lands on them, and Strike is what uses it -- so the power is
	// spent once either way, and levels and mutates once.
	EffectivePower Effect;
	const FString Target = EngageNearest();
	if (Target.IsEmpty())
	{
		const GameState::PowerUse Used = Game->Sim().UsePower(Slot, Witnesses, /*bInPublic*/ true);
		if (!Used.bOk)
		{
			return;
		}
		Effect = Used.Power;
	}
	else
	{
		const StrikeResult Hit = Game->Sim().Strike(TCHAR_TO_UTF8(*Target), StrikeKind::Power, Slot);
		if (!Hit.Summary.empty())
		{
			Game->OnNotice.Broadcast(UTF8_TO_TCHAR(Hit.Summary.c_str()));
		}
		// Reading the power's numbers back is free; using it again would not be.
		const PowerInstance* Instance = Game->Sim().PlayerPowers.Slot(Slot);
		if (!Instance)
		{
			return;
		}
		Effect = Game->Sim().Powers.Evaluate(*Instance);
		if (!Effect.Def)
		{
			return;
		}
	}
	NextPowerTime[Slot] = Now + FMath::Max(0.2, Effect.CooldownS);

	// What the power does in the world. The simulation owns the numbers; this is the body of it.
	const FVector Forward = GetActorForwardVector();
	switch (Effect.Def->Effect)
	{
	case PowerEffect::Flight:
	{
		// Both of these change how the player moves and both have to be put back, or one keypress
		// leaves them flying, or sprinting, for the rest of the game.
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->MaxFlySpeed = static_cast<float>(600.0 * Effect.Potency);
		GetWorldTimerManager().SetTimer(FlightTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			// Unless they are in a car by then, in which case the movement mode is not theirs to set.
			if (!DrivingNow() && GetCharacterMovement()->MovementMode == MOVE_Flying)
			{
				GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}), static_cast<float>(FMath::Max(1.0, Effect.DurationS)), false);
		break;
	}
	case PowerEffect::SuperSpeed:
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * static_cast<float>(1.0 + Effect.Potency);
		GetWorldTimerManager().SetTimer(SpeedTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		}), static_cast<float>(FMath::Max(1.0, Effect.DurationS)), false);
		break;
	}
	case PowerEffect::SuperStrength:
	case PowerEffect::Telekinesis:
	{
		// Shove whatever is in front: cars included, through the chassis simulation.
		const FVector Centre = GetActorLocation() + Forward * 150.0f;
		for (TActorIterator<ADTGVehicle> It(GetWorld()); It; ++It)
		{
			if (FVector::DistSquared(It->GetActorLocation(), Centre) < FMath::Square(Effect.RangeM * DTGMetresToUnreal))
			{
				const FVector Push = (It->GetActorLocation() - GetActorLocation()).GetSafeNormal() + FVector(0.0f, 0.0f, 0.3f);
				const Vec3 Impulse(Push.X * 4000.0 * Effect.Potency, Push.Y * 4000.0 * Effect.Potency, Push.Z * 4000.0 * Effect.Potency);
				const FVector At = It->GetActorLocation();
				It->Chassis().ApplyImpulse(Vec3(At.X / DTGMetresToUnreal, At.Y / DTGMetresToUnreal, At.Z / DTGMetresToUnreal), Impulse);
			}
		}
		break;
	}
	case PowerEffect::Teleport:
	{
		const FVector Landing = GetActorLocation() + Forward * static_cast<float>(Effect.RangeM * DTGMetresToUnreal);
		SetActorLocation(Landing, /*bSweep*/ true);
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
		const FVector End = Start + Forward * static_cast<float>(Effect.RangeM * DTGMetresToUnreal);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DTGPower), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			if (ADTGVehicle* Car = Cast<ADTGVehicle>(Hit.GetActor()))
			{
				const Vec3 Impulse(Forward.X * 3000.0 * Effect.Potency, Forward.Y * 3000.0 * Effect.Potency, 800.0);
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
		// Not while driving: the same flag hides the driver inside the car.
		GetWorldTimerManager().SetTimer(InvisibilityTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!DrivingNow())
				{
					SetActorHiddenInGame(false);
				}
			}),
			static_cast<float>(FMath::Max(0.5, Effect.DurationS)), false);
		break;
	}
	default:
		break;
	}
}
