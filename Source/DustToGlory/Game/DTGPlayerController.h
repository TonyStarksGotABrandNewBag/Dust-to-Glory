// Input and the player's hands on the world.
//
// Every input action and mapping is created in code, so the project needs no input assets and the controls
// are readable in one place. Two contexts: on foot and driving. Driving carries the clutchless manual the
// player asked for -- in manual the gearbox only changes when the paddle is pulled, in automatic it shifts
// itself, and the choice is the simulation's SetTransmission, not a local flag.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DTGPlayerController.generated.h"

class ADTGCharacter;
class ADTGVehicle;
class UDTGGameSubsystem;
class UInputAction;
class UInputMappingContext;

UCLASS()
class DUSTTOGLORY_API ADTGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADTGPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Switches the active mapping context when the player gets in or out of a car. */
	void SetDrivingContext(bool bDriving);

	/** True while a menu (building services, phone, court) owns the input. */
	bool IsMenuOpen() const { return bMenuOpen; }
	void SetMenuOpen(bool bOpen);

private:
	void BuildInput();

	// On foot
	void OnMove(const struct FInputActionValue& Value);
	void OnMoveReleased();
	void OnLook(const struct FInputActionValue& Value);
	void OnJump();
	void OnInteract();
	void OnEnterExit();
	void OnPower(int32 Slot);
	void OnPowerOne();
	void OnPowerTwo();
	void OnPowerThree();
	void OnPunch();

	// Driving
	void OnThrottle(const struct FInputActionValue& Value);
	void OnBrake(const struct FInputActionValue& Value);
	void OnSteer(const struct FInputActionValue& Value);
	void OnHandbrake(const struct FInputActionValue& Value);
	void OnShiftUp();
	void OnShiftDown();
	void OnToggleGearbox();

	// Always
	void OnPhone();
	void OnPause();

	UPROPERTY()
	TObjectPtr<UInputMappingContext> FootContext;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> DriveContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY()
	TObjectPtr<UInputAction> JumpAction;
	UPROPERTY()
	TObjectPtr<UInputAction> PunchAction;
	UPROPERTY()
	TObjectPtr<UInputAction> InteractAction;
	UPROPERTY()
	TObjectPtr<UInputAction> EnterExitAction;
	UPROPERTY()
	TObjectPtr<UInputAction> PowerActions[3];
	UPROPERTY()
	TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY()
	TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY()
	TObjectPtr<UInputAction> SteerAction;
	UPROPERTY()
	TObjectPtr<UInputAction> HandbrakeAction;
	UPROPERTY()
	TObjectPtr<UInputAction> ShiftUpAction;
	UPROPERTY()
	TObjectPtr<UInputAction> ShiftDownAction;
	UPROPERTY()
	TObjectPtr<UInputAction> GearboxAction;
	UPROPERTY()
	TObjectPtr<UInputAction> PhoneAction;
	UPROPERTY()
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	double Throttle = 0.0;
	double Brake = 0.0;
	double Steer = 0.0;
	double Handbrake = 0.0;
	bool bMenuOpen = false;
	bool bDrivingContext = false;
	bool bScrollHeld = false;
	double NextScroll = 0.0;
	float NextFightTurn = 1.0f;
};
