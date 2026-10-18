#pragma once
#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputActionValue.h"

class UInputComponent : public UActorComponent
{
};

/**
 * The real BindAction returns a binding handle; the project ignores it. The member-function pointer
 * is deliberately typed, so binding a handler with the wrong signature fails here rather than in
 * the editor.
 */
class UEnhancedInputComponent : public UInputComponent
{
public:
	template <typename UserClass, typename... Args>
	void BindAction(const UInputAction* Action, ETriggerEvent Event, UserClass* Object,
		void (UserClass::*Method)(Args...))
	{
		(void)Action;
		(void)Event;
		(void)Object;
		(void)Method;
	}
};
