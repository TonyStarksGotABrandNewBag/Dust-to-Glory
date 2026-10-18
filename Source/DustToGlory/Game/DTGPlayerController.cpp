#include "Game/DTGPlayerController.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

#include "Game/DTGCharacter.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGHud.h"
#include "Game/DTGVehicle.h"

using namespace dtg::sim;

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	/** Maps a key to an action, optionally negated or swizzled, the way the editor's modifiers would. */
	void MapKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bNegate = false, bool bSwizzleYx = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bSwizzleYx)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}
}

ADTGPlayerController::ADTGPlayerController()
{
	bShowMouseCursor = false;
}

void ADTGPlayerController::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	SetDrivingContext(false);
}

void ADTGPlayerController::BuildInput()
{
	if (FootContext)
	{
		return;
	}
	MoveAction = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	EnterExitAction = MakeAction(this, TEXT("IA_EnterExit"), EInputActionValueType::Boolean);
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		PowerActions[Slot] = MakeAction(this, *FString::Printf(TEXT("IA_Power%d"), Slot + 1), EInputActionValueType::Boolean);
	}
	ThrottleAction = MakeAction(this, TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	BrakeAction = MakeAction(this, TEXT("IA_Brake"), EInputActionValueType::Axis1D);
	SteerAction = MakeAction(this, TEXT("IA_Steer"), EInputActionValueType::Axis1D);
	HandbrakeAction = MakeAction(this, TEXT("IA_Handbrake"), EInputActionValueType::Axis1D);
	ShiftUpAction = MakeAction(this, TEXT("IA_ShiftUp"), EInputActionValueType::Boolean);
	ShiftDownAction = MakeAction(this, TEXT("IA_ShiftDown"), EInputActionValueType::Boolean);
	GearboxAction = MakeAction(this, TEXT("IA_Gearbox"), EInputActionValueType::Boolean);
	PhoneAction = MakeAction(this, TEXT("IA_Phone"), EInputActionValueType::Boolean);
	PauseAction = MakeAction(this, TEXT("IA_Pause"), EInputActionValueType::Boolean);

	// ---- On foot
	FootContext = NewObject<UInputMappingContext>(this, TEXT("IMC_OnFoot"));
	MapKey(FootContext, MoveAction, EKeys::W);                       // X = forward
	MapKey(FootContext, MoveAction, EKeys::S, /*bNegate*/ true);
	MapKey(FootContext, MoveAction, EKeys::D, false, /*bSwizzle*/ true);  // Y = right
	MapKey(FootContext, MoveAction, EKeys::A, true, true);
	MapKey(FootContext, MoveAction, EKeys::Gamepad_LeftY);
	MapKey(FootContext, MoveAction, EKeys::Gamepad_LeftX, false, true);
	MapKey(FootContext, LookAction, EKeys::Mouse2D);
	MapKey(FootContext, LookAction, EKeys::Gamepad_RightX);
	MapKey(FootContext, LookAction, EKeys::Gamepad_RightY, false, true);
	MapKey(FootContext, JumpAction, EKeys::SpaceBar);
	MapKey(FootContext, JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(FootContext, InteractAction, EKeys::E);
	MapKey(FootContext, InteractAction, EKeys::Gamepad_FaceButton_Left);
	MapKey(FootContext, EnterExitAction, EKeys::F);
	MapKey(FootContext, EnterExitAction, EKeys::Gamepad_FaceButton_Top);
	MapKey(FootContext, PowerActions[0], EKeys::One);
	MapKey(FootContext, PowerActions[1], EKeys::Two);
	MapKey(FootContext, PowerActions[2], EKeys::Three);
	MapKey(FootContext, PhoneAction, EKeys::Tab);
	MapKey(FootContext, PauseAction, EKeys::Escape);

	// ---- Driving
	DriveContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Driving"));
	MapKey(DriveContext, ThrottleAction, EKeys::W);
	MapKey(DriveContext, ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
	MapKey(DriveContext, BrakeAction, EKeys::S);
	MapKey(DriveContext, BrakeAction, EKeys::Gamepad_LeftTriggerAxis);
	MapKey(DriveContext, SteerAction, EKeys::D);
	MapKey(DriveContext, SteerAction, EKeys::A, /*bNegate*/ true);
	MapKey(DriveContext, SteerAction, EKeys::Gamepad_LeftX);
	MapKey(DriveContext, HandbrakeAction, EKeys::SpaceBar);
	MapKey(DriveContext, HandbrakeAction, EKeys::Gamepad_FaceButton_Right);
	MapKey(DriveContext, ShiftUpAction, EKeys::E);
	MapKey(DriveContext, ShiftUpAction, EKeys::Gamepad_RightShoulder);
	MapKey(DriveContext, ShiftDownAction, EKeys::Q);
	MapKey(DriveContext, ShiftDownAction, EKeys::Gamepad_LeftShoulder);
	MapKey(DriveContext, GearboxAction, EKeys::G);
	MapKey(DriveContext, EnterExitAction, EKeys::F);
	MapKey(DriveContext, EnterExitAction, EKeys::Gamepad_FaceButton_Top);
	MapKey(DriveContext, LookAction, EKeys::Mouse2D);
	MapKey(DriveContext, LookAction, EKeys::Gamepad_RightX);
	MapKey(DriveContext, PhoneAction, EKeys::Tab);
	MapKey(DriveContext, PauseAction, EKeys::Escape);
}

void ADTGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInput();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogDTG, Error, TEXT("Enhanced Input is not the input component class; check DefaultInput.ini."));
		return;
	}
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADTGPlayerController::OnMove);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADTGPlayerController::OnMoveReleased);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADTGPlayerController::OnLook);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnJump);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnInteract);
	Input->BindAction(EnterExitAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnEnterExit);
	Input->BindAction(PowerActions[0], ETriggerEvent::Started, this, &ADTGPlayerController::OnPowerOne);
	Input->BindAction(PowerActions[1], ETriggerEvent::Started, this, &ADTGPlayerController::OnPowerTwo);
	Input->BindAction(PowerActions[2], ETriggerEvent::Started, this, &ADTGPlayerController::OnPowerThree);
	Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ADTGPlayerController::OnThrottle);
	Input->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ADTGPlayerController::OnThrottle);
	Input->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ADTGPlayerController::OnBrake);
	Input->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ADTGPlayerController::OnBrake);
	Input->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ADTGPlayerController::OnSteer);
	Input->BindAction(SteerAction, ETriggerEvent::Completed, this, &ADTGPlayerController::OnSteer);
	Input->BindAction(HandbrakeAction, ETriggerEvent::Triggered, this, &ADTGPlayerController::OnHandbrake);
	Input->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ADTGPlayerController::OnHandbrake);
	Input->BindAction(ShiftUpAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnShiftUp);
	Input->BindAction(ShiftDownAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnShiftDown);
	Input->BindAction(GearboxAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnToggleGearbox);
	Input->BindAction(PhoneAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnPhone);
	Input->BindAction(PauseAction, ETriggerEvent::Started, this, &ADTGPlayerController::OnPause);
}

