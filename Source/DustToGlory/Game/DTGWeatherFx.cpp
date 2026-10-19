#include "Game/DTGWeatherFx.h"

#include "DustToGlory.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Math/RotationMatrix.h"
#include "ProceduralMeshComponent.h"

#include "EngineUtils.h"

#include "Sim/Clock.h"

#include "Game/DTGGameSubsystem.h"
#include "Game/DTGCharacter.h"
#include "Game/DTGInterior.h"
#include "Game/DTGMeshBuilder.h"
#include "Game/DTGSky.h"
#include "Game/DTGWeatherAudio.h"

using namespace dtg::sim;

namespace
{
	/**
	 * One segment of a streak: a closed four-sided tube down the local Z.
	 *
	 * It has to be closed. Quads are single-sided and the vertex-colour material is not two-sided, so
	 * two quads crossed in an X vanish entirely when the camera is on the wrong side of both of them --
	 * which, with the box centred on the camera, means the rain disappears from whole bearings at a
	 * time. A tube is front-facing from every direction outside it, so it is there whichever way the
	 * player turns.
	 */
	void AddTube(FDTGMesh& Out, const FVector& Top, double LengthUu, double WidthUu, const FLinearColor& Colour)
	{
		const double Drop = LengthUu;
		const FVector A(Top.X - WidthUu, Top.Y - WidthUu, Top.Z);
		const FVector B(Top.X + WidthUu, Top.Y - WidthUu, Top.Z);
		const FVector C(Top.X + WidthUu, Top.Y + WidthUu, Top.Z);
		const FVector D(Top.X - WidthUu, Top.Y + WidthUu, Top.Z);
		const FVector Down(0.0, 0.0, -Drop);
		Out.AddQuad(A, B, B + Down, A + Down, FVector(0.0, -1.0, 0.0), Colour, 0.02);
		Out.AddQuad(B, C, C + Down, B + Down, FVector(1.0, 0.0, 0.0), Colour, 0.02);
		Out.AddQuad(C, D, D + Down, C + Down, FVector(0.0, 1.0, 0.0), Colour, 0.02);
		Out.AddQuad(D, A, A + Down, D + Down, FVector(-1.0, 0.0, 0.0), Colour, 0.02);
	}

	/**
	 * One streak, straight down the local Z. It is long because real rain is a motion-blurred line
	 * rather than a drop, and it is built in two segments so the head can be brighter than the tail --
	 * the engine's vertex-colour material is opaque, so a falloff has to be made of pieces rather than
	 * of alpha, and a quad carries only one colour. A translucent material with a gradient along its
	 * length is one of the things real art would bring.
	 *
	 * The lean comes from rotating the whole layer at runtime rather than from the geometry, so the
	 * wind -- and the player's own speed -- can change it.
	 */
	void AddStreak(FDTGMesh& Out, const FVector& Top, double LengthUu, double WidthUu, const FLinearColor& Colour)
	{
		const double HeadLength = LengthUu * 0.4;
		const FLinearColor Tail(Colour.R * 0.5f, Colour.G * 0.5f, Colour.B * 0.58f, 1.0f);
		AddTube(Out, Top, HeadLength, WidthUu, Colour);
		AddTube(Out, Top - FVector(0.0, 0.0, HeadLength), LengthUu - HeadLength, WidthUu * 0.8, Tail);
	}
}

ADTGWeatherFx::ADTGWeatherFx()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the camera has moved for the frame, so the rain is around where the player is actually looking.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	for (int32 Index = 0; Index < LayerCount; ++Index)
	{
		UProceduralMeshComponent* Layer = CreateDefaultSubobject<UProceduralMeshComponent>(
			*FString::Printf(TEXT("RainLayer%d"), Index));
		Layer->SetupAttachment(Root);
		Layer->SetMobility(EComponentMobility::Movable);
		// Rain is to be looked through, not bumped into, lit, or shadowed.
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->bUseAsOccluder = false;
		Layer->SetVisibility(false);
		StreakLayers.Add(Layer);
	}

	// Rain and thunder, synthesised rather than played, and heard wherever the listener is.
	Sound = CreateDefaultSubobject<UDTGWeatherAudio>(TEXT("WeatherAudio"));
	Sound->SetupAttachment(Root);
}

