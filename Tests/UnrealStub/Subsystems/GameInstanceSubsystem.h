#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

class FSubsystemCollectionBase
{
};

class USubsystem : public UObject
{
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) { (void)Collection; }
	virtual void Deinitialize() {}
};

class UGameInstanceSubsystem : public USubsystem
{
public:
	UGameInstance* GetGameInstance() const { return nullptr; }
	UWorld* GetWorld() const;
};
