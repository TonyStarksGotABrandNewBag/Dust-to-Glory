#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

class UInputComponent;
class ULocalPlayer;

struct FInputModeDataBase
{
};
struct FInputModeGameOnly : public FInputModeDataBase
{
};
struct FInputModeGameAndUI : public FInputModeDataBase
{
	FInputModeGameAndUI& SetHideCursorDuringCapture(bool bHide) { (void)bHide; return *this; }
	FInputModeGameAndUI& SetLockMouseToViewportBehavior(EMouseLockMode Behaviour)
	{
		(void)Behaviour;
		return *this;
	}
};
struct FInputModeUIOnly : public FInputModeDataBase
{
};

class APlayerController : public AController
{
public:
	virtual void SetupInputComponent() {}
	virtual void PlayerTick(float DeltaTime) { (void)DeltaTime; }
	void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const
	{
		OutLocation = FVector();
		OutRotation = FRotator();
	}
	void SetViewTarget(AActor* Target) { (void)Target; }
	void SetViewTargetWithBlend(AActor* Target, float BlendTime = 0.0f)
	{
		(void)Target;
		(void)BlendTime;
	}
	ULocalPlayer* GetLocalPlayer() const { return nullptr; }
	class AHUD* GetHUD() const { return MyHUD; }
	class AHUD* MyHUD = nullptr;
	void SetShowMouseCursor(bool bShow) { bShowMouseCursor = bShow; }
	void SetInputMode(const FInputModeDataBase& Mode) { (void)Mode; }
	void AddYawInput(float Value) { (void)Value; }
	void AddPitchInput(float Value) { (void)Value; }

	UInputComponent* InputComponent = nullptr;
	bool bShowMouseCursor = false;
	bool bEnableClickEvents = false;
};
