#include "Game/DTGPhotoMode.h"

#include "DustToGlory.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include <array>
#include <vector>

#include "Sim/StreetFurniture.h"

#include "EngineUtils.h"

#include "Game/DTGBody.h"
#include "Game/DTGCharacter.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGVehicle.h"
#include "Game/DTGWorldBuilder.h"

using namespace dtg::sim;

// The engine keeps its running frame-rate average in UnrealEngine.cpp with no public declaration, so every
// reader declares it, as the engine's own analytics does.
extern ENGINE_API float GAverageFPS;

namespace detail_photo
{
	/** Long enough for the chunks under a new viewpoint to stream in and the lighting to settle. */
	constexpr double SettleSeconds = 5.0;
	/** The settle, or -DTGSettle=N seconds: chunks stream in for a while after the camera jumps, and a
	 * profile taken while they do measures the streaming, not the view. */
	double Settle()
	{
		static double Seconds = -1.0;
		if (Seconds < 0.0)
		{
			Seconds = SettleSeconds;
			FParse::Value(FCommandLine::Get(), TEXT("DTGSettle="), Seconds);
		}
		return Seconds;
	}
	/** A walker is framed where they will be after this long, so it is short: the further ahead, the more they turn or stop. */
	constexpr double WalkerSettleSeconds = 2.0;
	/** The first shot also waits out shader compilation on a fresh machine. */
	constexpr double FirstSettleSeconds = 12.0;

	FRotator Looking(const FVector& From, const FVector& To)
	{
		return (To - From).Rotation();
	}
}

ADTGPhotoMode::ADTGPhotoMode()
{
	PrimaryActorTick.bCanEverTick = true;
	// The camera is this actor: a view target with no camera component is seen from its own location
	// and rotation, so moving the actor is moving the camera.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool ADTGPhotoMode::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("DTGPhoto"));
}

void ADTGPhotoMode::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	if (!Game || !Game->IsContentLoaded())
	{
		UE_LOG(LogDTG, Error, TEXT("Photo mode: no content loaded, nothing to photograph."));
		FPlatformMisc::RequestExit(false);
		return;
	}
	PlanShots();
	if (Shots.IsEmpty())
	{
		UE_LOG(LogDTG, Error, TEXT("Photo mode: found no viewpoints."));
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		if (AHUD* Hud = Controller->GetHUD())
		{
			Hud->bShowHUD = false;
		}
		if (APawn* Player = Controller->GetPawn())
		{
			PlayerPawn = Player;
			Player->SetActorHiddenInGame(true);
			Controller->UnPossess();
		}
		Controller->SetViewTarget(this);
	}
	Current = 0;
	bCaptured = false;
	Frame(Shots[0]);
	NextAt = GetWorld()->GetTimeSeconds() + detail_photo::FirstSettleSeconds;
	UE_LOG(LogDTG, Log, TEXT("Photo mode: %d viewpoints"), Shots.Num());
}

void ADTGPhotoMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Current == INDEX_NONE || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// Every frame, not once per shot: something else in the game (an event with a camera of its own, or
	// the controller settling on its pawn) took the view back at noon and the shot was the player's.
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController(); Controller && Controller->GetViewTarget() != this)
	{
		UE_LOG(LogDTG, Log, TEXT("Photo mode: the view target had become %s; taking it back."),
			Controller->GetViewTarget() ? *Controller->GetViewTarget()->GetName() : TEXT("nothing"));
		Controller->SetViewTarget(this);
	}
	if (Now < NextAt)
	{
		return;
	}
	if (!bCaptured && Shots[Current].bWalker && !bWalkerFound && WalkerTries < 30)
	{
		// Nobody walking yet: look again in a second, for up to half a minute, before settling for the view.
		++WalkerTries;
		Frame(Shots[Current]);
		NextAt = Now + (bWalkerFound ? detail_photo::WalkerSettleSeconds : 1.0);
		return;
	}
	if (!bCaptured)
	{
		UE_LOG(LogDTG, Log, TEXT("Photo mode: %s shot from (%.1f, %.1f, %.1f) m facing yaw %.0f; planned (%.1f, %.1f, %.1f) yaw %.0f"), *Shots[Current].Name,
			GetActorLocation().X / DTGMetresToUnreal, GetActorLocation().Y / DTGMetresToUnreal, GetActorLocation().Z / DTGMetresToUnreal, GetActorRotation().Yaw,
			Shots[Current].At.X / DTGMetresToUnreal, Shots[Current].At.Y / DTGMetresToUnreal, Shots[Current].At.Z / DTGMetresToUnreal, Shots[Current].Facing.Yaw);
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Photos"), Shots[Current].Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bInShowUI*/ false, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogDTG, Log, TEXT("Photo mode: %s (%.0f fps)"), *Path, GAverageFPS);
		// Once a run: the building shells must have collision. It is cooked once after every section is in
		// (ADTGWorldBuilder::CookBuildingCollision), which is exactly the kind of thing that breaks without a
		// sign, so a trace from above the first building must stop at its roof and not at the ground.
		if (Current == 0 && Game && Game->IsContentLoaded() && !Game->Sim().City.Buildings.empty())
		{
			const dtg::sim::BuildingPlan& Building = Game->Sim().City.Buildings.front();
			const double Cx = Building.Footprint.CenterX();
			const double Cy = Building.Footprint.CenterY();
			const double GroundM = Game->Sim().Map.HeightAt(Cx, Cy);
			const double RoofM = GroundM + FMath::Max(3.0, Building.HeightM);
			FHitResult Hit;
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, FVector(Cx * DTGMetresToUnreal, Cy * DTGMetresToUnreal, (RoofM + 60.0) * DTGMetresToUnreal),
				FVector(Cx * DTGMetresToUnreal, Cy * DTGMetresToUnreal, (GroundM - 5.0) * DTGMetresToUnreal), ECC_Visibility);
			const double HitM = bHit ? Hit.ImpactPoint.Z / DTGMetresToUnreal : GroundM - 5.0;
			if (HitM < RoofM - 1.5)
			{
				UE_LOG(LogDTG, Warning, TEXT("Photo mode: building collision MISSING: the trace over building %d stopped at %.1f m, roof at %.1f m, ground at %.1f m."),
					Building.Id, HitM, RoofM, GroundM);
			}
			else
			{
				UE_LOG(LogDTG, Log, TEXT("Photo mode: building collision ok: the trace over building %d stopped at %.1f m on %s, roof at %.1f m."),
					Building.Id, HitM, Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("?"), RoofM);
			}
		}
		// -DTGMemReport: what is in memory at the car shot, to Saved/Profiling/MemReports, for when the GPU is full.
		if (GEngine && FParse::Param(FCommandLine::Get(), TEXT("DTGGpuProfile")))
		{
			// A GPU profile of this settled frame, per pass, into the log (LogRHI "GPU" lines), so a change
			// can be measured against the same view rather than against a feeling.
			UE_LOG(LogDTG, Log, TEXT("Photo mode: GPU profile of the %s view follows."), *Shots[Current].Name);
			GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
		}
		if (GEngine && Shots[Current].Name == TEXT("car") && FParse::Param(FCommandLine::Get(), TEXT("DTGMemReport")))
		{
			GEngine->Exec(GetWorld(), TEXT("MemReport -full"));
		}
		bCaptured = true;
		NextAt = Now + 1.0; // the request is served on a later frame
		return;
	}
	++Current;
	if (!Shots.IsValidIndex(Current))
	{
		UE_LOG(LogDTG, Log, TEXT("Photo mode: done"));
		Current = INDEX_NONE;
		FPlatformMisc::RequestExit(false);
		return;
	}
	bCaptured = false;
	bWalkerFound = false;
	WalkerTries = 0;
	Frame(Shots[Current]);
	// A shot that moves the clock gets twice as long: the camera has to re-adapt, the lit windows' shaders
	// have to compile, and the frame rate logged with it should be the settled one.
	// A forced weather takes longest: the sky, the fog and the rain all ease towards it.
	NextAt = Now + (Shots[Current].bWalker && bWalkerFound ? detail_photo::WalkerSettleSeconds
		: (Shots[Current].ForceWeather >= 0 ? 3.0 : (Shots[Current].ClockMinutes >= 0.0 ? 2.0 : 1.0)) * detail_photo::Settle());
}

