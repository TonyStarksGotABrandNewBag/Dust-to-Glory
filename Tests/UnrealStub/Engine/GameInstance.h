#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
class UGameInstance : public UObject
{
public:
	UWorld* GetWorld() const;
	template <typename T>
	T* GetSubsystem() const { return nullptr; }
};
class ULocalPlayer : public UObject
{
public:
	template <typename T>
	T* GetSubsystem() const { return nullptr; }
	/** The static form, which is how the enhanced input subsystem is normally reached. */
	template <typename T>
	static T* GetSubsystem(const ULocalPlayer* Player) { (void)Player; return nullptr; }
};
