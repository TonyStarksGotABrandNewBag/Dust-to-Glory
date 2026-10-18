#include "Game/DTGVehicle.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/DTGEngineAudio.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGWorldBuilder.h"

using namespace dtg::sim;

namespace
{
	/** Line traces against everything solid: the streamed terrain, bridge decks, buildings and other cars. */
	class FWorldGround : public GroundQuery
	{
	public:
		FWorldGround(const UWorld* InWorld, const AActor* InIgnore) : World(InWorld), Ignore(InIgnore) {}

		GroundHit Cast(const Vec3& Origin, const Vec3& Direction, double MaxDistance) const override
		{
			GroundHit Out;
			if (!World)
			{
				return Out;
			}
			const FVector Start(Origin.X * DTGMetresToUnreal, Origin.Y * DTGMetresToUnreal, Origin.Z * DTGMetresToUnreal);
			const FVector End = Start + FVector(Direction.X, Direction.Y, Direction.Z) * (MaxDistance * DTGMetresToUnreal);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(DTGWheel), /*bTraceComplex*/ true, Ignore);
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
			{
				return Out;
			}
			Out.bHit = true;
			Out.Distance = Hit.Distance / DTGMetresToUnreal;
			Out.Point = Vec3(Hit.ImpactPoint.X / DTGMetresToUnreal, Hit.ImpactPoint.Y / DTGMetresToUnreal, Hit.ImpactPoint.Z / DTGMetresToUnreal);
			Out.Normal = Vec3(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z);
			// Surface grip would come from a physical material; tarmac until those exist.
			Out.Grip = 1.0;
			return Out;
		}

	private:
		const UWorld* World = nullptr;
		const AActor* Ignore = nullptr;
	};

	FQuat ToUnrealQuat(const Quat& Value)
	{
		return FQuat(Value.X, Value.Y, Value.Z, Value.W);
	}

	FVector ToUnrealVector(const Vec3& Value)
	{
		return FVector(Value.X * DTGMetresToUnreal, Value.Y * DTGMetresToUnreal, Value.Z * DTGMetresToUnreal);
	}
}

ADTGVehicle::ADTGVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Overlap);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetGenerateOverlapEvents(true);

	// Through ConstructorHelpers, because this constructor also runs for the class default object.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	Shell->SetupAttachment(Body);
	Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded())
	{
		Shell->SetStaticMesh(CubeMesh.Object);
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel%d"), Index));
		Wheel->SetupAttachment(Body);
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CylinderMesh.Succeeded())
		{
			Wheel->SetStaticMesh(CylinderMesh.Object);
		}
		WheelMeshes.Add(Wheel);
	}

	Audio = CreateDefaultSubobject<UDTGEngineAudio>(TEXT("EngineAudio"));
	Audio->SetupAttachment(Body);
}

void ADTGVehicle::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	World = Cast<ADTGWorldBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGWorldBuilder::StaticClass()));
	Sim.SetStepRate(StepHz);
}

void ADTGVehicle::ConfigureFromOwned(const OwnedVehicle& Car)
{
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	Uid = UTF8_TO_TCHAR(Car.Uid.c_str());
	Model = UTF8_TO_TCHAR(Car.ModelId.c_str());
	Setup = Game->Sim().SetupForOwned(Car);
	if (const VehicleModel* Def = Game->Sim().Vehicles.FindModel(Car.ModelId))
	{
		LengthM = Def->LengthM;
		WidthM = Def->WidthM;
		HeightM = Def->HeightM;
	}
	Sim.Configure(Setup, LengthM, WidthM, HeightM);
	Sim.Dynamics().bAutomatic = Game->Sim().AutomaticGearbox();
	// Worn-out cars make less power, just as they do in the simulation's own telemetry.
	Sim.Dynamics().EngineHealth = FMath::Clamp(Car.Condition, 0.2, 1.0);
	ApplyVisuals();
	Audio->Configure(Setup);
}

