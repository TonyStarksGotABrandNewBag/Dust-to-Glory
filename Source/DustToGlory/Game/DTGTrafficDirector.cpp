#include "Game/DTGTrafficDirector.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

#include "Game/DTGCharacter.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGMeshBuilder.h"
#include "Game/DTGVehicle.h"

using namespace dtg::sim;

ADTGTrafficDirector::ADTGTrafficDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADTGTrafficDirector::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
}

FVector ADTGTrafficDirector::NodeLocation(int32 NodeId) const
{
	if (!Game || !Game->IsContentLoaded())
	{
		return FVector::ZeroVector;
	}
	const Intersection* Node = Game->Sim().City.FindIntersection(NodeId);
	if (!Node)
	{
		return FVector::ZeroVector;
	}
	const double Z = Game->Sim().Map.HeightAt(Node->X, Node->Y);
	return Game->ToUnreal(Node->X, Node->Y, Z);
}

int32 ADTGTrafficDirector::PickNode(const FVector& Around, bool bFar) const
{
	if (!Game || !Game->IsContentLoaded())
	{
		return 0;
	}
	const CityPlan& City = Game->Sim().City;
	if (City.Intersections.empty())
	{
		return 0;
	}
	const FVector2D Here = Game->ToSim(Around);
	const int32 Nearest = City.NearestIntersection(Here.X, Here.Y);
	if (!bFar)
	{
		return Nearest;
	}
	// A neighbour of a nearby node: far enough to appear off-camera, close enough to matter.
	if (const Intersection* Node = City.FindIntersection(Nearest))
	{
		if (!Node->Links.empty())
		{
			return Node->Links[FMath::RandHelper(static_cast<int32>(Node->Links.size()))];
		}
	}
	return Nearest;
}

ADTGVehicle* ADTGTrafficDirector::SpawnCar(const FVector& At, double YawDegrees, bool bPolice)
{
	if (!Game || !Game->IsContentLoaded())
	{
		return nullptr;
	}
	// Traffic drives whatever people around here drive: ordinary, affordable, current-ish models.
	const std::vector<VehicleModel>& Models = Game->Sim().Vehicles.Models();
	if (Models.empty())
	{
		return nullptr;
	}
	const VehicleModel* Pick = nullptr;
	for (int32 Attempt = 0; Attempt < 12 && !Pick; ++Attempt)
	{
		const VehicleModel& Candidate = Models[FMath::RandHelper(static_cast<int32>(Models.size()))];
		const bool bSuitable = !Candidate.bEventOnly && (bPolice ? Candidate.Body == BodyStyle::Sedan || Candidate.Body == BodyStyle::SUV
															   : Candidate.MarketPrice < Dollars(30000));
		if (bSuitable)
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
		return nullptr;
	}
	// Distant cars run their physics at half rate: the handling is the same, the ray casts are halved.
	// BeginPlay has already run by now, so the chassis is told directly as well as through the property.
	Car->StepHz = 120.0f;
	Car->Chassis().SetStepRate(120.0);
	Car->ConfigureFromModel(UTF8_TO_TCHAR(Pick->Id.c_str()));
	Car->PlaceAt(At, YawDegrees);
	return Car;
}

void ADTGTrafficDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	// The car while driving, the street outside while indoors, the player otherwise.
	const FVector Around = ADTGCharacter::InterestPointFor(Pawn);
	UpdateTraffic(Around, DeltaSeconds);
	UpdatePolice(Around);
	UpdatePedestrians(Around);
	UpdateParkedCars(Around);
}

