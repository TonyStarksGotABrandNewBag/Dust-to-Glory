#include "Game/DTGEngineAudio.h"

#include "DustToGlory.h"

using namespace dtg::sim;

UDTGEngineAudio::UDTGEngineAudio()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
	bAlwaysPlay = true;
	NumChannels = 1; // one engine, positioned in the world by the attenuation settings
}

void UDTGEngineAudio::Configure(const VehicleSetup& InSetup)
{
	// Rebuilding the synth reallocates its firing schedule and filters, so it happens on the render thread:
	// the component may already be playing.
	SynthCommand([this, InSetup]()
	{
		Setup = InSetup;
		Engine.Configure(Setup.Sound, static_cast<double>(Rate), Setup.Engine.RedlineRpm);
		Road.Configure(static_cast<double>(Rate));
		Live = EngineSoundInput();
		Live.Rpm = Setup.Engine.IdleRpm;
		bConfigured = true;
	});
}

void UDTGEngineAudio::SetState(const EngineSoundInput& Input, double MaxSlip, double SurfaceGrip)
{
	SynthCommand([this, Input, MaxSlip, SurfaceGrip]()
	{
		Live = Input;
		LiveSlip = MaxSlip;
		LiveGrip = SurfaceGrip;
	});
}

bool UDTGEngineAudio::Init(int32& SampleRate)
{
	Rate = SampleRate;
	NumChannels = 1;
	Engine.Configure(Setup.Sound, static_cast<double>(Rate), Setup.Engine.RedlineRpm);
	Road.Configure(static_cast<double>(Rate));
	// Sized here so no block ever allocates while the mixer is pulling samples.
	RoadBuffer.SetNumUninitialized(8192);
	return true;
}

int32 UDTGEngineAudio::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	if (!bConfigured)
	{
		FMemory::Memzero(OutAudio, sizeof(float) * NumSamples);
		return NumSamples;
	}
	// The synths are real-time safe (no allocation), so both render straight into the mixer's buffer.
	Engine.Render(OutAudio, NumSamples, Live);
	if (RoadBuffer.Num() < NumSamples)
	{
		// An unusually large block: skip the road layer rather than allocate on this thread.
		return NumSamples;
	}
	Road.Render(RoadBuffer.GetData(), NumSamples, Live.SpeedMs, LiveSlip, LiveGrip);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		OutAudio[Index] = FMath::Clamp(OutAudio[Index] * EngineLevel + RoadBuffer[Index] * RoadLevel, -1.0f, 1.0f);
	}
	return NumSamples;
}
