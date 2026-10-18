#pragma once
#include "CoreMinimal.h"
#include "InputMappingContext.h"

class UEnhancedInputLocalPlayerSubsystem : public UObject
{
public:
	void AddMappingContext(const UInputMappingContext* Context, int32 Priority)
	{
		(void)Context;
		(void)Priority;
	}
	void RemoveMappingContext(const UInputMappingContext* Context) { (void)Context; }
	void ClearAllMappings() {}
};
