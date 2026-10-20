#include "Game/DTGVehicle.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include "Game/DTGEngineAudio.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGMeshBuilder.h"
#include "Game/DTGSky.h"
#include "Game/DTGWorldBuilder.h"

using namespace dtg::sim;

namespace
{
	/** Line traces against everything solid: the streamed terrain, bridge decks, buildings and other cars. */
	class FWorldGround : public GroundQuery
	{
	public:
		FWorldGround(const UWorld* InWorld, const AActor* InIgnore, double InGrip)
			: World(InWorld), Ignore(InIgnore), Grip(InGrip)
		{
		}

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
			// Tarmac, wetted by whatever the weather is doing. A per-surface material would refine this.
			Out.Grip = Grip;
			return Out;
		}

	private:
		const UWorld* World = nullptr;
		const AActor* Ignore = nullptr;
		double Grip = 1.0;
	};

	FQuat ToUnrealQuat(const Quat& Value)
	{
		return FQuat(Value.X, Value.Y, Value.Z, Value.W);
	}

	FLinearColor FromRgb(uint32 Rgb)
	{
		return FLinearColor(FColor(static_cast<uint8>((Rgb >> 16) & 0xFF), static_cast<uint8>((Rgb >> 8) & 0xFF),
			static_cast<uint8>(Rgb & 0xFF)));
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
	// So another car's body sweep, and anything else looking for solid things, finds this one.
	Body->SetCollisionResponseToChannel(DTGBodySweepChannel, ECR_Block);
	// Nothing subscribes to overlaps, and the car is teleported every frame by the chassis simulation,
	// which would make the engine recompute the overlap set each time for nobody.
	Body->SetGenerateOverlapEvents(false);

	// The body and wheels are generated from the car's own dimensions (see DTGMeshBuilder), so no vehicle
	// art ships with the game and every model in the catalog gets a shape of its own.
	Shell = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Shell"));
	Shell->SetupAttachment(Body);
	Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shell->SetRelativeLocation(FVector::ZeroVector);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UProceduralMeshComponent* Wheel = CreateDefaultSubobject<UProceduralMeshComponent>(*FString::Printf(TEXT("Wheel%d"), Index));
		Wheel->SetupAttachment(Body);
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WheelMeshes.Add(Wheel);
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		USpotLightComponent* Lamp = CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("Headlight%d"), Index));
		Lamp->SetupAttachment(Body);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetIntensity(12000.0f);
		Lamp->SetAttenuationRadius(5500.0f);
		Lamp->SetInnerConeAngle(18.0f);
		Lamp->SetOuterConeAngle(38.0f);
		Lamp->SetLightColor(FLinearColor(1.0f, 0.97f, 0.88f));
		// No shadows: a beam on a road does not need them, and with twenty cars about, forty shadowed
		// spotlights took a night street from forty frames a second to ten.
		Lamp->SetCastShadows(false);
		Lamp->SetVisibility(false);
		Headlights.Add(Lamp);
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
	PaintColour = FromRgb(Car.Build.PaintRgb);
	PaintFinish = Car.Build.PaintFinish;
	RimColour = FromRgb(Car.Build.WheelRgb);
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
	// Traffic gets a colour of its own, derived from the model id so the same car is always the same colour.
	const uint32 Hash = GetTypeHash(ModelId);
	PaintColour = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash & 0xFF), static_cast<uint8>(50 + (Hash >> 8) % 110), 185);
	RimColour = FLinearColor(0.55f, 0.55f, 0.57f);
	// Most traffic is ordinary gloss; a share of it is metallic, as on a real road.
	PaintFinish = (Hash >> 16) % 5 < 2 ? 1 : 0;
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
		FDTGSurfaceMeshes Meshes;
		DTGMeshBuilder::BuildCarBody(Setup.Body, LengthM, WidthM, HeightM, PaintColour, Meshes);
		// A reconfigured car may have a different set of surfaces, so nothing from the last build is kept.
		Shell->ClearAllMeshSections();
		Meshes.Apply(Shell, 0, false);
		// The finish the player paid for: gloss, metallic, matte, pearl or chrome.
		const int32 PaintSection = Meshes.SectionOf(EDTGSurface::Paint, 0);
		if (PaintSection != INDEX_NONE)
		{
			if (UMaterialInstanceDynamic* Finish = UMaterialInstanceDynamic::Create(DTGSurface::Material(EDTGSurface::Paint), this))
			{
				static const float Roughness[] = {0.16f, 0.28f, 0.65f, 0.2f, 0.05f};
				static const float Metallic[] = {0.05f, 0.7f, 0.0f, 0.35f, 1.0f};
				const int32 Pick = FMath::Clamp(PaintFinish, 0, 4);
				Finish->SetScalarParameterValue(TEXT("Roughness"), Roughness[Pick]);
				Finish->SetScalarParameterValue(TEXT("Metallic"), Metallic[Pick]);
				Shell->SetMaterial(PaintSection, Finish);
			}
		}
	}
	// Wheels sized from the tires actually fitted, so a car on bigger rubber looks like it.
	const double FrontRadius = FMath::Max(0.2, Setup.FrontTire.RadiusM);
	const double RearRadius = FMath::Max(0.2, Setup.RearTire.RadiusM);
	const double TireWidth = FMath::Clamp(0.22 * WidthM, 0.16, 0.42);
	for (int32 Index = 0; Index < Headlights.Num(); ++Index)
	{
		if (Headlights[Index])
		{
			Headlights[Index]->SetRelativeLocation(FVector(0.48 * LengthM * S, (Index == 0 ? -1 : 1) * 0.33 * WidthM * S,
				-0.1 * HeightM * S));
		}
	}
	for (int32 Index = 0; Index < WheelMeshes.Num(); ++Index)
	{
		if (!WheelMeshes[Index])
		{
			continue;
		}
		FDTGSurfaceMeshes Meshes;
		DTGMeshBuilder::BuildWheel(Index < 2 ? FrontRadius : RearRadius, TireWidth, RimColour, Meshes);
		WheelMeshes[Index]->ClearAllMeshSections();
		Meshes.Apply(WheelMeshes[Index], 0, false);
	}
}

