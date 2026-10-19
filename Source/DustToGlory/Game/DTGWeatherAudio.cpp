#include "Game/DTGWeatherAudio.h"

#include "DustToGlory.h"

using namespace dtg::sim;

UDTGWeatherAudio::UDTGWeatherAudio(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
	bAlwaysPlay = true;
	// Weather is all around the listener rather than coming from a place, so it is not attenuated.
	bAllowSpatialization = false;
	NumChannels = 1;
}

void UDTGWeatherAudio::SetWeather(double Intensity, double Shelter)
{
	SynthCommand([this, Intensity, Shelter]()
	{
		LiveIntensity = Intensity;
		LiveShelter = Shelter;
	});
}

void UDTGWeatherAudio::Thunder(double DistanceKm, double Loudness)
{
	// Queued onto the render thread: the clap's countdown is state the synth owns.
	SynthCommand([this, DistanceKm, Loudness]()
	{
		Synth.Thunder(DistanceKm, Loudness);
	});
}

bool UDTGWeatherAudio::Init(int32& SampleRate)
{
	Rate = SampleRate;
	NumChannels = 1;
	Synth.Configure(static_cast<double>(Rate));
	return true;
}

int32 UDTGWeatherAudio::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	// Real-time safe: the synth allocates nothing, so it renders straight into the mixer's buffer.
	Synth.Render(OutAudio, NumSamples, LiveIntensity, LiveShelter);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		OutAudio[Index] = FMath::Clamp(OutAudio[Index] * Level, -1.0f, 1.0f);
	}
	return NumSamples;
}