namespace detail_photo
{
	/**
	 * Whether a spot (Unreal centimetres) is on a building's lot or within a stride of it: a doorway, or
	 * crossing the lot. From inside a shell the one-sided walls are not there, and a shot from there came
	 * out as a row of window frames in a field, so neither the subject nor the eye goes there. Street
	 * pedestrians are plain characters dressed by the traffic director, so OnTheStreet takes any
	 * character, and asks the project's own kind whether it is indoors.
	 */
	bool OnALot(const FVector& Where, const CityPlan& City)
	{
		const double X = Where.X / DTGMetresToUnreal;
		const double Y = Where.Y / DTGMetresToUnreal;
		for (const BuildingPlan& Building : City.Buildings)
		{
			const dtg::sim::Rect& Lot = Building.Footprint;
			if (X > Lot.MinX - 1.5 && X < Lot.MaxX + 1.5 && Y > Lot.MinY - 1.5 && Y < Lot.MaxY + 1.5)
			{
				return true;
			}
		}
		return false;
	}

	bool OnTheStreet(const ACharacter& Person, const CityPlan& City)
	{
		const ADTGCharacter* const Ours = Cast<ADTGCharacter>(&Person);
		return !(Ours && Ours->IsIndoors()) && !OnALot(Person.GetActorLocation(), City);
	}
}

