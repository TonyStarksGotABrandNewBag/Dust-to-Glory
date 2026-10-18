#pragma once
#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputModifiers.h"

struct FEnhancedActionKeyMapping
{
	TArray<TObjectPtr<UInputModifier>> Modifiers;
	TArray<TObjectPtr<class UInputTrigger>> Triggers;
};

class UInputTrigger : public UObject
{
};

class UInputMappingContext : public UObject
{
public:
	FEnhancedActionKeyMapping& MapKey(const UInputAction* Action, FKey Key);
};
