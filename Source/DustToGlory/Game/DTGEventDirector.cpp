#include "Game/DTGEventDirector.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "Game/DTGCharacter.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGMeshBuilder.h"
#include "Game/DTGVehicle.h"

using namespace dtg::sim;

ADTGEventDirector::ADTGEventDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADTGEventDirector::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
}

FVector ADTGEventDirector::CheckpointLocation(int32 Index) const
{
	return Checkpoints.IsValidIndex(Index) ? Checkpoints[Index] : FVector::ZeroVector;
}

bool ADTGEventDirector::StartEvent(EventKind InKind, int32 InTier)
{
	if (!Game || !Game->IsContentLoaded() || State != EDTGRaceState::Idle)
	{
		return false;
	}
	const VenuePlan* Venue = Game->Sim().City.FindVenue(InKind);
	if (!Venue)
	{
		return false;
	}
	if (Venue->Path.size() < 2)
	{
		UE_LOG(LogDTG, Warning, TEXT("%s has no racing line; not charging an entry fee."), UTF8_TO_TCHAR(EventKindName(InKind)));
		return false;
	}
	// You enter a race in a car. Checked before the fee is taken, not after.
	const ADTGCharacter* Entrant = Cast<ADTGCharacter>(
		GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr);
	if (!Entrant || !Entrant->DrivingNow())
	{
		Game->OnNotice.Broadcast(TEXT("You'll need to be in a car for that."));
		return false;
	}
	// The simulation takes the entry fee and says whether the player can afford to be here.
	const ActionResult Entry = Game->Sim().EnterEvent(InKind, InTier);
	Game->OnNotice.Broadcast(UTF8_TO_TCHAR(Entry.Message.c_str()));
	if (!Entry.bOk)
	{
		return false;
	}
	Kind = InKind;
	Tier = InTier;
	Stage(*Venue, InTier);
	return true;
}