void ADTGPhotoMode::Frame(const FDTGPhotoShot& Shot)
{
	if (Shot.ClockMinutes >= 0.0 && Game && Game->IsContentLoaded())
	{
		auto& Clock = Game->Sim().Clock;
		Clock.SetTotalMinutes(Clock.Day() * 1440.0 + Shot.ClockMinutes);
	}
	if (Shot.ForceWeather >= 0 && Game && Game->IsContentLoaded())
	{
		// Held for half an hour of game time: long enough for the shot, gone before the next clock jump.
		Game->Sim().Weather.Force(static_cast<Weather>(Shot.ForceWeather), Game->Sim().Clock.TotalMinutes(), 30);
	}
	FVector At = Shot.At;
	FRotator Facing = Shot.Facing;
	bool bShowPlayer = Shot.bShowPlayer;
	if ((Shot.bPerson || Shot.bWalker) && Game)
	{
		// The nearest person out on the street (walking, for the walker shot), from the front and a little
		// to the side, framed where they will be when the shot is taken, so the stride can be judged against
		// the ground; what the body says it is doing goes in the log. The eye keeps off the lots too.
		const CityPlan& City = Game->Sim().City;
		const ACharacter* Subject = nullptr;
		double Nearness = 1e18;
		for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
		{
			if (*It == PlayerPawn || (Shot.bWalker && It->GetVelocity().Size2D() < 60.0) || !detail_photo::OnTheStreet(**It, City))
			{
				continue;
			}
			const double Distance = FVector::DistSquared(It->GetActorLocation(), Shot.At);
			if (Distance < Nearness)
			{
				Nearness = Distance;
				Subject = *It;
			}
		}
		bWalkerFound = Subject != nullptr;
		if (!Subject && Shot.bPerson)
		{
			// Nobody about yet: the player, shown for this one shot.
			Subject = Cast<ACharacter>(PlayerPawn.Get());
			bShowPlayer = Subject != nullptr;
		}
		if (Subject)
		{
			const double Settle = Shot.bWalker ? detail_photo::WalkerSettleSeconds : detail_photo::Settle();
			const FVector Velocity = Subject->GetVelocity();
			const bool bMoving = Velocity.Size2D() >= 60.0;
			const FVector Centre = Subject->GetActorLocation() + (bMoving ? FVector(Velocity.X, Velocity.Y, 0.0) * Settle : FVector::ZeroVector);
			const FVector Ahead = bMoving ? Velocity.GetSafeNormal2D() : Subject->GetActorForwardVector().GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
			const FVector Rise(0.0, 0.0, 110.0);
			const FVector Options[] = {Ahead * 450.0 + Side * 200.0, Ahead * 450.0 - Side * 200.0, Side * 480.0, -Side * 480.0, Ahead * 480.0};
			At = Centre + Options[0] + Rise;
			for (const FVector& Option : Options)
			{
				if (!detail_photo::OnALot(Centre + Option, City))
				{
					At = Centre + Option + Rise;
					break;
				}
			}
			Facing = detail_photo::Looking(At, Centre + FVector(0.0, 0.0, 90.0));
			const UDTGBody* const Body = Subject->FindComponentByClass<UDTGBody>();
			UE_LOG(LogDTG, Log, TEXT("Photo mode: %s %s moving at %.0f cm/s: %s"), *Shot.Name, *Subject->GetName(), Velocity.Size2D(),
				Body ? *Body->Describe() : TEXT("no body"));
		}
		else if (Shot.bWalker && WalkerTries >= 30)
		{
			UE_LOG(LogDTG, Warning, TEXT("Photo mode: no pedestrian walking outdoors for the walker shot in %d seconds."), WalkerTries);
			for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
			{
				const UDTGBody* const Body = It->FindComponentByClass<UDTGBody>();
				UE_LOG(LogDTG, Log, TEXT("Photo mode:   %s%s at %.0f cm/s, %s: %s"), *It->GetName(), *It == PlayerPawn ? TEXT(" (player)") : TEXT(""),
					It->GetVelocity().Size2D(), detail_photo::OnTheStreet(**It, City) ? TEXT("on the street") : TEXT("indoors or on a lot"),
					Body ? *Body->Describe() : TEXT("no body"));
			}
		}
	}
	SetActorLocationAndRotation(At, Facing);
	if (PlayerPawn)
	{
		// Unpossessed in BeginPlay, so still standing where it started.
		PlayerPawn->SetActorHiddenInGame(!bShowPlayer);
	}
	if (ADTGWorldBuilder* Builder = Cast<ADTGWorldBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGWorldBuilder::StaticClass())))
	{
		Builder->EnsureGroundAt(Shot.At);
	}
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->SetViewTarget(this);
	}
}

