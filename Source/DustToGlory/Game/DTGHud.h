// Everything the player reads, drawn in code.
//
// There are no UMG assets: the HUD draws the driving instruments, the minimap, a notification feed, and the
// menus -- building services, the phone (calls, texts and email), court and jail. Drawing them in code keeps
// the whole game buildable from source with no editor work, and every screen reads straight from the
// simulation, so a menu can never show something the simulation doesn't believe.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "Sim/GameState.h"

#include "DTGHud.generated.h"

class ADTGCharacter;
class ADTGVehicle;
class UDTGGameSubsystem;

/** Which screen the menu is showing. */
UENUM()
enum class EDTGScreen : uint8
{
	None,
	Building,   // the services of whatever the player walked into
	Phone,      // calls, texts, email
	Court,      // plea, trial, lawyer, co-defendant
	Jail,       // serve the day, programs, parole, appeal -- and the ways out
	Garage      // the player's cars
};

UCLASS()
class DUSTTOGLORY_API ADTGHud : public AHUD
{
	GENERATED_BODY()

public:
	ADTGHud();

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	/** Opens a screen (the player controller calls this when the player interacts). */
	void Open(EDTGScreen Screen, int32 Context = -1);
	void Close();
	EDTGScreen Screen() const { return Current; }

	/** Runs the highlighted option on the open screen. */
	void Choose(int32 Index);
	void MoveSelection(int32 Delta);

	UFUNCTION()
	void OnNotice(const FString& Text);

private:
	struct FNotice
	{
		FString Text;
		double Until = 0.0;
	};

	void DrawDriving(const ADTGVehicle& Car);
	void DrawOnFoot(const ADTGCharacter& Player);
	void DrawStatusBar();
	void DrawMinimap();
	void DrawNotices();
	void DrawMenu();
	TArray<FString> BuildOptions() const;
	void RunBuildingService(int32 Index);
	void RunJailOption(int32 Index);
	void RunCourtOption(int32 Index);

	void Text(const FString& Line, float X, float Y, const FLinearColor& Colour = FLinearColor::White, float Scale = 1.0f);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Colour);

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TObjectPtr<UFont> Face;

	EDTGScreen Current = EDTGScreen::None;
	int32 ContextId = -1;
	int32 Selection = 0;
	TArray<FNotice> Notices;
};
