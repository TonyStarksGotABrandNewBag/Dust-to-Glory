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
	Garage,     // the player's cars
	Talk,       // a conversation with someone in the street
	Events,     // what's on at the venues, and entering it
	Service,    // one building service: the job board, the showroom floor, the stock desk
	Title       // the front end: new game, continue, slots
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
	/** Opens a conversation with a particular NPC mind. */
	void OpenTalk(const FString& MindId);
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
	void DrawFight();
	void DrawNotices();
	void DrawMenu();
	TArray<FString> BuildOptions() const;
	/**
	 * The option list, rebuilt at a few times a second rather than every frame.
	 *
	 * Building it is not free: a Jail screen quotes five escape plans, a Phone screen formats every
	 * message in the inbox, and a Building screen looks the building up. None of that changes between
	 * frames, and the player cannot act on it faster than this either.
	 */
	const TArray<FString>& Options() const;
	void InvalidateOptions() { OptionsUntil = 0.0; }
	void RunBuildingService(int32 Index);
	void RunJailOption(int32 Index);
	void RunCourtOption(int32 Index);
	void RunTalkOption(int32 Index);
	void RunTitleOption(int32 Index);
	void RunEventOption(int32 Index);
	void RunServiceOption(int32 Index);

	/**
	 * One row of a service screen: what it says and what doing it needs.
	 *
	 * Built once and used for both drawing and acting, so the row the player is looking at and the
	 * row the game acts on cannot drift apart -- which is what happens when a menu is listed in one
	 * place and dispatched by index in another.
	 */
	struct FServiceRow
	{
		FString Label;
		FString Key;        // a model id, a career id, a ticker, a law id -- whatever the action needs
		Money Cost = 0;
	};
	/** The rows of the service currently open. */
	TArray<FServiceRow> ServiceRows() const;
	/** Clears away everything standing in the old world and puts the player where the new one starts. */
	void RestartWorld(bool bRebuildTerrain);

	void Text(const FString& Line, float X, float Y, const FLinearColor& Colour = FLinearColor::White, float Scale = 1.0f);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Colour);

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TObjectPtr<UFont> Face;

	EDTGScreen Current = EDTGScreen::None;
	int32 ContextId = -1;
	FString TalkingTo;
	/** Which service of the open building is showing, e.g. "Job board". */
	FString ServiceName;
	FString LastReply;
	int32 Selection = 0;
	TArray<FNotice> Notices;
	mutable TArray<FString> CachedOptions;
	mutable double OptionsUntil = 0.0;
	mutable EDTGScreen CachedFor = EDTGScreen::None;
	mutable int32 CachedContext = -1;
};