void ADTGEventDirector::Stage(const VenuePlan& Venue, int32 InTier)
{
	Racers.Reset();
	Checkpoints.Reset();
	for (const std::array<double, 2>& Point : Venue.Path)
	{
		const double Z = Game->Sim().Map.HeightAt(Point[0], Point[1]);
		Checkpoints.Add(Game->ToUnreal(Point[0], Point[1], Z + 0.5));
	}
	if (Checkpoints.Num() < 2)
	{
		return;
	}
	// A drag race is one run; a derby has no laps at all; circuits run a handful.
	switch (Kind)
	{
	case EventKind::DragRace: Laps = 1; break;
	case EventKind::DemolitionDerby: Laps = 0; break;
	case EventKind::MonsterJam: Laps = 2; break;
	case EventKind::Formula: Laps = 2 + InTier; break;
	default: Laps = 1; break;
	}

	// The player lines up in whatever they drove here; the field is built from suitable machinery.
	const FVector Line = Checkpoints[0];
	const FVector Direction = (Checkpoints[1] - Checkpoints[0]).GetSafeNormal2D();
	const FVector Across = FVector::CrossProduct(FVector::UpVector, Direction);
	const double Yaw = Direction.Rotation().Yaw;

	ADTGCharacter* Player = Cast<ADTGCharacter>(GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr);
	ADTGVehicle* PlayerCar = Player ? Player->DrivingNow() : nullptr;
	if (PlayerCar)
	{
		PlayerCar->PlaceAt(Line, Yaw);
		FDTGRacer Racer;
		Racer.Car = PlayerCar;
		Racer.bPlayer = true;
		Racer.Name = TEXT("You");
		Racers.Add(Racer);
	}

	const std::vector<VehicleModel>& Models = Game->Sim().Vehicles.Models();
	for (int32 Index = 1; Index < DefaultField && !Models.empty(); ++Index)
	{
		// Event machinery for the specialist events, road cars for the rest.
		const VehicleModel* Pick = nullptr;
		for (int32 Attempt = 0; Attempt < 20 && !Pick; ++Attempt)
		{
			const VehicleModel& Candidate = Models[FMath::RandHelper(static_cast<int32>(Models.size()))];
			const bool bWantsEventCar = Kind == EventKind::MonsterJam || Kind == EventKind::Formula;
			if (Candidate.bEventOnly == bWantsEventCar)
			{
				Pick = &Candidate;
			}
		}
		if (!Pick)
		{
			Pick = &Models.front();
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTGVehicle* Car = GetWorld()->SpawnActor<ADTGVehicle>(ADTGVehicle::StaticClass(), FTransform::Identity, Params);
		if (!Car)
		{
			continue;
		}
		Car->ConfigureFromModel(UTF8_TO_TCHAR(Pick->Id.c_str()));
		const FVector Slot = Line + Across * ((Index % 2 == 0 ? 1 : -1) * 400.0 * ((Index + 1) / 2)) - Direction * (Index * 150.0);
		Car->PlaceAt(Slot, Yaw);
		FDTGRacer Racer;
		Racer.Car = Car;
		Racer.Name = UTF8_TO_TCHAR(Pick->DisplayName.c_str());
		Racers.Add(Racer);
	}
	State = EDTGRaceState::Countdown;
	StateUntil = GetWorld()->GetTimeSeconds() + 4.0;
	Game->OnNotice.Broadcast(FString::Printf(TEXT("%s, tier %d: %d cars. Stand on it."),
		UTF8_TO_TCHAR(EventKindName(Kind)), Tier, Racers.Num()));
}

void ADTGEventDirector::AbandonEvent()
{
	for (FDTGRacer& Racer : Racers)
	{
		if (Racer.Car && !Racer.bPlayer)
		{
			Racer.Car->Destroy();
		}
	}
	Racers.Reset();
	State = EDTGRaceState::Idle;
}

void ADTGEventDirector::Reset()
{
	AbandonEvent();
	for (TObjectPtr<AActor>& Who : Superhumans)
	{
		if (Who)
		{
			Who->Destroy();
		}
	}
	Superhumans.Reset();
	if (Game && Game->IsContentLoaded())
	{
		Game->Sim().Fights.Clear();
	}
	State = EDTGRaceState::Idle;
}

int32 ADTGEventDirector::PlayerPosition() const
{
	int32 Position = 1;
	const FDTGRacer* Player = Racers.FindByPredicate([](const FDTGRacer& Racer) { return Racer.bPlayer; });
	if (!Player)
	{
		return 0;
	}
	for (const FDTGRacer& Racer : Racers)
	{
		if (&Racer == Player)
		{
			continue;
		}
		const bool bAhead = Racer.Finished >= 0.0 && (Player->Finished < 0.0 || Racer.Finished < Player->Finished);
		const bool bFurther = Racer.Finished < 0.0 && Player->Finished < 0.0 &&
			(Racer.Lap > Player->Lap || (Racer.Lap == Player->Lap && Racer.Checkpoint > Player->Checkpoint));
		Position += (bAhead || bFurther) ? 1 : 0;
	}
	return Position;
}

int32 ADTGEventDirector::PlayerLap() const
{
	const FDTGRacer* Player = Racers.FindByPredicate([](const FDTGRacer& Racer) { return Racer.bPlayer; });
	return Player ? Player->Lap + 1 : 0;
}

void ADTGEventDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	switch (State)
	{
	case EDTGRaceState::Countdown:
		// Hold everyone on the line, then go.
		for (FDTGRacer& Racer : Racers)
		{
			if (Racer.Car && !Racer.bPlayer)
			{
				DriverInput Hold;
				Hold.Brake = 1.0;
				Racer.Car->SetDriverInput(Hold);
			}
		}
		if (Now >= StateUntil)
		{
			State = EDTGRaceState::Running;
			StartedAt = Now;
			Game->OnNotice.Broadcast(TEXT("GO"));
		}
		break;
	case EDTGRaceState::Running:
		UpdateRace(DeltaSeconds);
		break;
	default:
		break;
	}

	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	UpdateSuperhumans(Controller && Controller->GetPawn() ? Controller->GetPawn()->GetActorLocation() : FVector::ZeroVector);
}

