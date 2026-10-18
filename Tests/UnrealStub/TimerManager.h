#pragma once
#include "CoreMinimal.h"

class FTimerDelegate
{
public:
	template <typename UserClass, typename... Args>
	static FTimerDelegate CreateUObject(UserClass* Object, void (UserClass::*Method)(Args...), Args... InArgs)
	{
		(void)Object;
		(void)Method;
		return FTimerDelegate();
	}
	template <typename Fn>
	static FTimerDelegate CreateLambda(Fn InAction)
	{
		(void)InAction;
		return FTimerDelegate();
	}
	/** Binds a lambda whose lifetime is tied to a UObject, which is what timers on actors want. */
	template <typename Fn>
	static FTimerDelegate CreateWeakLambda(UObject* Context, Fn InAction)
	{
		(void)Context;
		(void)InAction;
		return FTimerDelegate();
	}
};

class FTimerManager
{
public:
	void SetTimer(FTimerHandle& Handle, FTimerDelegate Delegate, float Rate, bool bLoop,
		float FirstDelay = -1.0f)
	{
		(void)Handle; (void)Delegate; (void)Rate; (void)bLoop; (void)FirstDelay;
	}
	template <typename UserClass>
	void SetTimer(FTimerHandle& Handle, UserClass* Object, void (UserClass::*Method)(), float Rate,
		bool bLoop, float FirstDelay = -1.0f)
	{
		(void)Handle; (void)Object; (void)Method; (void)Rate; (void)bLoop; (void)FirstDelay;
	}
	void ClearTimer(FTimerHandle& Handle) { Handle.Invalidate(); }
	bool IsTimerActive(const FTimerHandle& Handle) const { return Handle.IsValid(); }
	float GetTimerRemaining(const FTimerHandle& Handle) const { (void)Handle; return 0.0f; }
};