void ADTGVehicle::ConfigureFromModel(const FString& ModelId)
{
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	Model = ModelId;
	Uid.Reset();
	Setup = Game->Sim().SetupFor(TCHAR_TO_UTF8(*ModelId));
	if (const VehicleModel* Def = Game->Sim().Vehicles.FindModel(TCHAR_TO_UTF8(*ModelId)))
	{
		LengthM = Def->LengthM;
		WidthM = Def->WidthM;
		HeightM = Def->HeightM;
	}
	Sim.Configure(Setup, LengthM, WidthM, HeightM);
	Sim.Dynamics().bAutomatic = true;
	ApplyVisuals();
	Audio->Configure(Setup);
}

void ADTGVehicle::ApplyVisuals()
{
	const double S = DTGMetresToUnreal;
	Body->SetBoxExtent(FVector(0.5 * LengthM * S, 0.5 * WidthM * S, 0.5 * HeightM * S), false);
	if (Shell)
	{
		// The engine cube is 100 uu a side and centred, so scale in metres and sit it on the body's middle.
		Shell->SetRelativeScale3D(FVector(LengthM, WidthM, HeightM));
		Shell->SetRelativeLocation(FVector::ZeroVector);
	}
	const double Radius = FMath::Max(0.2, Setup.FrontTire.RadiusM > 0.0 ? Setup.FrontTire.RadiusM : 0.32);
	for (TObjectPtr<UStaticMeshComponent>& Wheel : WheelMeshes)
	{
		if (Wheel)
		{
			// The engine cylinder is 100 uu tall and 100 uu across, standing on Z: lay it on its side so it rolls.
			Wheel->SetRelativeScale3D(FVector(2.0 * Radius, 2.0 * Radius, 0.25));
		}
	}
}

void ADTGVehicle::PlaceAt(const FVector& World2, double YawDegrees)
{
	const Vec3 Ground(World2.X / DTGMetresToUnreal, World2.Y / DTGMetresToUnreal, World2.Z / DTGMetresToUnreal);
	Sim.Teleport(Ground, FMath::DegreesToRadians(YawDegrees));
	SetActorLocationAndRotation(ToUnrealVector(Sim.State.Position), ToUnrealQuat(Sim.State.Orientation));
}

double ADTGVehicle::EngineRpm() const
{
	return Sim.Dynamics().Telemetry().EngineRpm;
}

int ADTGVehicle::CurrentGear() const
{
	return Sim.Dynamics().Telemetry().Gear;
}

void ADTGVehicle::SetupPlayerInputComponent(UInputComponent* InputComponent)
{
	// The player controller owns the bindings (Enhanced Input, created in code) and calls SetDriverInput.
	Super::SetupPlayerInputComponent(InputComponent);
}

void ADTGVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector Before = GetActorLocation();
	StepPhysics(FMath::Min(static_cast<double>(DeltaSeconds), 0.1));
	ResolveCollisions(Before);

	SetActorLocationAndRotation(ToUnrealVector(Sim.State.Position), ToUnrealQuat(Sim.State.Orientation), /*bSweep*/ false);

	// Wheels: where the suspension put them, spinning and steering as the simulation says.
	const std::array<ChassisWheel, 4>& Wheels = Sim.Wheels();
	for (int32 Index = 0; Index < WheelMeshes.Num() && Index < 4; ++Index)
	{
		if (!WheelMeshes[Index])
		{
			continue;
		}
		const Vec3 Local = Wheels[static_cast<std::size_t>(Index)].CenterLocal();
		WheelMeshes[Index]->SetRelativeLocation(FVector(Local.X * DTGMetresToUnreal, Local.Y * DTGMetresToUnreal, Local.Z * DTGMetresToUnreal));
		const FRotator Rotation(FMath::RadiansToDegrees(Wheels[static_cast<std::size_t>(Index)].SpinAngle), // pitch: rolling
			FMath::RadiansToDegrees(Wheels[static_cast<std::size_t>(Index)].SteerAngle),                     // yaw: steering
			90.0);                                                                                           // roll: lay the cylinder over
		WheelMeshes[Index]->SetRelativeRotation(Rotation);
	}

	if (Audio)
	{
		const DynamicsTelemetry& Telemetry = Sim.Dynamics().Telemetry();
		EngineSoundInput Sound;
		Sound.Rpm = Telemetry.EngineRpm;
		Sound.Throttle = Telemetry.Throttle;
		Sound.Load = Telemetry.EngineLoad;
		Sound.Boost = Telemetry.Boost;
		Sound.SpeedMs = Sim.State.Velocity.Length();
		Sound.bRevLimiter = Telemetry.bRevLimiter;
		Sound.bShifting = Telemetry.bShifting;
		Audio->SetState(Sound, Sim.MaxSlip(), 1.0);
	}
}

