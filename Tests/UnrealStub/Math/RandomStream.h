#pragma once
#include "CoreMinimal.h"
struct FRandomStream
{
	FRandomStream() = default;
	explicit FRandomStream(int32 InSeed) : Seed(InSeed) {}
	void Initialize(int32 InSeed) { Seed = InSeed; }
	float FRand() const { return 0.5f; }
	float FRandRange(float Low, float High) const { return (Low + High) * 0.5f; }
	int32 RandRange(int32 Low, int32 High) const { return Low <= High ? Low : High; }
	int32 RandHelper(int32 Range) const { (void)Range; return 0; }
	int32 Seed = 0;
};
