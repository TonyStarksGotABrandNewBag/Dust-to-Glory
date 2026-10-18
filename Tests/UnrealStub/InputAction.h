#pragma once
#include "CoreMinimal.h"
#include "InputActionValue.h"

enum class ETriggerEvent : uint8
{
	None,
	Started,
	Ongoing,
	Canceled,
	Triggered,
	Completed,
};

class UInputAction : public UObject
{
public:
	EInputActionValueType ValueType = EInputActionValueType::Boolean;
	bool bConsumeInput = true;
};
