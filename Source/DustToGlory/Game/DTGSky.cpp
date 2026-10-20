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
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

#include "Game/DTGGameSubsystem.h"

using namespace dtg::sim;

namespace detail_sky
{
	/** Turns a light to Wanted only once it is a third of a degree off where it was last turned to. */
	void Step(UDirectionalLightComponent* Light, const FRotator& Wanted, FRotator& Current)
	{
		constexpr double StepDegrees = 1.0 / 3.0;
		if (FMath::Abs(FRotator::NormalizeAxis(Wanted.Pitch - Current.Pitch)) >= StepDegrees
			|| FMath::Abs(FRotator::NormalizeAxis(Wanted.Yaw - Current.Yaw)) >= StepDegrees || Current.Pitch > 1000.0)
		{
			Current = Wanted;
			Light->SetWorldRotation(Wanted);
		}
	}
}

ADTGSky::ADTGSky()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	// Physical units: the sun is a hundred thousand lux at full strength, the lamps thousands of lumens,
	// and the camera below meters the scene the way a real one would. The atmosphere dims and reddens the
	// sun per pixel as it drops, so its colour is never faked.
	Sun->SetIntensity(100000.0f);
	// Lumen wants a sun that tells it where the light is coming from, and shadows that follow it.
	Sun->SetDynamicShadowDistanceMovableLight(30000.0f);
	Sun->bAtmosphereSunLight = true;
	Sun->bPerPixelAtmosphereTransmittance = true;
	Sun->SetCastShadows(true);

	// The moon: a faint, cool second sun that gives the night a direction, shadows and a sky that is not
	// black. The atmosphere scatters it as the second atmosphere light, so a night sky still has a glow.
	// Far brighter than a real moon against this sun, because a night you cannot see is not playable.
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetIntensity(0.0f);
	Moon->SetLightColor(FLinearColor(0.62f, 0.72f, 1.0f));
	Moon->SetDynamicShadowDistanceMovableLight(20000.0f);
	Moon->bAtmosphereSunLight = true;
	Moon->AtmosphereSunLightIndex = 1;
	Moon->bPerPixelAtmosphereTransmittance = true;
	Moon->SetCastShadows(true);

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
	// The exposure meters the physical scene between EV 0 and EV 16: a moonlit street and noon. Left to
	// itself the camera opened up until a moonlit street looked like an overcast afternoon; the floor is
	// what keeps the night dark, instead of the bias that used to. By day it is held half a stop
	// under what the meter wants (set each frame), which is where a sunny street stops looking bleached.
	Look.bOverride_AutoExposureBias = true;
	Look.AutoExposureBias = 0.0f;
	Look.bOverride_AutoExposureMinBrightness = true;
	Look.AutoExposureMinBrightness = 0.0f;
	Look.bOverride_AutoExposureMaxBrightness = true;
	Look.AutoExposureMaxBrightness = 16.0f;
	Look.bOverride_HistogramLogMin = true;
	Look.HistogramLogMin = -2.0f;
	Look.bOverride_HistogramLogMax = true;
	Look.HistogramLogMax = 18.0f;
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
		// Instanced, so the weather can shut the sky: the engine's cloud material has a global coverage
		// (-0.2 is scattered fair-weather cloud, 0.6 an overcast) and a storm look.
		CloudLook = UMaterialInstanceDynamic::Create(CloudMaterial, this);
		Clouds->SetMaterial(CloudLook ? static_cast<UMaterialInterface*>(CloudLook) : CloudMaterial);
	}
	else
	{
		UE_LOG(LogDTG, Warning, TEXT("No volumetric cloud material; the sky will be clear."));
	}
	Parameters = Cast<UMaterialParameterCollection>(StaticLoadObject(UMaterialParameterCollection::StaticClass(), nullptr,
		TEXT("/Game/Art/Materials/MPC_DTG.MPC_DTG")));
	if (!Parameters)
	{
		UE_LOG(LogDTG, Warning, TEXT("No material parameter collection; windows will not light up and rain will not wet the roads."));
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
	// South Carolina and Florida both face east, so the sun comes up over the Atlantic. The sun turns in
	// steps of a third of a degree rather than every frame: a directional light that has moved at all
	// throws away every cached page of its virtual shadow map, and with the whole world redrawn into it
	// each frame that was 9 ms of a 25 ms street frame. A step moves a 20 m shadow about 12 cm.
	detail_sky::Step(Sun, FRotator(-SunPitch, 100.0 + Fraction * 200.0, 0.0), SunStep);

	const Weather Now = Game->Sim().Weather.Current();
	ApplyWeather(Now, DeltaSeconds);
	Strike(Now == Weather::Storm, DeltaSeconds);

	// Daylight, 0 at night to 1 by day, is what the lamps, the lit windows and the fog follow.
	Daylight = FMath::Clamp((SunPitch + 6.0) / 18.0, 0.0, 1.0);
	// The sun in lux: a hundred thousand in clear air, the weather's fraction of that under cloud, and
	// the atmosphere's per-pixel transmittance takes it down to orange at the horizon. It goes out over
	// the few degrees around the horizon: pitched under it, a sun of this strength lights roofs and tree
	// crowns from underneath, so the sky light carries the twilight instead.
	const double Up = FMath::Clamp((SunPitch + 1.0) / 4.0, 0.0, 1.0);
	const float TargetIntensity = static_cast<float>(100000.0 * Up * Brightness);
	// Eased, and started where the first frame wants it, so a game that begins at night does not open
	// with a second of daylight easing away.
	SunBase = bSunSettled ? FMath::FInterpTo(SunBase, TargetIntensity, DeltaSeconds, 2.0f) : TargetIntensity;
	bSunSettled = true;
	Sun->SetIntensity(SunBase);
	Sun->SetVisibility(SunBase > 1.0f);
	// The sky light is the captured sky at its real strength. A flash is sky light rather than sun light
	// (the sun is under the horizon at night, when it shows most): a sky thirty times brighter for a
	// moment, decaying, which is what you actually see of lightning.
	Ambient->SetIntensity(static_cast<float>(1.0 + Flash * 30.0));
	// The moon stands opposite the sun, highest when the sun is deepest, and comes up as the sun goes
	// under. Three lux, a dozen times the real thing, because a night you cannot see is not playable. Hidden
	// while it is off: a light of any intensity is a second set of shadow maps.
	const double Moonlight = FMath::Clamp(-SunPitch / 6.0, 0.0, 1.0);
	const double MoonPitch = FMath::Clamp(-SunPitch * 1.5, 0.0, 60.0);
	detail_sky::Step(Moon, FRotator(-MoonPitch, 280.0 + Fraction * 200.0, 0.0), MoonStep);
	Moon->SetIntensity(static_cast<float>(3.0 * Moonlight));
	Moon->SetVisibility(Moonlight > 0.0);
	Grade->Settings.AutoExposureBias = static_cast<float>(-0.5 * Daylight);
	// What the fog scatters is the sky's own brightness: thousands of nits by day, under one by moonlight.
	FogLuminance = static_cast<float>(3000.0 * Daylight + 0.3);
	// What the materials read: windows light up as the day goes, and the roads darken and shine in the rain,
	// which takes a while to wet them and a while to dry.
	const double TargetWet = Now == Weather::Rain ? 0.85 : (Now == Weather::Storm ? 1.0 : (Now == Weather::Fog ? 0.3 : 0.0));
	Wetness = FMath::FInterpTo(Wetness, static_cast<float>(TargetWet), DeltaSeconds, 0.15f);
	if (Parameters)
	{
		// Night drives the windows' glow, which is in nits now that the exposure is physical: a lit room
		// shows a few of them through the glass, bright against a street at EV 1 without blowing out.
		UKismetMaterialLibrary::SetScalarParameterValue(this, Parameters, TEXT("Night"), static_cast<float>(4.0 * (1.0 - Daylight)));
		UKismetMaterialLibrary::SetScalarParameterValue(this, Parameters, TEXT("Wet"), Wetness);
	}
	if (Flash > 0.0)
	{
		// The fog lights up with it, which is what makes a flash read across a whole wet street.
		Fog->SetFogInscatteringColor(FLinearColor(
			FMath::Min(1.0f, 0.45f + static_cast<float>(Flash) * 0.6f),
			FMath::Min(1.0f, 0.5f + static_cast<float>(Flash) * 0.6f),
			FMath::Min(1.0f, 0.6f + static_cast<float>(Flash) * 0.5f)) * FogLuminance * (1.0f + static_cast<float>(Flash) * 30.0f));
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
	// Each weather is a visibility, a thickness of air and the fraction of the sun that gets through;
	// everything else follows from those. Clear air is thin: the sky atmosphere already gives distance
	// its aerial perspective, and the 0.015 this used to be washed every long view out to grey.
	float TargetDensity = 0.004f;
	float TargetBrightness = 1.0f;
	float TargetCover = -0.2f;
	float TargetStorm = 0.0f;
	double TargetVisibility = 6000.0;
	FLinearColor FogColour(0.45f, 0.52f, 0.6f);
	switch (Value)
	{
	case Weather::Clear:
		break;
	case Weather::PartlyCloudy:
		TargetBrightness = 0.85f;
		TargetCover = 0.05f;
		TargetVisibility = 5000.0;
		break;
	case Weather::Overcast:
		TargetDensity = 0.012f;
		TargetBrightness = 0.2f;
		TargetCover = 0.5f;
		TargetVisibility = 3000.0;
		FogColour = FLinearColor(0.42f, 0.44f, 0.47f);
		break;
	case Weather::Rain:
		TargetDensity = 0.025f;
		TargetBrightness = 0.12f;
		TargetCover = 0.55f;
		TargetStorm = 0.4f;
		TargetVisibility = 1400.0;
		FogColour = FLinearColor(0.36f, 0.39f, 0.43f);
		break;
	case Weather::Storm:
		TargetDensity = 0.045f;
		TargetBrightness = 0.06f;
		TargetCover = 0.65f;
		TargetStorm = 1.0f;
		TargetVisibility = 700.0;
		FogColour = FLinearColor(0.25f, 0.27f, 0.31f);
		break;
	case Weather::Fog:
		TargetDensity = 0.16f;
		TargetBrightness = 0.35f;
		TargetCover = 0.35f;
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
	Fog->SetFogInscatteringColor(FogColour * FogLuminance);
	// The sky shuts over as slowly as the fog arrives, and the rain falls out of cloud, not out of blue.
	CloudCover = FMath::FInterpTo(CloudCover, TargetCover, DeltaSeconds, 0.35f);
	StormCloud = FMath::FInterpTo(StormCloud, TargetStorm, DeltaSeconds, 0.35f);
	if (CloudLook)
	{
		CloudLook->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), CloudCover);
		CloudLook->SetScalarParameterValue(TEXT("StormClouds"), StormCloud);
	}
}
