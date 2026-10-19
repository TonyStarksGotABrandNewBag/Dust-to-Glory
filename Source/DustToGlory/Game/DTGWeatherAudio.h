// The weather you can hear.
//
// Rain and thunder are synthesised sample by sample from the simulation's WeatherSynth, the same way the
// engine note is, rather than played from a loop. That means the rain gets louder as it gets heavier
// without a crossfade, and that a thunderclap arrives when its sound would have arrived -- seconds after
// the flash, if the strike was far enough away.
#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"

#include "Sim/EngineAudio.h"

#include "DTGWeatherAudio.generated.h"

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class DUSTTOGLORY_API UDTGWeatherAudio : public USynthComponent
{
	GENERATED_BODY()

public:
	UDTGWeatherAudio(const FObjectInitializer& ObjectInitializer);

	/** How hard it is raining (0..1) and how much is between the player and it (1 = inside a car). */
	void SetWeather(double Intensity, double Shelter);
	/** A strike, at a distance in kilometres. The delay before it is heard is the synth's business. */
	void Thunder(double DistanceKm, double Loudness);

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Audio")
	float Level = 0.55f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	dtg::sim::WeatherSynth Synth;

	/** Only touched on the audio render thread (see SynthCommand in the setters). */
	double LiveIntensity = 0.0;
	double LiveShelter = 0.0;
	int32 Rate = 48000;
};