void ADTGPhotoMode::PlanShots()
{
	const CityPlan& City = Game->Sim().City;
	const StatePlan& Map = Game->Sim().Map;
	const double SpawnX = City.SpawnX;
	const double SpawnY = City.SpawnY;
	auto Ground = [&Map](double X, double Y) { return Map.HeightAt(X, Y); };
	auto At = [this](double X, double Y, double Z) { return Game->ToUnreal(X, Y, Z); };

	// A street: the starting town's nearest, from one end, at eye height, looking down it.
	const RoadPlan* Street = nullptr;
	double Best = 1e18;
	for (const RoadPlan& Road : City.Roads)
	{
		const double Dx = Road.Area.CenterX() - SpawnX;
		const double Dy = Road.Area.CenterY() - SpawnY;
		if (Dx * Dx + Dy * Dy < Best)
		{
			Best = Dx * Dx + Dy * Dy;
			Street = &Road;
		}
	}
	if (Street)
	{
		const dtg::sim::Rect& Area = Street->Area;
		const double X = Street->bHorizontal ? Area.MinX + 0.08 * Area.Width() : Area.CenterX() + 0.2 * Area.Width();
		const double Y = Street->bHorizontal ? Area.CenterY() + 0.2 * Area.Depth() : Area.MinY + 0.08 * Area.Depth();
		const FVector Eye = At(X, Y, Ground(X, Y) + 1.7);
		Shots.Add({TEXT("street"), Eye, FRotator(-2.0, Street->bHorizontal ? 0.0 : 90.0, 0.0)});
	}

	// A house and a downtown tower, each from in front of its door: the nearest of each to the start.
	auto Frontage = [&](const BuildingPlan& Building, double AwayM, double SideM, double EyeM, double LookAtM, const TCHAR* Name)
	{
		static const double OutX[] = {0.0, 1.0, 0.0, -1.0};
		static const double OutY[] = {-1.0, 0.0, 1.0, 0.0};
		const int32 Side = FMath::Clamp(Building.DoorSide, 0, 3);
		const double Cx = Building.Footprint.CenterX();
		const double Cy = Building.Footprint.CenterY();
		const double Ex = Building.DoorX + OutX[Side] * AwayM - OutY[Side] * SideM;
		const double Ey = Building.DoorY + OutY[Side] * AwayM + OutX[Side] * SideM;
		const FVector Eye = At(Ex, Ey, Ground(Ex, Ey) + EyeM);
		Shots.Add({Name, Eye, detail_photo::Looking(Eye, At(Cx, Cy, Ground(Cx, Cy) + LookAtM))});
	};
	const BuildingPlan* House = nullptr;
	const BuildingPlan* Tower = nullptr;
	double HouseBest = 1e18;
	double TowerBest = 1e18;
	for (const BuildingPlan& Building : City.Buildings)
	{
		const double Dx = Building.Footprint.CenterX() - SpawnX;
		const double Dy = Building.Footprint.CenterY() - SpawnY;
		const double Distance = Dx * Dx + Dy * Dy;
		if (Building.Role == LotRole::Home && Distance < HouseBest)
		{
			HouseBest = Distance;
			House = &Building;
		}
		if (Building.bDowntown && Building.Floors >= 5 && Distance < TowerBest)
		{
			TowerBest = Distance;
			Tower = &Building;
		}
	}
	if (House)
	{
		Frontage(*House, 13.0, 5.0, 1.7, 2.5, TEXT("house"));
	}
	if (Tower)
	{
		Frontage(*Tower, 45.0, 12.0, 1.8, 18.0, TEXT("downtown"));
	}

	// The town from above, looking across it.
	{
		const FVector Target = At(SpawnX, SpawnY, Ground(SpawnX, SpawnY));
		const FVector Eye = At(SpawnX - 90.0, SpawnY - 90.0, Ground(SpawnX, SpawnY) + 45.0);
		Shots.Add({TEXT("town"), Eye, detail_photo::Looking(Eye, Target)});
	}

	// A highway: the nearest stretch, in the right-hand lane looking along it, and from above beside it.
	const HighwayPlan* Highway = nullptr;
	double Along = 0.0;
	Best = 1e18;
	for (const HighwayPlan& Road : City.Highways)
	{
		const double Length = Road.LengthM();
		for (double Distance = 0.0; Distance < Length; Distance += 50.0)
		{
			const std::array<double, 3> Here = Road.At(Distance);
			const double Dx = Here[0] - SpawnX;
			const double Dy = Here[1] - SpawnY;
			if (Dx * Dx + Dy * Dy < Best)
			{
				Best = Dx * Dx + Dy * Dy;
				Highway = &Road;
				Along = Distance;
			}
		}
	}
	if (Highway)
	{
		const std::array<double, 3> Here = Highway->At(Along);
		const std::array<double, 3> Ahead = Highway->At(FMath::Min(Highway->LengthM(), Along + 30.0));
		double Dx = Ahead[0] - Here[0];
		double Dy = Ahead[1] - Here[1];
		const double Len = FMath::Max(1e-6, FMath::Sqrt(Dx * Dx + Dy * Dy));
		Dx /= Len;
		Dy /= Len;
		const double Lane = 0.25 * Highway->WidthM;
		const FVector Eye = At(Here[0] - Dy * Lane, Here[1] + Dx * Lane, Here[2] + 1.5);
		Shots.Add({TEXT("highway"), Eye, FRotator(-1.0, FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx)), 0.0)});
		const FVector High = At(Here[0] - Dy * 40.0 - Dx * 30.0, Here[1] + Dx * 40.0 - Dy * 30.0, Here[2] + 30.0);
		const FVector Target = At(Here[0] + Dx * 60.0, Here[1] + Dy * 60.0, Here[2]);
		Shots.Add({TEXT("highway-above"), High, detail_photo::Looking(High, Target)});
	}

	// A beach: the nearest dry sand to the start, looking out to sea.
	bool bBeach = false;
	for (double Radius = 200.0; Radius < 60000.0 && !bBeach; Radius += 200.0)
	{
		for (int32 Step = 0; Step < 24 && !bBeach; ++Step)
		{
			const double Angle = 2.0 * UE_PI * Step / 24.0;
			const double X = SpawnX + Radius * FMath::Cos(Angle);
			const double Y = SpawnY + Radius * FMath::Sin(Angle);
			const SurfaceInfo Info = Map.SurfaceAt(X, Y);
			if (Info.bWater || Info.GroundM > 1.2 || Map.CoastDistance(X, Y) > 60.0)
			{
				continue;
			}
			for (int32 Look = 0; Look < 16; ++Look)
			{
				const double Heading = 2.0 * UE_PI * Look / 16.0;
				const SurfaceInfo Out = Map.SurfaceAt(X + 150.0 * FMath::Cos(Heading), Y + 150.0 * FMath::Sin(Heading));
				if (Out.bWater && Out.bOcean)
				{
					Shots.Add({TEXT("beach"), At(X, Y, Info.GroundM + 1.8), FRotator(-4.0, FMath::RadiansToDegrees(Heading), 0.0)});
					bBeach = true;
					break;
				}
			}
		}
	}

	// A country road: the nearest run of utility poles outside town, from the lane, looking down the lines.
	{
		FurnitureRules Rules;
		Rules.Limit = 100000;
		bool bFound = false;
		for (int32 Ring = 1; Ring <= 8 && !bFound; ++Ring)
		{
			for (int32 Step = 0; Step < 8 && !bFound; ++Step)
			{
				const double Angle = Step * 0.785398;
				const double Cx = SpawnX + Ring * 2000.0 * FMath::Cos(Angle);
				const double Cy = SpawnY + Ring * 2000.0 * FMath::Sin(Angle);
				const dtg::sim::Rect Patch{Cx - 400.0, Cy - 400.0, Cx + 400.0, Cy + 400.0};
				std::vector<Prop> Poles;
				for (const Prop& Value : FurnitureIn(Map, City, Patch, Rules))
				{
					if (Value.Kind == PropKind::UtilityPole)
					{
						Poles.push_back(Value);
					}
				}
				if (Poles.size() < 6)
				{
					continue;
				}
				// Back down the road from the first pole, moved in off the verge onto the near lane.
				const Prop& Pole = Poles[Poles.size() / 2];
				const double Dx = FMath::Cos(Pole.Yaw);
				const double Dy = FMath::Sin(Pole.Yaw);
				const double In = Rules.PoleSetbackM + 4.0;
				const double X = Pole.X - Dx * 28.0 + Dy * In;
				const double Y = Pole.Y - Dy * 28.0 - Dx * In;
				Shots.Add({TEXT("country-road"), At(X, Y, Map.RoadHeightAt(X, Y) + 1.6), FRotator(2.0, FMath::RadiansToDegrees(Pole.Yaw), 0.0)});
				bFound = true;
			}
		}
	}

	// The start town itself, from the middle of its main road looking through the crossroads, at noon,
	// at golden hour, at night and in heavy rain: the vertical slice the look is judged on.
	for (const CityInfo& Town : City.Cities)
	{
		if (!Town.bStart)
		{
			continue;
		}
		// From the road's shoulder, not its middle, where a car queues on the camera.
		const double X = Town.Bounds.CenterX() + 4.2;
		const double Y = Town.Bounds.CenterY() - 70.0;
		const FVector Eye = At(X, Y, Map.RoadHeightAt(X, Y) + 1.6);
		const FRotator Look(-2.0, 94.0, 0.0);
		struct FCondition { const TCHAR* Name; double Minutes; Weather Sky; };
		const FCondition Conditions[] = {
			{TEXT("longs-noon"), 12.5 * 60.0, Weather::Clear}, {TEXT("longs-golden"), 18.6 * 60.0, Weather::Clear},
			{TEXT("longs-night"), 23.0 * 60.0, Weather::Clear}, {TEXT("longs-rain"), 14.0 * 60.0, Weather::Rain}};
		for (const FCondition& Condition : Conditions)
		{
			FDTGPhotoShot Shot{Condition.Name, Eye, Look};
			Shot.ClockMinutes = Condition.Minutes;
			Shot.ForceWeather = static_cast<int>(Condition.Sky);
			Shots.Add(Shot);
		}
		// One lot, close: from 16 m above the road's edge looking down the drive to the house.
		for (const BuildingPlan& Building : City.Buildings)
		{
			if (!Town.Bounds.Contains(Building.Footprint.CenterX(), Building.Footprint.CenterY()) || Building.Role != LotRole::Home)
			{
				continue;
			}
			const double Hx = Building.Footprint.CenterX();
			const double Hy = Building.Footprint.CenterY();
			// Stand off on the door's side, 22 m out and 16 m up, looking at the house.
			const double Ox = Building.DoorSide == 1 ? 22.0 : (Building.DoorSide == 3 ? -22.0 : 0.0);
			const double Oy = Building.DoorSide == 2 ? 22.0 : (Building.DoorSide == 0 ? -22.0 : 0.0);
			const FVector LotEye = At(Hx + Ox, Hy + Oy, Map.HeightAt(Hx + Ox, Hy + Oy) + 16.0);
			FDTGPhotoShot LotShot{TEXT("longs-lot"), LotEye, detail_photo::Looking(LotEye, At(Hx, Hy, Map.HeightAt(Hx, Hy) + 2.0))};
			LotShot.ClockMinutes = 12.5 * 60.0;
			LotShot.ForceWeather = static_cast<int>(Weather::Clear);
			Shots.Add(LotShot);
			break;
		}
		// And the same crossroads from 45 m up, for the lots: drives, pads, fences and sheds.
		FDTGPhotoShot Above{TEXT("longs-above"), At(X, Y - 30.0, Map.RoadHeightAt(X, Y) + 45.0), FRotator(-55.0, 90.0, 0.0)};
		Above.ClockMinutes = 12.5 * 60.0;
		Above.ForceWeather = static_cast<int>(Weather::Clear);
		Shots.Add(Above);
	}

	// A person: the nearest out on the street when the shot comes up (framed then, in Frame), or the player
	// when nobody is about yet.
	{
		FDTGPhotoShot Person{TEXT("person"), At(SpawnX, SpawnY, Ground(SpawnX, SpawnY) + 1.6), FRotator::ZeroRotator};
		Person.bPerson = true;
		Shots.Add(Person);
		// And someone walking, found when the shot comes up: nobody is yet when the tour is planned.
		FDTGPhotoShot Walker{TEXT("walker"), At(SpawnX, SpawnY, Ground(SpawnX, SpawnY) + 1.6), FRotator::ZeroRotator};
		Walker.bWalker = true;
		Shots.Add(Walker);
	}

	// The same street and highway again at golden hour, the light the game is meant to be seen in.
	{
		const int32 Count = Shots.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Shots[Index].Name == TEXT("street") || Shots[Index].Name == TEXT("highway-above"))
			{
				FDTGPhotoShot Golden = Shots[Index];
				Golden.Name = TEXT("golden-") + Golden.Name;
				Golden.ClockMinutes = 19.0 * 60.0 + 5.0;
				Shots.Add(Golden);
			}
		}
	}

	// A car, from the front three-quarter: one parked for the purpose in the open street, because the
	// starter car stands against a building and a camera put beside it ends up inside the wall.
	if (Street)
	{
		const dtg::sim::Rect& Area = Street->Area;
		const double X = Street->bHorizontal ? Area.MinX + 0.25 * Area.Width() : Area.CenterX() - 0.15 * Area.Width();
		const double Y = Street->bHorizontal ? Area.CenterY() - 0.15 * Area.Depth() : Area.MinY + 0.25 * Area.Depth();
		const double Yaw = Street->bHorizontal ? 0.0 : 90.0;
		const VehicleModel* Model = Game->Sim().Vehicles.NewestOfBody(BodyStyle::Sports);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTGVehicle* Car = Model ? GetWorld()->SpawnActor<ADTGVehicle>(ADTGVehicle::StaticClass(), FTransform::Identity, Params) : nullptr;
		if (Car)
		{
			Car->ConfigureFromModel(UTF8_TO_TCHAR(Model->Id.c_str()));
			const FVector Spot = At(X, Y, Ground(X, Y));
			Car->PlaceAt(Spot, Yaw);
			const FVector Forward = FRotator(0.0, Yaw, 0.0).Vector();
			const FVector Right = FRotator(0.0, Yaw + 90.0, 0.0).Vector();
			const FVector Centre = Spot + FVector(0.0, 0.0, 60.0);
			const FVector Eye = Centre + Forward * 560.0 + Right * 380.0 + FVector(0.0, 0.0, 70.0);
			Shots.Add({TEXT("car"), Eye, detail_photo::Looking(Eye, Centre)});
		}
	}

	// The kerb up close: the nearest block corner to the spawn, from the street, low, with the curb stones
	// running off along both edges and whatever the corner holds -- a hydrant or a trash can.
	{
		const dtg::sim::Rect* Nearest = nullptr;
		double Nearness = 1e18;
		for (const dtg::sim::Rect& Block : City.Blocks)
		{
			const double Distance = FMath::Square(Block.MinX - SpawnX) + FMath::Square(Block.MinY - SpawnY);
			if (Distance < Nearness)
			{
				Nearness = Distance;
				Nearest = &Block;
			}
		}
		if (Nearest)
		{
			const double X = Nearest->MinX;
			const double Y = Nearest->MinY;
			// The corner from the street, and then the kerb itself from a metre out, knee high, looking along it.
			const FVector CornerEye = At(X - 4.5, Y - 4.5, Ground(X - 4.5, Y - 4.5) + 1.5);
			const FVector CornerAt = At(X + 1.5, Y + 1.5, Ground(X, Y) + 0.3);
			Shots.Add({TEXT("corner"), CornerEye, detail_photo::Looking(CornerEye, CornerAt)});
			const FVector KerbEye = At(X - 1.3, Y + 2.0, Ground(X - 1.3, Y + 2.0) + 0.7);
			const FVector KerbAt = At(X + 0.1, Y + 7.0, Ground(X, Y + 7.0) + 0.1);
			Shots.Add({TEXT("kerb"), KerbEye, detail_photo::Looking(KerbEye, KerbAt)});
		}
	}

	// And the street and the towers after dark: lit windows, lamps, moonlight. Last, because the camera
	// takes a while to open back up from the dark, and the daylight shots would come out washed out.
	{
		const int32 Count = Shots.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Shots[Index].Name == TEXT("street") || Shots[Index].Name == TEXT("downtown"))
			{
				FDTGPhotoShot Night = Shots[Index];
				Night.Name = TEXT("night-") + Night.Name;
				Night.ClockMinutes = 21.0 * 60.0 + 40.0;
				Shots.Add(Night);
			}
		}
	}

	// -DTGPhotoShots=car,street keeps only those, for a quick look at one thing without the full tour.
	FString Only;
	if (FParse::Value(FCommandLine::Get(), TEXT("DTGPhotoShots="), Only, /*bShouldStopOnSeparator*/ false) && !Only.IsEmpty())
	{
		TArray<FString> Wanted;
		Only.ParseIntoArray(Wanted, TEXT(","), true);
		Shots.RemoveAll([&Wanted](const FDTGPhotoShot& Shot) { return !Wanted.Contains(Shot.Name); });
	}
}
