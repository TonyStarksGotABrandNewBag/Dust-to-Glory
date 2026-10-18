#pragma once
#include "CoreMinimal.h"

struct FKey
{
	FKey() = default;
	explicit FKey(const TCHAR* InName) : KeyName(InName) {}
	FName KeyName;
	bool IsValid() const { return true; }
};

/** Only the keys this project binds. A typo in a key name should fail here, so none are invented. */
struct EKeys
{
	static const FKey A;
	static const FKey D;
	static const FKey E;
	static const FKey F;
	static const FKey G;
	static const FKey Q;
	static const FKey S;
	static const FKey W;
	static const FKey One;
	static const FKey Two;
	static const FKey Three;
	static const FKey Up;
	static const FKey Down;
	static const FKey Tab;
	static const FKey Escape;
	static const FKey SpaceBar;
	static const FKey LeftMouseButton;
	static const FKey Mouse2D;
	static const FKey Gamepad_LeftX;
	static const FKey Gamepad_LeftY;
	static const FKey Gamepad_RightX;
	static const FKey Gamepad_RightY;
	static const FKey Gamepad_LeftShoulder;
	static const FKey Gamepad_RightShoulder;
	static const FKey Gamepad_LeftTriggerAxis;
	static const FKey Gamepad_RightTriggerAxis;
	static const FKey Gamepad_DPad_Up;
	static const FKey Gamepad_DPad_Down;
	static const FKey Gamepad_FaceButton_Bottom;
	static const FKey Gamepad_FaceButton_Top;
	static const FKey Gamepad_FaceButton_Left;
	static const FKey Gamepad_FaceButton_Right;
};
