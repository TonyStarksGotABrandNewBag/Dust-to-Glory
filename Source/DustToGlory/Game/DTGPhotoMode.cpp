#include "Game/DTGPhotoMode.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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

#include "Game/DTGGameSubsystem.h"
#include "Game/DTGVehicle.h"
#include "Game/DTGWorldBuilder.h"

using namespace dtg::sim;

namespace detail_photo
{
	/** Long enough for the chunks under a new viewpoint to stream in and the lighting to settle. */
	constexpr double SettleSeconds = 5.0;
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
	if (Now < NextAt)
	{
		return;
	}
	if (!bCaptured)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Photos"), Shots[Current].Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bInShowUI*/ false, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogDTG, Log, TEXT("Photo mode: %s"), *Path);
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
	Frame(Shots[Current]);
	NextAt = Now + detail_photo::SettleSeconds;
}

void ADTGPhotoMode::Frame(const FDTGPhotoShot& Shot)
{
	SetActorLocationAndRotation(Shot.At, Shot.Facing);
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
}