void ADTGWeatherFx::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	// The sky owns the lightning, because it is the sky that lights up; this listens for the flash.
	for (TActorIterator<ADTGSky> It(GetWorld()); It; ++It)
	{
		Sky = *It;
		break;
	}

	// Built once. Each layer is a different size and falls at a different speed, so they wrap out of
	// phase and the near one reads as near.
	const double Scales[LayerCount] = {1.0, 0.62, 0.38};
	// Paler and dimmer with distance, which is the only depth cue an opaque material gives.
	const FLinearColor Colours[LayerCount] = {
		FLinearColor(0.78f, 0.84f, 0.92f, 1.0f),
		FLinearColor(0.60f, 0.66f, 0.75f, 1.0f),
		FLinearColor(0.46f, 0.51f, 0.60f, 1.0f)};
	const double LayerSpeeds[LayerCount] = {1.0, 0.78, 0.6};
	Scroll.Empty();
	Speeds.Empty();
	for (int32 Index = 0; Index < LayerCount; ++Index)
	{
		BuildLayer(Index, Scales[Index], Colours[Index]);
		Scroll.Add(static_cast<double>(Index) * CellHeightM * DTGMetresToUnreal / LayerCount);
		Speeds.Add(LayerSpeeds[Index]);
	}
	bBuilt = true;
}

void ADTGWeatherFx::BuildLayer(int32 Index, double Scale, const FLinearColor& Colour)
{
	if (!StreakLayers.IsValidIndex(Index) || !StreakLayers[Index])
	{
		return;
	}
	// Seeded per layer so the same layer is the same every run, which keeps a screenshot reproducible.
	FRandomStream Stream(7717 + Index * 31);
	const double WidthUu = CellWidthM * DTGMetresToUnreal;
	const double HeightUu = CellHeightM * DTGMetresToUnreal;
	const double LengthUu = (0.9 + 0.5 * Scale) * DTGMetresToUnreal * Scale;
	const double ThickUu = 1.1 * DTGMetresToUnreal * 0.012 * Scale;

	FDTGMesh Mesh;
	const int32 Count = FMath::Max(1, FMath::RoundToInt32(static_cast<float>(StreaksPerLayer) * static_cast<float>(Scale)));
	for (int32 Streak = 0; Streak < Count; ++Streak)
	{
		// Uniform through the cell: the wrap below is only invisible because the distribution is.
		const FVector Top(
			Stream.FRandRange(-WidthUu * 0.5, WidthUu * 0.5),
			Stream.FRandRange(-WidthUu * 0.5, WidthUu * 0.5),
			Stream.FRandRange(0.0, HeightUu));
		const double Long = LengthUu * Stream.FRandRange(0.75, 1.3);
		// Every streak is placed twice, a cell apart. That makes the layer exactly periodic over one
		// cell while spanning two, which is what lets it scroll a whole cell and wrap without the view
		// draining out of the top -- a single cell of streaks runs out before it comes back round.
		AddStreak(Mesh, Top, Long, ThickUu, Colour);
		AddStreak(Mesh, Top + FVector(0.0, 0.0, HeightUu), Long, ThickUu, Colour);
	}
	StreakLayers[Index]->CreateMeshSection_LinearColor(0, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors,
		Mesh.Tangents, /*bCreateCollision*/ false);
	if (UMaterialInterface* Material = DTGMeshBuilder::VertexColorMaterial())
	{
		StreakLayers[Index]->SetMaterial(0, Material);
	}
	// A box of rain that is always around the camera has no meaningful bounds to cull against.
	StreakLayers[Index]->SetBoundsScale(4.0f);
}

void ADTGWeatherFx::ReadWeather(float DeltaSeconds)
{
	double Wanted = 0.0;
	double WantedWind = 0.0;
	if (Game && Game->IsContentLoaded())
	{
		switch (Game->Sim().Weather.Current())
		{
		case Weather::Rain:
			Wanted = 0.65;
			WantedWind = 4.0;    // metres per second of crosswind
			break;
		case Weather::Storm:
			Wanted = 1.0;
			WantedWind = 13.0;
			break;
		case Weather::Clear:
		case Weather::PartlyCloudy:
		case Weather::Overcast:
		case Weather::Fog:
		case Weather::Count:
			break;
		}
	}
	// Rain arrives and eases off rather than switching on, the same way the fog does.
	Fall = FMath::FInterpTo(Fall, Wanted, static_cast<double>(DeltaSeconds), 0.5);
	Wind = FMath::FInterpTo(Wind, WantedWind, static_cast<double>(DeltaSeconds), 0.4);
}

void ADTGWeatherFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt || DeltaSeconds <= 0.0f)
	{
		return;
	}
	ReadWeather(DeltaSeconds);

	// Wherever the player is looking from, which is the car's camera when they are driving.
	FVector Eye = GetActorLocation();
	if (const APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0))
	{
		FVector Where;
		FRotator Facing;
		Pc->GetPlayerViewPoint(Where, Facing);
		Eye = Where;
	}
	// Interiors are a pocket half a million units below the world, so a camera down there is indoors
	// and should not have rain falling through the ceiling.
	bIndoors = Eye.Z < ADTGInterior::PocketZ * 0.5;

	if (Fall <= 0.02 || bIndoors)
	{
		for (const TObjectPtr<UProceduralMeshComponent>& Layer : StreakLayers)
		{
			if (Layer && Layer->IsVisible())
			{
				Layer->SetVisibility(false);
			}
		}
		bHasLastEye = false;
		// Still updated while it is dry, so the last of the rain fades out rather than cutting off.
		UpdateSound();
		return;
	}

	// How fast the camera is travelling, measured rather than asked for, so it works on foot, in a car,
	// and in anything added later.
	FVector Travel = FVector::ZeroVector;
	if (bHasLastEye)
	{
		Travel = (Eye - LastEye) / static_cast<double>(DeltaSeconds);
		if (Travel.SizeSquared() > FMath::Square(200.0 * DTGMetresToUnreal))
		{
			Travel = FVector::ZeroVector; // a teleport or a respawn, not a speed
		}
	}
	LastEye = Eye;
	bHasLastEye = true;
	SetActorLocation(Eye);

	// Rain falls through the air; the air moves with the wind; and the player moves through both. What a
	// streak looks like is the sum of the three, which is why driving fast lays the rain over towards you.
	const double FallUu = (7.0 + 9.0 * Fall) * DTGMetresToUnreal;
	const FVector Air(Wind * DTGMetresToUnreal, 0.0, 0.0); // the wind comes off the ocean, out of the east
	const FVector Relative = Air - FVector(Travel.X, Travel.Y, 0.0);
	// Local +Z is the direction a streak points: up, leaned back into the relative wind.
	const FVector Along(-Relative.X, -Relative.Y, FallUu);
	const FVector Up = Along.GetSafeNormal(1.0, FVector::UpVector);
	SetActorRotation(FRotationMatrix::MakeFromZ(Up).Rotator());
	// How fast it travels along that axis, which is more than it falls once the wind or the car is in
	// it. Scrolling at the vertical rate would leave the rain crawling past at seventy miles an hour.
	const double AlongUu = FMath::Max(Along.Size(), FallUu);

	const double HeightUu = CellHeightM * DTGMetresToUnreal;
	// Three tenths of a cell below the eye and seven tenths above it, at every point in the wrap.
	const double Base = -0.3 * HeightUu;
	// A storm falls faster as well as harder.
	for (int32 Index = 0; Index < StreakLayers.Num(); ++Index)
	{
		if (!StreakLayers[Index] || !Scroll.IsValidIndex(Index) || !Speeds.IsValidIndex(Index))
		{
			continue;
		}
		Scroll[Index] += AlongUu * Speeds[Index] * static_cast<double>(DeltaSeconds);
		// Wrapping a layer that is periodic over one cell is invisible, which is the whole trick.
		while (Scroll[Index] >= HeightUu)
		{
			Scroll[Index] -= HeightUu;
		}
		StreakLayers[Index]->SetRelativeLocation(FVector(0.0, 0.0, Base - Scroll[Index]));
		// Thinner rain means fewer layers rather than thinner streaks: the far one drops out first.
		const bool bLayerOn = Fall > 0.25 * static_cast<double>(Index);
		if (StreakLayers[Index]->IsVisible() != bLayerOn)
		{
			StreakLayers[Index]->SetVisibility(bLayerOn);
		}
	}

	UpdateSound();
}

void ADTGWeatherFx::UpdateSound()
{
	if (!Sound)
	{
		return;
	}
	// Inside a car you hear the drumming on the roof rather than the hiss of the air; inside a building
	// you hear it through a wall, which is more of the same.
	// The player keeps possessing the character while driving -- the car is what the camera looks
	// through, not what the controller owns -- so being in a car is a question for the character.
	double Shelter = 0.0;
	if (const ADTGCharacter* Player = Cast<ADTGCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Shelter = Player->DrivingNow() != nullptr ? 0.85 : 0.0;
	}
	Sound->SetWeather(bIndoors ? Fall * 0.45 : Fall, bIndoors ? 1.0 : Shelter);

	// One flash, one clap. The sky decides when lightning strikes; how far off it was decides how long
	// the sound takes to arrive, and the synth holds it back by exactly that.
	if (Sky)
	{
		const double Now = Sky->FlashStrength();
		if (Now > LastFlash + 0.2)
		{
			// A brighter flash is a nearer strike, which is the only cue the player has to go on.
			const double DistanceKm = FMath::Lerp(9.0, 0.4, FMath::Clamp((Now - 0.5) / 0.5, 0.0, 1.0));
			Sound->Thunder(DistanceKm, 0.65 + 0.35 * Now);
		}
		LastFlash = Now;
	}
}
