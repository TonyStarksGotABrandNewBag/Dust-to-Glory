#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"

/** TActorIterator<T>(World): walks every actor of a type. Empty here, but the loop must type-check. */
template <typename T>
class TActorIterator
{
public:
	explicit TActorIterator(UWorld* World) { (void)World; }
	explicit operator bool() const { return false; }
	TActorIterator& operator++() { return *this; }
	T* operator*() const { return nullptr; }
	T* operator->() const { return nullptr; }
};
