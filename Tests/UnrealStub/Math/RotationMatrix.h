#pragma once
#include "CoreMinimal.h"
namespace EAxis
{
	enum Type
	{
		None,
		X,
		Y,
		Z,
	};
}

struct FMatrix
{
	FRotator Rotator() const { return FRotator(); }
	FVector GetScaledAxis(EAxis::Type Axis) const { (void)Axis; return FVector(); }
	FVector GetUnitAxis(EAxis::Type Axis) const { (void)Axis; return FVector(); }
	FVector TransformPosition(const FVector& In) const { return In; }
};
struct FRotationMatrix : public FMatrix
{
	explicit FRotationMatrix(const FRotator& Rotation) { (void)Rotation; }
	static FRotationMatrix MakeFromZ(const FVector& Z);
	static FRotationMatrix MakeFromX(const FVector& X);
	static FRotationMatrix MakeFromZX(const FVector& Z, const FVector& X);
};
