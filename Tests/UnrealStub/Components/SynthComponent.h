#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/**
 * NOTE: USynthComponent's real contract is Init() plus OnGenerateAudio(float*, int32) returning
 * int32, overridden by the subclass. Declared virtual here so an override with the wrong signature
 * is caught rather than silently becoming a new function.
 *
 * Checked against UE 5.8.3: the only constructor takes an FObjectInitializer, so a subclass must
 * declare one and pass it to Super. The default constructor the stub used to allow does not exist.
 */
class FObjectInitializer;

class USynthComponent : public USceneComponent
{
public:
	USynthComponent(const FObjectInitializer& ObjectInitializer) { (void)ObjectInitializer; }

	virtual bool Init(int32& SampleRate) { (void)SampleRate; return true; }
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples)
	{
		(void)OutAudio;
		return NumSamples;
	}
	void Start() {}
	void Stop() {}
	bool IsPlaying() const { return false; }
	void SetVolumeMultiplier(float Value) { (void)Value; }
	/** Queues work onto the audio render thread. Takes a callable; the project passes a lambda. */
	template <typename Fn>
	void SynthCommand(Fn InCommand) { (void)InCommand; }

	int32 NumChannels = 1;
	bool bAllowSpatialization = false;
	bool bAlwaysPlay = true;
	bool bIsUISound = false;
};