void ADTGTrafficDirector::UpdateTraffic(const FVector& Around, float DeltaSeconds)
{
	const double Despawn = DespawnRadiusM * DTGMetresToUnreal;
	for (int32 Index = Drivers.Num() - 1; Index >= 0; --Index)
	{
		FDTGDriver& Driver = Drivers[Index];
		if (!Driver.Car)
		{
			Drivers.RemoveAtSwap(Index);
			continue;
		}
		// Police hang on further than traffic -- a chase that unloads its pursuers is no chase -- but
		// they do not hang on for ever, which is what kept every unit ever dispatched alive and ticking.
		const double Keep = Driver.bPolice ? Despawn * 2.5 : Despawn;
		if (FVector::Dist(Driver.Car->GetActorLocation(), Around) > Keep)
		{
			Driver.Car->Destroy();
			Drivers.RemoveAtSwap(Index);
			continue;
		}
		// At the junction, pick the next one and carry on.
		const FVector Target = NodeLocation(Driver.ToNode);
		if (FVector::Dist2D(Driver.Car->GetActorLocation(), Target) < 900.0)
		{
			const Intersection* Node = Game->Sim().City.FindIntersection(Driver.ToNode);
			Driver.FromNode = Driver.ToNode;
			if (Node && !Node->Links.empty())
			{
				Driver.ToNode = Node->Links[FMath::RandHelper(static_cast<int32>(Node->Links.size()))];
			}
		}
		DriveToward(Driver, NodeLocation(Driver.ToNode), DeltaSeconds);
	}

	// Keep the roads as busy as the simulation says they should be: an outbreak thins them out.
	const double Life = Game->Sim().Outbreak.StreetLife();
	const int32 Want = FMath::RoundToInt32(TrafficCars * Life);
	const double Now = GetWorld()->GetTimeSeconds();
	int32 Civilians = 0;
	for (const FDTGDriver& Driver : Drivers)
	{
		Civilians += Driver.bPolice ? 0 : 1;
	}
	// Counted without the police, or a five-star response empties the streets it is chasing you through.
	if (Civilians < Want && Now > NextSpawn)
	{
		NextSpawn = Now + 0.25;
		const int32 Node = PickNode(Around, /*bFar*/ true);
		const FVector At = NodeLocation(Node);
		if (!At.IsNearlyZero() && FVector::Dist(At, Around) < SpawnRadiusM * DTGMetresToUnreal)
		{
			if (ADTGVehicle* Car = SpawnCar(At + FVector(0.0f, 0.0f, 60.0f), FMath::FRandRange(0.0f, 360.0f), false))
			{
				FDTGDriver Driver;
				Driver.Car = Car;
				Driver.FromNode = Node;
				Driver.ToNode = Node;
				Drivers.Add(Driver);
			}
		}
	}
}

void ADTGTrafficDirector::UpdatePolice(const FVector& Around)
{
	const int32 Want = Game->Sim().Wanted.UnitsWanted();
	int32 Have = 0;
	for (const FDTGDriver& Driver : Drivers)
	{
		Have += Driver.bPolice ? 1 : 0;
	}
	if (Have >= Want)
	{
		// Stand the extras down once the heat is off.
		for (int32 Index = Drivers.Num() - 1; Index >= 0 && Have > Want; --Index)
		{
			if (Drivers[Index].bPolice)
			{
				if (Drivers[Index].Car)
				{
					Drivers[Index].Car->Destroy();
				}
				Drivers.RemoveAtSwap(Index);
				--Have;
			}
		}
		return;
	}
	// One unit at a time, like ordinary traffic. Without this the whole response arrives inside a few
	// frames -- each one picking a model, building a chassis and generating a mesh -- which is both a
	// visible hitch and a police force that teleports in rather than converging. Checked before the
	// node is picked, because picking one is the expensive part and most frames do not get to spawn.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now < NextPoliceSpawn)
	{
		return;
	}
	const int32 Node = PickNode(Around, /*bFar*/ true);
	const FVector At = NodeLocation(Node);
	if (At.IsNearlyZero() || FVector::Dist(At, Around) > DespawnRadiusM * DTGMetresToUnreal)
	{
		return; // nowhere to come from that is not already out of sight
	}
	NextPoliceSpawn = Now + 0.7;
	if (ADTGVehicle* Car = SpawnCar(At + FVector(0.0f, 0.0f, 60.0f), 0.0, /*bPolice*/ true))
	{
		FDTGDriver Driver;
		Driver.Car = Car;
		Driver.bPolice = true;
		Driver.FromNode = Node;
		Driver.ToNode = Node;
		Drivers.Add(Driver);
	}
}

