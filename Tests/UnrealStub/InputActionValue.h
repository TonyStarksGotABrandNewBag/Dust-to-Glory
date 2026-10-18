#pragma once
#include "CoreMinimal.h"

enum class EInputActionValueType : uint8
{
	Boolean,
	Axis1D,
	Axis2D,
	Axis3D,
};

/**
 * NOTE: the real Get<T>() is constrained to bool, float, FVector2D and FVector. Left unconstrained
 * here, so a Get<SomethingSilly>() would compile; every other misuse still fails.
 */
struct FInputActionValue
{
	FInputActionValue() = default;
	explicit FInputActionValue(bool bValue) : Axis(bValue ? 1.0 : 0.0) {}
	explicit FInputActionValue(float Value) : Axis(Value) {}
	explicit FInputActionValue(FVector2D Value) : Axis(Value.X), Axis2(Value) {}

	template <typename T>
	T Get() const;

	double Axis = 0.0;
	FVector2D Axis2;
};

template <>
inline bool FInputActionValue::Get<bool>() const { return Axis != 0.0; }
template <>
inline float FInputActionValue::Get<float>() const { return static_cast<float>(Axis); }
template <>
inline FVector2D FInputActionValue::Get<FVector2D>() const { return Axis2; }
template <>
inline FVector FInputActionValue::Get<FVector>() const { return FVector(Axis2.X, Axis2.Y, 0.0); }
