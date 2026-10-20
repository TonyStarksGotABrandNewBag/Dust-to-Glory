#include "Game/DTGSky.h"

#include "DustToGlory.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

#include "Game/DTGGameSubsystem.h"

using namespace dtg::sim;

ADTGSky::ADTGSky()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetIntensity(6.0f);
	// Lumen wants a sun that tells it where the light is coming from, and shadows that follow it.
	Sun->SetDynamicShadowDistanceMovableLight(30000.0f);
	Sun->bAtmosphereSunLight = true;
	Sun->SetCastShadows(true);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	Ambient = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	Ambient->SetupAttachment(Root);
	Ambient->SetMobility(EComponentMobility::Movable);
	Ambient->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	Ambient->bRealTimeCapture = true;
	Ambient->SetIntensity(1.0f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(FogDensity);
	Fog->SetFogHeightFalloff(0.2f);
	// Fog that takes its colour from the sky is what makes a coastal morning read as one.
	Fog->SetVolumetricFog(true);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);

	// The grade. Open-world games get most of their "photographic" look here rather than from geometry:
	// warm highlights against slightly cool shadows, a touch more contrast and saturation than the raw
	// render, a soft vignette and fine grain to take the CG edge off, and motion blur kept light so a
	// fast drive still reads. Lumen is asked for more than its defaults, which an RTX card can afford.
	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(Root);
	Grade->bUnbound = true;
	FPostProcessSettings& Look = Grade->Settings;
	Look.bOverride_ColorSaturation = true;
	Look.ColorSaturation = FVector4(1.06, 1.06, 1.06, 1.0);
	Look.bOverride_ColorContrast = true;
	Look.ColorContrast = FVector4(1.06, 1.06, 1.06, 1.0);
	Look.bOverride_ColorGainHighlights = true;
	Look.ColorGainHighlights = FVector4(1.04, 1.01, 0.95, 1.0);
	Look.bOverride_ColorOffsetShadows = true;
	Look.ColorOffsetShadows = FVector4(-0.002, 0.0, 0.004, 0.0);
	Look.bOverride_VignetteIntensity = true;
	Look.VignetteIntensity = 0.28f;
	Look.bOverride_FilmGrainIntensity = true;
	Look.FilmGrainIntensity = 0.04f;
	Look.bOverride_SceneFringeIntensity = true;
	Look.SceneFringeIntensity = 0.0f;
	Look.bOverride_BloomIntensity = true;
	Look.BloomIntensity = 0.45f;
	Look.bOverride_MotionBlurAmount = true;
	Look.MotionBlurAmount = 0.3f;
	Look.bOverride_Sharpen = true;
	Look.Sharpen = 0.4f;
	Look.bOverride_LumenFinalGatherQuality = true;
	Look.LumenFinalGatherQuality = 2.0f;
	Look.bOverride_LumenSceneDetail = true;
	Look.LumenSceneDetail = 1.5f;
	Look.bOverride_LumenReflectionQuality = true;
	Look.LumenReflectionQuality = 2.0f;
}

void ADTGSky::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	// The engine's own volumetric cloud material. Loaded by path, so it is in the cook list in DefaultGame.ini.
	if (UMaterialInterface* CloudMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
			TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"))))
	{
		Clouds->SetMaterial(CloudMaterial);
	}
	else
	{
		UE_LOG(LogDTG, Warning, TEXT("No volumetric cloud material; the sky will be clear."));
	}
}

void ADTGSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	Fraction = Game->Sim().Clock.DayFraction();

	// The sun climbs from sunrise to noon and back down: a half-sine through the daylight hours, which is
	// close enough to the real thing at this latitude and keeps dawn and dusk long enough to be worth seeing.
	constexpr double Sunrise = 6.3 / 24.0;
	constexpr double Sunset = 19.7 / 24.0;
	if (Fraction > Sunrise && Fraction < Sunset)
	{
		const double Through = (Fraction - Sunrise) / (Sunset - Sunrise);
		SunPitch = 72.0 * FMath::Sin(Through * UE_PI);
	}
	else
	{
		// Below the horizon; the night side carries on so the moonless small hours are the darkest.
		const double Through = Fraction >= Sunset ? (Fraction - Sunset) / (1.0 - Sunset + Sunrise)
												  : (Fraction + 1.0 - Sunset) / (1.0 - Sunset + Sunrise);
		SunPitch = -40.0 * FMath::Sin(Through * UE_PI);
	}
	// South Carolina and Florida both face east, so the sun comes up over the Atlantic.
	Sun->SetWorldRotation(FRotator(-SunPitch, 100.0 + Fraction * 200.0, 0.0));

	const Weather Now = Game->Sim().Weather.Current();
	ApplyWeather(Now, DeltaSeconds);
	Strike(Now == Weather::Storm, DeltaSeconds);

	// Night is lit by the sky rather than the sun, and never goes fully black. A flash is added on top
	// of all of it, so lightning lights a midnight road as well as a grey afternoon.
	const double Daylight = FMath::Clamp((SunPitch + 6.0) / 18.0, 0.0, 1.0);
	const float TargetIntensity = static_cast<float>(0.15 + Daylight * Brightness);
	// The daylight level is kept separately from what the light is actually set to, in case anything
	// else is ever added on top of it. It starts where the first frame wants it, so a game that begins
	// at night does not open with a second of daylight easing away.
	SunBase = bSunSettled ? FMath::FInterpTo(SunBase, TargetIntensity, DeltaSeconds, 2.0f) : TargetIntensity;
	bSunSettled = true;
	Sun->SetIntensity(SunBase);
	// Warm at the horizon, white overhead: the cheapest trick there is for making dusk look like dusk.
	const double Warmth = 1.0 - FMath::Clamp(SunPitch / 25.0, 0.0, 1.0);
	Sun->SetLightColor(FLinearColor(1.0f, static_cast<float>(1.0 - 0.35 * Warmth), static_cast<float>(1.0 - 0.6 * Warmth)));
	// A flash is sky light rather than sun light. The sun is pitched below the horizon at night, so
	// adding it there would light the world from underneath at exactly the moment it shows most.
	// Lightning is instantaneous; the decay is what you actually see.
	Ambient->SetIntensity(static_cast<float>(0.25 + Daylight * 0.9 + Flash * 11.0));
	if (Flash > 0.0)
	{
		// The fog lights up with it, which is what makes a flash read across a whole wet street.
		Fog->SetFogInscatteringColor(FLinearColor(
			FMath::Min(1.0f, 0.45f + static_cast<float>(Flash) * 0.6f),
			FMath::Min(1.0f, 0.5f + static_cast<float>(Flash) * 0.6f),
			FMath::Min(1.0f, 0.6f + static_cast<float>(Flash) * 0.5f)));
	}
}

void ADTGSky::Strike(bool bStormy, float DeltaSeconds)
{
	// A flash is bright for a frame or two and gone, sometimes with a second one on its heels, which is
	// what makes lightning read as lightning rather than as the lights flickering.
	Flash = FMath::Max(0.0, Flash - static_cast<double>(DeltaSeconds) * 7.0);
	if (!bStormy)
	{
		NextStrike = FMath::Min(NextStrike, 4.0);
		return;
	}
	NextStrike -= static_cast<double>(DeltaSeconds);
	if (NextStrike > 0.0)
	{
		return;
	}
	Flash = FMath::FRandRange(0.5, 1.0);
	// Clustered rather than metronomic: a storm gives you two close together and then nothing for a while.
	NextStrike = FMath::FRand() < 0.35 ? FMath::FRandRange(0.12, 0.4) : FMath::FRandRange(5.0, 22.0);
}

void ADTGSky::ApplyWeather(Weather Value, float DeltaSeconds)
{
	// Each weather is a visibility and a thickness of air; everything else follows from those. Clear air
	// is thin: the sky atmosphere already gives distance its aerial perspective, and the 0.015 this used
	// to be washed every long view out to grey.
	float TargetDensity = 0.004f;
	float TargetBrightness = 6.0f;
	double TargetVisibility = 6000.0;
	FLinearColor FogColour(0.45f, 0.52f, 0.6f);
	switch (Value)
	{
	case Weather::Clear:
		break;
	case Weather::PartlyCloudy:
		TargetBrightness = 5.2f;
		TargetVisibility = 5000.0;
		break;
	case Weather::Overcast:
		TargetDensity = 0.012f;
		TargetBrightness = 3.0f;
		TargetVisibility = 3000.0;
		FogColour = FLinearColor(0.42f, 0.44f, 0.47f);
		break;
	case Weather::Rain:
		TargetDensity = 0.025f;
		TargetBrightness = 2.0f;
		TargetVisibility = 1400.0;
		FogColour = FLinearColor(0.36f, 0.39f, 0.43f);
		break;
	case Weather::Storm:
		TargetDensity = 0.045f;
		TargetBrightness = 1.1f;
		TargetVisibility = 700.0;
		FogColour = FLinearColor(0.25f, 0.27f, 0.31f);
		break;
	case Weather::Fog:
		TargetDensity = 0.16f;
		TargetBrightness = 2.4f;
		TargetVisibility = 220.0;
		FogColour = FLinearColor(0.62f, 0.64f, 0.66f);
		break;
	case Weather::Count:
		break;
	}
	// Weather arrives rather than snapping on.
	FogDensity = FMath::FInterpTo(FogDensity, TargetDensity, DeltaSeconds, 0.35f);
	Brightness = FMath::FInterpTo(Brightness, TargetBrightness, DeltaSeconds, 0.35f);
	Visibility = FMath::FInterpTo(Visibility, TargetVisibility, static_cast<double>(DeltaSeconds), 0.35);
	Fog->SetFogDensity(FogDensity);
	Fog->SetFogInscatteringColor(FogColour);
}