void ADTGEventDirector::UpdateRace(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	int32 Running = 0;
	for (FDTGRacer& Racer : Racers)
	{
		if (!Racer.Car || Racer.Finished >= 0.0)
		{
			continue;
		}
		// A derby is over for you when your car stops working, not when a lap ends.
		if (Kind == EventKind::DemolitionDerby)
		{
			// Rolled over is finished, and so is being hit hard enough often enough -- a derby decided
			// only by rollovers is a derby that never ends, because most hits do not roll a car.
			Racer.Damage += Racer.Car->Chassis().IsCapsized() ? DeltaSeconds : 0.0;
			const double Impact = Racer.Car->Chassis().State.Velocity.Length();
			Racer.Damage += Racer.LastSpeed - Impact > 6.0 ? 1.2 : 0.0;
			Racer.LastSpeed = Impact;
			if (Racer.Damage > 4.0)
			{
				Racer.Finished = Now;
				continue;
			}
			++Running;
			if (!Racer.bPlayer)
			{
				// Aim for whoever is closest and keep hitting them.
				FDTGRacer* Nearest = nullptr;
				double Best = TNumericLimits<double>::Max();
				for (FDTGRacer& Other : Racers)
				{
					if (&Other == &Racer || !Other.Car || Other.Finished >= 0.0)
					{
						continue;
					}
					const double Distance = FVector::DistSquared(Other.Car->GetActorLocation(), Racer.Car->GetActorLocation());
					if (Distance < Best)
					{
						Best = Distance;
						Nearest = &Other;
					}
				}
				if (Nearest && Nearest->Car)
				{
					const FVector ToTarget = (Nearest->Car->GetActorLocation() - Racer.Car->GetActorLocation()).GetSafeNormal2D();
					const FVector Facing = Racer.Car->GetActorForwardVector().GetSafeNormal2D();
					DriverInput Input;
					Input.Steer = FMath::Clamp(FMath::Atan2(FVector::CrossProduct(Facing, ToTarget).Z, FVector::DotProduct(Facing, ToTarget)) * 1.5, -1.0, 1.0);
					Input.Throttle = 0.85;
					Racer.Car->SetDriverInput(Input);
				}
			}
			continue;
		}
		++Running;
		// Checkpoints: reach one and the next is the target.
		const FVector Target = CheckpointLocation(Racer.Checkpoint);
		if (!Target.IsNearlyZero() && FVector::Dist2D(Racer.Car->GetActorLocation(), Target) < 1200.0)
		{
			++Racer.Checkpoint;
			if (Racer.Checkpoint >= Checkpoints.Num())
			{
				Racer.Checkpoint = 0;
				++Racer.Lap;
				if (Racer.Lap >= Laps)
				{
					Racer.Finished = Now;
					if (Racer.bPlayer)
					{
						Game->OnNotice.Broadcast(FString::Printf(TEXT("Finished P%d of %d."), PlayerPosition(), Racers.Num()));
					}
					continue;
				}
			}
		}
		if (!Racer.bPlayer)
		{
			DriveRacer(Racer);
		}
	}
	const FDTGRacer* Player = Racers.FindByPredicate([](const FDTGRacer& Racer) { return Racer.bPlayer; });
	const bool bPlayerDone = !Player || Player->Finished >= 0.0;
	// Every event has an outside limit, so nothing can leave the player stuck in one: a derby where
	// nobody manages to put anybody out, a racer wedged against a wall, a checkpoint never reached.
	const bool bTimedOut = StartedAt > 0.0 && Now - StartedAt > EventLimitSeconds;
	if (bPlayerDone || Running == 0 || bTimedOut || (Kind == EventKind::DemolitionDerby && Running <= 1))
	{
		Finish();
	}
}