void ADTGTrafficDirector::DriveToward(FDTGDriver& Driver, const FVector& Target, float DeltaSeconds)
{
	if (!Driver.Car)
	{
		return;
	}
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const APawn* Chased = Controller ? Controller->GetPawn() : nullptr;
	FVector Goal = Target;
	double SpeedLimitKph = 50.0;
	if (Driver.bPolice && Game->Sim().Wanted.IsWanted() && Chased)
	{
		// Wanted: the police stop following the road graph and come straight for the player.
		Goal = Chased->GetActorLocation();
		SpeedLimitKph = 130.0;
	}
	const FVector Here = Driver.Car->GetActorLocation();
	const FVector ToGoal = (Goal - Here).GetSafeNormal2D();
	const FVector Facing = Driver.Car->GetActorForwardVector().GetSafeNormal2D();
	const double Turn = FMath::Atan2(FVector::CrossProduct(Facing, ToGoal).Z, FVector::DotProduct(Facing, ToGoal));

	DriverInput Input;
	Input.Steer = FMath::Clamp(Turn * 1.4, -1.0, 1.0);
	const double Speed = Driver.Car->SpeedKph();
	// Ease off through the corner and into the junction, as a driver would.
	const double Wanted = SpeedLimitKph * FMath::Clamp(1.0 - FMath::Abs(Turn) * 0.6, 0.25, 1.0);
	Input.Throttle = Speed < Wanted ? FMath::Clamp((Wanted - Speed) / 15.0, 0.0, 0.8) : 0.0;
	Input.Brake = Speed > Wanted + 8.0 ? FMath::Clamp((Speed - Wanted) / 25.0, 0.0, 0.7) : 0.0;

	// Don't drive into the back of whatever is ahead.
	FHitResult Hit;
	const FVector Ahead = Here + Facing * FMath::Max(600.0, Speed * 25.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTGTraffic), false, Driver.Car);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Here + FVector(0, 0, 60), Ahead, ECC_Visibility, Params))
	{
		if (Hit.GetActor() && Hit.GetActor()->IsA<APawn>())
		{
			Input.Throttle = 0.0;
			Input.Brake = 0.9;
		}
	}
	Driver.Car->SetDriverInput(Input);
	Driver.Patience = FMath::Max(0.0, Driver.Patience - DeltaSeconds * 0.01);
}