void ADTGVehicle::StepPhysics(double Dt)
{
	dtg::sim::DriverInput Input = Controls;
	Input.ShiftRequest = PendingShift;
	PendingShift = 0;

	// Deep water drowns the engine: no throttle, heavy drag, and the car coasts to a stop.
	bSwamped = false;
	if (World)
	{
		double SurfaceZ = 0.0;
		if (World->IsWaterAt(GetActorLocation(), &SurfaceZ))
		{
			const double DepthM = (SurfaceZ - Sim.State.Position.Z * DTGMetresToUnreal) / DTGMetresToUnreal;
			bSwamped = DepthM > 0.4;
		}
	}
	if (bSwamped)
	{
		Input.Throttle = 0.0;
		Input.Brake = FMath::Max(Input.Brake, 0.35);
	}

	const FWorldGround Ground(GetWorld(), this);
	Sim.Step(Dt, Input, Ground);
}

void ADTGVehicle::ResolveCollisions(const FVector& PreviousLocation)
{
	UWorld* const CurrentWorld = GetWorld();
	if (!CurrentWorld || !Body)
	{
		return;
	}
	// Sweep the body from where it was to where the physics put it. Anything in the way becomes an impulse
	// the chassis resolves, so a wall or another car is felt rather than driven through. The ground is left
	// out of it: the wheels' own ray casts hold the car up, and the body box reaches below the centre of
	// mass, so including it would report a collision with the road on every frame.
	const FVector Target = ToUnrealVector(Sim.State.Position);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTGBody), /*bTraceComplex*/ false, this);
	if (World)
	{
		Params.AddIgnoredActor(World);
	}
	FHitResult Hit;
	const FQuat Rotation = ToUnrealQuat(Sim.State.Orientation);
	const FCollisionShape Shape = FCollisionShape::MakeBox(Body->GetScaledBoxExtent() * 0.9f);
	if (!CurrentWorld->SweepSingleByChannel(Hit, PreviousLocation, Target, Rotation, ECC_Visibility, Shape, Params))
	{
		return;
	}
	if (Hit.bStartPenetrating && Hit.PenetrationDepth <= KINDA_SMALL_NUMBER)
	{
		return; // already touching and not pressing in: nothing new happened
	}
	const Vec3 Point(Hit.ImpactPoint.X / DTGMetresToUnreal, Hit.ImpactPoint.Y / DTGMetresToUnreal, Hit.ImpactPoint.Z / DTGMetresToUnreal);
	const Vec3 Normal(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z);
	Vec3 OtherVelocity;
	if (const ADTGVehicle* Other = Cast<ADTGVehicle>(Hit.GetActor()))
	{
		OtherVelocity = Other->Chassis().State.Velocity;
	}
	const double Closing = Sim.ResolveContact(Point, Normal, /*Restitution*/ 0.15, /*Friction*/ 0.6, OtherVelocity);
	if (Closing > 4.0 && Game && bPlayerDriven)
	{
		// A real hit is property damage the police may hear about, and wear on the car.
		Game->Sim().ReportCrime(Crime::PropertyDamage, false, true);
	}
	// Back the car out of whatever it hit by how far it went in, so the next sweep starts clear.
	const double PushM = (Hit.PenetrationDepth > 0.0 ? Hit.PenetrationDepth : 2.0) / DTGMetresToUnreal;
	Sim.State.Position += Normal * PushM;
}
