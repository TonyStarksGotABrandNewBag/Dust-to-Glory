#pragma once
#include "CoreMinimal.h"

class UInputModifier : public UObject
{
};

enum class EInputAxisSwizzle : uint8
{
	YXZ,
	ZYX,
	XZY,
	YZX,
	ZXY,
};

class UInputModifierSwizzleAxis : public UInputModifier
{
public:
	EInputAxisSwizzle Order = EInputAxisSwizzle::YXZ;
};

class UInputModifierNegate : public UInputModifier
{
public:
	bool bX = true;
	bool bY = true;
	bool bZ = true;
};

class UInputModifierDeadZone : public UInputModifier
{
public:
	float LowerThreshold = 0.2f;
	float UpperThreshold = 1.0f;
};

class UInputModifierScalar : public UInputModifier
{
public:
	FVector Scalar{1.0, 1.0, 1.0};
};