void ADTGTrafficDirector::UpdatePedestrians(const FVector& Around)
{
	const double Despawn = DespawnRadiusM * DTGMetresToUnreal;
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = Walkers.Num() - 1; Index >= 0; --Index)
	{
		FDTGWalker& Walker = Walkers[Index];
		if (!Walker.Actor)
		{
			Walkers.RemoveAtSwap(Index);
			continue;
		}
		if (FVector::Dist(Walker.Actor->GetActorLocation(), Around) > Despawn)
		{
			Walker.Actor->Destroy();
			Walkers.RemoveAtSwap(Index);
			continue;
		}
		if (Now > Walker.NextThink)
		{
			// Ask the simulation what this person wants to do; awakened minds choose for themselves.
			Walker.NextThink = Now + FMath::FRandRange(4.0, 9.0);
			NpcMind& Mind = Game->Sim().Npcs.GetOrCreate(TCHAR_TO_UTF8(*Walker.MindId));
			const NpcDecision Decision = Game->Sim().Npcs.Decide(Mind, Game->Sim().Clock.Hour(), Game->Sim().Random, Game->Sim().Careers);
			LotRole WantedRole = LotRole::Shop;
			const BuildingPlan* Destination = nullptr;
			if (!Decision.Destination.empty() && ParseLotRole(Decision.Destination, WantedRole))
			{
				Destination = Game->Sim().City.FirstWithRole(WantedRole);
			}
			if (Destination)
			{
				const double Z = Game->Sim().Map.HeightAt(Destination->DoorX, Destination->DoorY);
				Walker.Target = Game->ToUnreal(Destination->DoorX, Destination->DoorY, Z);
			}
			else
			{
				Walker.Target = Around + FVector(FMath::FRandRange(-4000.0f, 4000.0f), FMath::FRandRange(-4000.0f, 4000.0f), 0.0f);
			}
		}
		// Walk there. Pedestrians use Unreal's own movement: they are people, not vehicles.
		if (ACharacter* Person = Cast<ACharacter>(Walker.Actor))
		{
			const FVector Direction = (Walker.Target - Person->GetActorLocation()).GetSafeNormal2D();
			// Forced, because a pawn with no local controller ignores movement input outright. Letting the
		// movement component run (bRunPhysicsWithNoController, where they are spawned) only gets them
		// gravity; without this they stand on the pavement for ever, turning to face where they meant
		// to walk.
		Person->AddMovementInput(Direction, 1.0f, /*bForce*/ true);
			if (!Direction.IsNearlyZero())
			{
				Person->SetActorRotation(FRotator(0.0, Direction.Rotation().Yaw, 0.0));
			}
		}
	}

	const double Life = Game->Sim().Outbreak.StreetLife();
	const int32 Want = FMath::RoundToInt32(Pedestrians * Life);
	if (Walkers.Num() >= Want)
	{
		return;
	}
	// Spawn one on a sidewalk near the player.
	const CityPlan& City = Game->Sim().City;
	if (City.Blocks.empty())
	{
		return;
	}
	const FVector2D Here = Game->ToSim(Around);
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		const dtg::sim::Rect& Block = City.Blocks[FMath::RandHelper(static_cast<int32>(City.Blocks.size()))];
		const double Dx = Block.CenterX() - Here.X;
		const double Dy = Block.CenterY() - Here.Y;
		if (Dx * Dx + Dy * Dy > FMath::Square(SpawnRadiusM))
		{
			continue;
		}
		const double Z = Game->Sim().Map.HeightAt(Block.CenterX(), Block.CenterY());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ACharacter* Person = GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(),
			FTransform(FRotator::ZeroRotator, Game->ToUnreal(Block.CenterX(), Block.CenterY(), Z + 1.0)), Params);
		if (!Person)
		{
			return;
		}
		// A character with no controller does not move: the movement component only runs for a locally
		// controlled pawn, so AddMovementInput would be swallowed and gravity would never be applied --
		// pedestrians frozen mid-stride and anything spawned above the ground left hanging there.
		if (UCharacterMovementComponent* Legs = Person->GetCharacterMovement())
		{
			Legs->bRunPhysicsWithNoController = true;
			Legs->MaxWalkSpeed = 160.0f;
			Legs->bOrientRotationToMovement = false;
		}
		// Dressed after the mind id is known, so the same passer-by looks the same every time.
		FDTGWalker Walker;
		Walker.Actor = Person;
		// Keyed to the block they live around, so the same passer-by remembers the player between visits.
		Walker.MindId = FString::Printf(TEXT("ped-%d-%d"), FMath::RoundToInt32(Block.CenterX()), FMath::RoundToInt32(Block.CenterY()));
		// Tagged so anything that needs to know whose mind this is (power witnesses, conversations) can ask.
		Person->Tags.Add(FName(*FString::Printf(TEXT("mind:%s"), *Walker.MindId)));
		// Seeded from the id, and kept clear of 1 and 2, which dress a hero and a villain.
		DTGMeshBuilder::DressPerson(Person, 3 + static_cast<int32>(GetTypeHash(Walker.MindId) % 1000u));
		// A couple of the people already on this street know this one, which is how anything the
		// player does in front of one of them reaches the rest without being broadcast. Only a
		// couple: a town where everybody knows everybody carries a rumour everywhere in one hop.
		const std::string MyMind = TCHAR_TO_UTF8(*Walker.MindId);
		Game->Sim().Meet(MyMind, Block.CenterX(), Block.CenterY());
		int32 Introduced = 0;
		for (const FDTGWalker& Neighbour : Walkers)
		{
			if (Introduced >= 2)
			{
				break;
			}
			if (Neighbour.MindId.IsEmpty() || Neighbour.MindId == Walker.MindId || !Neighbour.Actor)
			{
				continue;
			}
			// Neighbours in the literal sense: people who share a stretch of street.
			if (FVector::Dist2D(Neighbour.Actor->GetActorLocation(), Person->GetActorLocation()) > NeighbourRangeM * DTGMetresToUnreal)
			{
				continue;
			}
			Game->Sim().Npcs.Introduce(MyMind, TCHAR_TO_UTF8(*Neighbour.MindId), 0.4);
			Introduced += 1;
		}
		Walker.Target = Person->GetActorLocation();
		Walkers.Add(Walker);
		return;
	}
}

