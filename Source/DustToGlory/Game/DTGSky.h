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
class USkyAtmosphereComponent;
class USkyLightComponent;

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

	/** How far you can see, in metres, which weather decides. */
	double VisibilityM() const { return Visibility; }

private:
	void ApplyWeather(dtg::sim::Weather Value, float DeltaSeconds);

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY()
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY()
	TObjectPtr<USkyLightComponent> Ambient;

	UPROPERTY()
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	double Fraction = 0.5;
	double SunPitch = 60.0;
	double Visibility = 4000.0;
	float Brightness = 6.0f;
	float FogDensity = 0.02f;
};
