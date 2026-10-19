// The car you hear is the car you're driving.
//
// EngineSynth builds the exhaust note from the engine's real firing order and the parts fitted, and RoadSynth
// adds tire squeal, rolling roar and wind. This component runs both on the audio render thread through a
// USynthComponent, so the sound tracks rpm, load and slip sample by sample instead of crossfading loops.
#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"

#include "Sim/EngineAudio.h"
#include "Sim/Tuning.h"

#include "DTGEngineAudio.generated.h"

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class DUSTTOGLORY_API UDTGEngineAudio : public USynthComponent
{
	GENERATED_BODY()

public:
	UDTGEngineAudio(const FObjectInitializer& ObjectInitializer);

	/** Rebuilds the synth for a car's engine, exhaust and forced induction. */
	void Configure(const dtg::sim::VehicleSetup& Setup);
	/** Called each frame from the vehicle: what the engine and tires are doing right now. */
	void SetState(const dtg::sim::EngineSoundInput& Input, double MaxSlip, double SurfaceGrip);

	/** Mix of road noise against the engine. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Audio")
	float RoadLevel = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Audio")
	float EngineLevel = 0.9f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	dtg::sim::EngineSynth Engine;
	dtg::sim::RoadSynth Road;
	dtg::sim::VehicleSetup Setup;

	/** Only touched on the audio render thread (see SynthCommand in Configure and SetState). */
	dtg::sim::EngineSoundInput Live;
	double LiveSlip = 0.0;
	double LiveGrip = 1.0;
	bool bConfigured = false;
	int32 Rate = 48000;
	TArray<float> RoadBuffer;
};