void ADTGPlayerController::SetDrivingContext(bool bDriving)
{
	BuildInput();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(bDriving ? DriveContext : FootContext, 0);
	bDrivingContext = bDriving;
	Throttle = Brake = Steer = Handbrake = 0.0;
}

void ADTGPlayerController::SetMenuOpen(bool bOpen)
{
	bMenuOpen = bOpen;
	bShowMouseCursor = bOpen;
	if (bOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
	if (bOpen)
	{
		// Let go of the pedals so a menu never leaves the car accelerating.
		Throttle = Brake = Steer = Handbrake = 0.0;
	}
}

void ADTGPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	ADTGCharacter* Player = Cast<ADTGCharacter>(GetPawn());
	ADTGVehicle* Car = Player ? Player->DrivingNow() : Cast<ADTGVehicle>(GetPawn());
	if (!Car)
	{
		return;
	}
	DriverInput Input;
	Input.Throttle = FMath::Clamp(Throttle, 0.0, 1.0);
	Input.Brake = FMath::Clamp(Brake, 0.0, 1.0);
	Input.Steer = FMath::Clamp(Steer, -1.0, 1.0);
	Input.Handbrake = FMath::Clamp(Handbrake, 0.0, 1.0);
	Car->SetDriverInput(Input);
	// The hidden driver rides with the car, so getting out puts them back where the car is.
	if (Player && Player->DrivingNow() == Car)
	{
		Player->SetActorLocation(Car->GetActorLocation());
		if (GetViewTarget() != Car)
		{
			SetViewTargetWithBlend(Car, 0.3f);
		}
	}
}

// ------------------------------------------------------------------------------------------- On foot

void ADTGPlayerController::OnMove(const FInputActionValue& Value)
{
	if (bMenuOpen)
	{
		// In a menu, forward and back move the highlight instead of the player -- once per press, with a
		// repeat if the key is held, rather than once per frame.
		const float Forward = Value.Get<FVector2D>().X;
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (FMath::Abs(Forward) > 0.6f && Now >= NextScroll)
		{
			if (ADTGHud* Hud = Cast<ADTGHud>(GetHUD()))
			{
				Hud->MoveSelection(Forward > 0.0f ? -1 : 1);
			}
			NextScroll = Now + (bScrollHeld ? 0.12 : 0.35);
			bScrollHeld = true;
		}
		return;
	}
	if (bDrivingContext)
	{
		return;
	}
	APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawOnly(0.0, GetControlRotation().Yaw, 0.0);
	Pawn->AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Axis.X);
	Pawn->AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Axis.Y);
}

void ADTGPlayerController::OnMoveReleased()
{
	bScrollHeld = false;
	NextScroll = 0.0;
}

