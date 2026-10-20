#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
class UEngine : public UObject
{
public:
	void AddOnScreenDebugMessage(uint64 Key, float Time, FColor Colour, const FString& Message)
	{
		(void)Key; (void)Time; (void)Colour; (void)Message;
	}
	class UFont* GetMediumFont() const { return nullptr; }
	class UFont* GetLargeFont() const { return nullptr; }
	class UFont* GetSmallFont() const { return nullptr; }
	class UFont* GetTinyFont() const { return nullptr; }
};
extern UEngine* GEngine;

/** The engine's running average frame rate (EngineGlobals.h). */
#ifndef ENGINE_API
#define ENGINE_API
#endif
extern float GAverageFPS;
