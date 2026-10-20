// The sky: time of day and weather, taken from the simulation.
//
// The game clock already runs and the weather already changes on its own Markov chain, so the sky has
// nothing to decide -- it reads the hour and the weather and sets the sun, the fog and the light to match.
// That means dusk on the Grand Strand happens because the clock says so, and a storm darkens the road the
// same moment the simulation starts counting it against the tires.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sim/Clock.h"

#include "DTGSky.generated.h"

class UDTGGameSubsystem;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialParameterCollection;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UMaterialInstanceDynamic;

UCLASS()
class DUSTTOGLORY_API ADTGSky : public AActor
{
	GENERATED_BODY()

public:
	ADTGSky();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0 at midnight, 0.5 at noon. */
	double TimeOfDay() const { return Fraction; }
	/** True while it is dark enough for headlights. */
	bool IsNight() const { return SunPitch < 2.0; }

	/** How dark it is, 0 by day to 1 in the small hours; what the street lamps and the windows follow. */
	double Night() const { return 1.0 - Daylight; }

	/** How far you can see, in metres, which weather decides. */
	double VisibilityM() const { return Visibility; }

	/** How bright the current lightning flash is, 0 when there isn't one. */
	double FlashStrength() const { return Flash; }

private:
	void ApplyWeather(dtg::sim::Weather Value, float DeltaSeconds);
	/** Lightning: strikes on their own schedule during a storm and decay over a few frames. */
	void Strike(bool bStormy, float DeltaSeconds);

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY()
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY()
	TObjectPtr<USkyLightComponent> Ambient;

	UPROPERTY()
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	/** Real clouds with depth that the sun lights through, rather than an empty blue dome. */
	UPROPERTY()
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	/** The cloud material, instanced so the weather can close the sky over: coverage and storm. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CloudLook;

	/** The look: colour grade, lens, motion blur and Lumen quality, applied everywhere. */
	UPROPERTY()
	TObjectPtr<UPostProcessComponent> Grade;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	/** What every material reads from the sky: Night, for the windows that light up, and Wet, for the roads. */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> Parameters;

	float Wetness = 0.0f;

	double Fraction = 0.5;
	double SunPitch = 60.0;
	double Daylight = 1.0;         // 0 at night to 1 by day, from the sun's height
	double Visibility = 4000.0;
	double Flash = 0.0;            // 0..1, the current flash, falling away
	double NextStrike = 8.0;       // seconds until the next one
	float SunBase = 100000.0f;     // eased strength of the sun, lux
	bool bSunSettled = false;      // false until the first frame has set it from the clock
	float Brightness = 1.0f;       // the weather's fraction of a clear sky's sun
	float FogDensity = 0.004f;
	float CloudCover = -0.2f;      // the cloud material's global coverage: -0.2 is a fair-weather sky, 0.6 shut
	float StormCloud = 0.0f;       // its storm look, 0 to 1
	float FogLuminance = 3000.3f;  // what the fog scatters: the sky's brightness, nits
	FRotator SunStep = FRotator(9999.0, 0.0, 0.0);   // where the sun was last turned to; pitch 9999 = never
	FRotator MoonStep = FRotator(9999.0, 0.0, 0.0);
};