void ADTGTrafficDirector::Reset()
{
	for (FDTGDriver& Driver : Drivers)
	{
		if (Driver.Car)
		{
			Driver.Car->Destroy();
		}
	}
	for (FDTGWalker& Walker : Walkers)
	{
		if (Walker.Actor)
		{
			Walker.Actor->Destroy();
		}
	}
	for (TObjectPtr<ADTGVehicle>& Car : Parked)
	{
		if (Car)
		{
			Car->Destroy();
		}
	}
	Drivers.Reset();
	Walkers.Reset();
	Parked.Reset();
}

void ADTGTrafficDirector::UpdateParkedCars(const FVector& Around)
{
	// A car outside a house is a car the player can walk up to and make an offer on, so each nearby home
	// gets the one the simulation says is parked there -- same model, same owner, same history.
	const double Despawn = DespawnRadiusM * DTGMetresToUnreal;
	for (int32 Index = Parked.Num() - 1; Index >= 0; --Index)
	{
		if (!Parked[Index] || FVector::Dist(Parked[Index]->GetActorLocation(), Around) > Despawn)
		{
			if (Parked[Index])
			{
				Parked[Index]->Destroy();
			}
			Parked.RemoveAtSwap(Index);
		}
	}
	if (Parked.Num() >= MaxParkedCars)
	{
		return;
	}
	const FVector2D Here = Game->ToSim(Around);
	// Only the houses near the player, from the plan's index.
	for (const BuildingPlan* Found : Game->Sim().City.BuildingsNear(Here.X, Here.Y, SpawnRadiusM))
	{
		const BuildingPlan& Building = *Found;
		if (Building.Role != LotRole::Home)
		{
			continue;
		}
		const FString CarId = FString::Printf(TEXT("home-%d"), Building.Id);
		bool bAlready = false;
		for (const TObjectPtr<ADTGVehicle>& Car : Parked)
		{
			bAlready = bAlready || (Car && Car->ParkedCarId() == CarId);
		}
		if (bAlready)
		{
			continue;
		}
		const PrivateCar& Listing = Game->Sim().PrivateCarAt(TCHAR_TO_UTF8(*CarId));
		if (Listing.bSold)
		{
			continue; // the player already bought this one; the driveway is empty
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTGVehicle* Car = GetWorld()->SpawnActor<ADTGVehicle>(ADTGVehicle::StaticClass(), FTransform::Identity, Params);
		if (!Car)
		{
			return;
		}
		Car->StepHz = 120.0f;
		Car->Chassis().SetStepRate(120.0);
		Car->ConfigureFromModel(UTF8_TO_TCHAR(Listing.ModelId.c_str()));
		Car->SetParkedCarId(CarId);
		const double Z = Game->Sim().Map.HeightAt(Building.DoorX, Building.DoorY);
		// Just off the door, nose to the kerb.
		Car->PlaceAt(Game->ToUnreal(Building.DoorX, Building.DoorY, Z) + FVector(0.0f, 300.0f, 60.0f), 0.0);
		Parked.Add(Car);
		if (Parked.Num() >= MaxParkedCars)
		{
			return;
		}
	}
}