void ADTGPlayerController::OnLook(const FInputActionValue& Value)
{
	if (bMenuOpen)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	AddYawInput(Axis.X);
	AddPitchInput(-Axis.Y);
}

void ADTGPlayerController::OnJump()
{
	if (ACharacter* Player = Cast<ACharacter>(GetPawn()))
	{
		Player->Jump();
	}
}

void ADTGPlayerController::OnInteract()
{
	ADTGHud* Hud = Cast<ADTGHud>(GetHUD());
	if (!Hud)
	{
		return;
	}
	// In a menu, this key is "do the highlighted thing".
	if (Hud->Screen() != EDTGScreen::None)
	{
		Hud->Choose(-1);
		return;
	}
	ADTGCharacter* Player = Cast<ADTGCharacter>(GetPawn());
	if (!Player || !Game)
	{
		return;
	}
	const FDTGInteraction& Focus = Player->Focus();
	if (Focus.BuildingId >= 0)
	{
		// Jail and court have their own screens; everything else opens its service list.
		Hud->Open(EDTGScreen::Building, Focus.BuildingId);
	}
	else if (!Focus.CarId.IsEmpty())
	{
		const OfferResponse Result = Game->Sim().OfferInPerson(TCHAR_TO_UTF8(*Focus.CarId), Game->Sim().Funds.Balance() / 4);
		Game->OnNotice.Broadcast(UTF8_TO_TCHAR(Result.Reply.c_str()));
	}
}

void ADTGPlayerController::OnEnterExit()
{
	ADTGCharacter* Player = Cast<ADTGCharacter>(GetPawn());
	if (!Player)
	{
		return;
	}
	ADTGVehicle* Car = Player->ToggleVehicle();
	SetDrivingContext(Player->DrivingNow() != nullptr);
	if (!Player->DrivingNow())
	{
		SetViewTargetWithBlend(Player, 0.3f);
		if (Car)
		{
			Car->SetDriverInput(DriverInput());
		}
	}
}

void ADTGPlayerController::OnPowerOne()
{
	OnPower(0);
}

void ADTGPlayerController::OnPowerTwo()
{
	OnPower(1);
}

void ADTGPlayerController::OnPowerThree()
{
	OnPower(2);
}

void ADTGPlayerController::OnPower(int32 Slot)
{
	if (bMenuOpen)
	{
		return;
	}
	if (ADTGCharacter* Player = Cast<ADTGCharacter>(GetPawn()))
	{
		Player->UsePower(Slot);
	}
}

// ------------------------------------------------------------------------------------------- Driving

void ADTGPlayerController::OnThrottle(const FInputActionValue& Value)
{
	Throttle = bMenuOpen ? 0.0 : Value.Get<float>();
}

void ADTGPlayerController::OnBrake(const FInputActionValue& Value)
{
	Brake = bMenuOpen ? 0.0 : Value.Get<float>();
}

void ADTGPlayerController::OnSteer(const FInputActionValue& Value)
{
	Steer = bMenuOpen ? 0.0 : Value.Get<float>();
}

void ADTGPlayerController::OnHandbrake(const FInputActionValue& Value)
{
	Handbrake = bMenuOpen ? 0.0 : Value.Get<float>();
}

void ADTGPlayerController::OnShiftUp()
{
	ADTGCharacter* Player = Cast<ADTGCharacter>(GetPawn());
	if (ADTGVehicle* Car = Player ? Player->DrivingNow() : nullptr)
	{
		Car->ShiftUp();
	}
}

void ADTGPlayerController::OnShiftDown()
{
	ADTGCharacter* Player = Cast<ADTGCharacter>(GetPawn());
	if (ADTGVehicle* Car = Player ? Player->DrivingNow() : nullptr)
	{
		Car->ShiftDown();
	}
}

void ADTGPlayerController::OnToggleGearbox()
{
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	// Manual or automatic is the player's standing choice, kept in the save, not a per-car toggle.
	const bool bManual = Game->Sim().AutomaticGearbox();
	const ActionResult Result = Game->Sim().SetTransmission(bManual);
	Game->OnNotice.Broadcast(UTF8_TO_TCHAR(Result.Message.c_str()));
	ADTGCharacter* Player = Cast<ADTGCharacter>(GetPawn());
	if (ADTGVehicle* Car = Player ? Player->DrivingNow() : nullptr)
	{
		Car->Chassis().Dynamics().bAutomatic = Game->Sim().AutomaticGearbox();
	}
}

// ------------------------------------------------------------------------------------------- Always

void ADTGPlayerController::OnPhone()
{
	if (ADTGHud* Hud = Cast<ADTGHud>(GetHUD()))
	{
		Hud->Open(Hud->Screen() == EDTGScreen::Phone ? EDTGScreen::None : EDTGScreen::Phone);
	}
}

void ADTGPlayerController::OnPause()
{
	if (ADTGHud* Hud = Cast<ADTGHud>(GetHUD()))
	{
		Hud->Close();
	}
	else
	{
		SetMenuOpen(false);
	}
}