void ADTGEventDirector::DriveRacer(FDTGRacer& Racer)
{
	const FVector Target = CheckpointLocation(Racer.Checkpoint);
	if (Target.IsNearlyZero() || !Racer.Car)
	{
		return;
	}
	const FVector ToTarget = (Target - Racer.Car->GetActorLocation()).GetSafeNormal2D();
	const FVector Facing = Racer.Car->GetActorForwardVector().GetSafeNormal2D();
	const double Turn = FMath::Atan2(FVector::CrossProduct(Facing, ToTarget).Z, FVector::DotProduct(Facing, ToTarget));
	DriverInput Input;
	Input.Steer = FMath::Clamp(Turn * 1.6, -1.0, 1.0);
	// Racers brake for corners and otherwise use everything they have; higher tiers make fewer mistakes.
	const double Commitment = FMath::Clamp(0.72 + 0.05 * Tier, 0.7, 0.98);
	Input.Throttle = FMath::Clamp(Commitment - FMath::Abs(Turn) * 0.7, 0.0, 1.0);
	Input.Brake = FMath::Abs(Turn) > 0.8 && Racer.Car->SpeedKph() > 60.0 ? 0.4 : 0.0;
	Racer.Car->SetDriverInput(Input);
}

void ADTGEventDirector::Finish()
{
	const int32 Position = FMath::Max(1, PlayerPosition());
	const int32 Field = FMath::Max(1, Racers.Num());
	const Money Prize = Game->Sim().FinishEvent(Kind, Tier, Position, Field);
	Game->OnNotice.Broadcast(Prize > 0
		? FString::Printf(TEXT("P%d of %d: %s"), Position, Field, UTF8_TO_TCHAR(FormatMoney(Prize).c_str()))
		: FString::Printf(TEXT("P%d of %d. No money this time."), Position, Field));
	// The field goes home and the venue is free for the next entry.
	AbandonEvent();
	LastPosition = Position;
	LastField = Field;
}

void ADTGEventDirector::UpdateSuperhumans(const FVector& Around)
{
	// A few people with powers are always out there; villains draw heroes, and both notice the player.
	const int32 Want = 3;
	for (int32 Index = Superhumans.Num() - 1; Index >= 0; --Index)
	{
		if (Superhumans[Index] && FVector::Dist(Superhumans[Index]->GetActorLocation(), Around) <= 60000.0)
		{
			continue;
		}
		if (Superhumans[Index])
		{
			// Leaving the area means leaving the fight; otherwise they would keep swinging from miles away.
			for (const FName& Tag : Superhumans[Index]->Tags)
			{
				const FString Name = Tag.ToString();
				if (Name.StartsWith(TEXT("super:")))
				{
					Game->Sim().Fights.Disengage(TCHAR_TO_UTF8(*Name.RightChop(6)));
				}
			}
			Superhumans[Index]->Destroy();
		}
		Superhumans.RemoveAtSwap(Index);
	}
	if (Superhumans.Num() >= Want || Around.IsNearlyZero())
	{
		return;
	}
	const Alignment Side = FMath::RandBool() ? Alignment::Hero : Alignment::Villain;
	const FVector At = Around + FVector(FMath::FRandRange(-12000.0f, 12000.0f), FMath::FRandRange(-12000.0f, 12000.0f), 200.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ACharacter* Person = GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(), FTransform(FRotator::ZeroRotator, At), Params);
	if (!Person)
	{
		return;
	}
	// Nobody is controlling them, and an uncontrolled character's movement component does not run at
	// all -- so without this they would hang in the air two metres above the road for as long as they
	// existed, which is where they are spawned.
	if (UCharacterMovementComponent* Legs = Person->GetCharacterMovement())
	{
		Legs->bRunPhysicsWithNoController = true;
	}
	// Registered only once they are standing somewhere, so a failed spawn leaves no ghost in the fight
	// list. The simulation keeps them, so a fight uses their real codename, threat and bounty.
	const std::string Id = Game->Sim().EngageSuperhuman(Side, 1 + FMath::RandHelper(5));
	const Fighter* Who = Game->Sim().Fights.Find(Id);
	if (!Who)
	{
		Person->Destroy();
		return;
	}
	DTGMeshBuilder::DressPerson(Person, Side == Alignment::Hero ? 1 : 2);
	// Tagged with the id the simulation gave them, so a punch finds the right fighter.
	Person->Tags.Add(FName(*FString::Printf(TEXT("super:%s"), UTF8_TO_TCHAR(Id.c_str()))));
	UE_LOG(LogDTG, Verbose, TEXT("%s is around: threat %d"), UTF8_TO_TCHAR(Who->Name.c_str()), Who->Threat);
	Superhumans.Add(Person);
}