void ADTGVehicle::PlaceAt(const FVector& WorldLocation, double YawDegrees)
{
	const Vec3 Ground(WorldLocation.X / DTGMetresToUnreal, WorldLocation.Y / DTGMetresToUnreal, WorldLocation.Z / DTGMetresToUnreal);
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

void ADTGVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// The player controller owns the bindings (Enhanced Input, created in code) and calls SetDriverInput.
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ADTGVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector Before = GetActorLocation();
	StepPhysics(FMath::Min(static_cast<double>(DeltaSeconds), 0.1));
	if (Sim.NumericalResets() > ReportedResets)
	{
		// The chassis rolled itself back. Say where, once per car, so the cause can be found.
		if (ReportedResets == 0)
		{
			UE_LOG(LogDTG, Warning, TEXT("%s: chassis blew up numerically near %s (%d wheels down, %s) and was rolled back."),
				*GetName(), *Before.ToString(), Sim.GroundedWheels(), bPlayerDriven ? TEXT("player") : TEXT("AI/parked"));
		}
		ReportedResets = Sim.NumericalResets();
	}
	ResolveCollisions(Before);
	if (!Sim.State.Position.IsFinite())
	{
		// The collision response runs outside the chassis's own guard. Whatever it produced, the engine
		// must never be handed a NaN transform: put the car back where it was this frame, at rest.
		Sim.State.Position = Vec3(Before.X / DTGMetresToUnreal, Before.Y / DTGMetresToUnreal, Before.Z / DTGMetresToUnreal);
		Sim.Freeze();
		UE_LOG(LogDTG, Warning, TEXT("%s: collision response produced a non-finite position near %s; restored."),
			*GetName(), *Before.ToString());
	}

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
		// The generated wheel already lies on its side, so this is simply spin and steer.
		const FRotator Rotation(FMath::RadiansToDegrees(Wheels[static_cast<std::size_t>(Index)].SpinAngle),
			FMath::RadiansToDegrees(Wheels[static_cast<std::size_t>(Index)].SteerAngle), 0.0);
		WheelMeshes[Index]->SetRelativeRotation(Rotation);
	}

	// Headlights come on when the sky says it is dark, and only for cars near enough to matter.
	if (!Headlights.IsEmpty())
	{
		if (!Sky)
		{
			Sky = Cast<ADTGSky>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGSky::StaticClass()));
		}
		bool bOn = Sky && Sky->IsNight();
		if (bOn)
		{
			// Near the camera, which in photo mode is not the player: every lit beam is a light the
			// whole view pays for, and one two hundred metres off is a dot.
			const APlayerController* Viewing = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
			const AActor* Viewer = Viewing ? Viewing->GetViewTarget() : nullptr;
			bOn = !Viewer || FVector::DistSquared(Viewer->GetActorLocation(), GetActorLocation()) < FMath::Square(180.0 * DTGMetresToUnreal);
		}
		for (TObjectPtr<USpotLightComponent>& Lamp : Headlights)
		{
			if (Lamp && Lamp->GetVisibleFlag() != bOn)
			{
				Lamp->SetVisibility(bOn);
			}
		}
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

	// Rain and standing water are felt through the tires, not just seen.
	const double Grip = Game && Game->IsContentLoaded() ? WeatherGrip(Game->Sim().Weather.Current()) : 1.0;
	const FWorldGround Ground(GetWorld(), this, Grip);
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
		// Road surfaces are driven on, so the body box may overlap them. The terrain excludes itself
		// from this channel; buildings, which belong to the same actor as the roads, do not.
		Params.AddIgnoredComponent(World->SurfacesComponent());
	}
	FHitResult Hit;
	const FQuat Rotation = ToUnrealQuat(Sim.State.Orientation);
	const FCollisionShape Shape = FCollisionShape::MakeBox(Body->GetScaledBoxExtent() * 0.9f);
	// Its own channel rather than Visibility: the engine's Pawn profile ignores Visibility, so a car
	// sweeping on it drives straight through every pedestrian in the state.
	if (!CurrentWorld->SweepSingleByChannel(Hit, PreviousLocation, Target, Rotation, DTGBodySweepChannel, Shape, Params))
	{
		return;
	}
	if (Hit.bStartPenetrating && Hit.PenetrationDepth <= UE_KINDA_SMALL_NUMBER)
	{
		return; // already touching and not pressing in: nothing new happened
	}
	// A sweep that starts inside something can come back with no usable normal -- zero, or NaN when the
	// depenetration fails. Pushing along that put a parked car's position to NaN every frame.
	if (Hit.ImpactNormal.ContainsNaN() || Hit.ImpactNormal.IsNearlyZero() || Hit.ImpactPoint.ContainsNaN() || Hit.Location.ContainsNaN())
	{
		return;
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
		// A real hit is property damage -- reported as seen only if somebody was there to see it, which
		// is what decides whether the police ever hear about it.
		const bool bSeen = Hit.GetActor() != nullptr && Hit.GetActor()->IsA(ACharacter::StaticClass());
		Game->Sim().ReportCrime(Crime::PropertyDamage, false, bSeen);
	}
	// Stop where the sweep stopped. PenetrationDepth is zero for an ordinary blocking hit, so backing
	// out by it left the car at the far end of the sweep -- inside the wall by however far it was
	// travelling that frame, which at speed is half a metre a frame and gets worse, not better.
	if (!Hit.bStartPenetrating)
	{
		const FVector Clear = Hit.Location + Hit.ImpactNormal * 2.0;
		Sim.State.Position = Vec3(Clear.X / DTGMetresToUnreal, Clear.Y / DTGMetresToUnreal, Clear.Z / DTGMetresToUnreal);
	}
	else
	{
		// Already overlapping: push straight back out along the normal by how deep it is.
		const double PushM = (Hit.PenetrationDepth > 0.0 ? Hit.PenetrationDepth : 2.0) / DTGMetresToUnreal;
		Sim.State.Position += Normal * PushM;
	}
}
