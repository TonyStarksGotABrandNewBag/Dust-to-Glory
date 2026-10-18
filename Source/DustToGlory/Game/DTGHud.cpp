#include "Game/DTGHud.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "Game/DTGCharacter.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGEventDirector.h"
#include "Game/DTGGameMode.h"
#include "Game/DTGInterior.h"
#include "Game/DTGTrafficDirector.h"
#include "Game/DTGVehicle.h"
#include "Game/DTGWorldBuilder.h"
#include "Game/DTGPlayerController.h"

using namespace dtg::sim;

namespace
{
	const FLinearColor PanelColour(0.02f, 0.02f, 0.03f, 0.82f);
	const FLinearColor Accent(0.95f, 0.72f, 0.25f);
	const FLinearColor Dim(0.65f, 0.66f, 0.7f);

	/** Not named Money: that is the simulation's own type, and the two would be ambiguous here. */
	FString Cash(dtg::sim::Money Cents)
	{
		return UTF8_TO_TCHAR(FormatMoney(Cents).c_str());
	}

	FString Utf8(const std::string& Text)
	{
		return UTF8_TO_TCHAR(Text.c_str());
	}
}

ADTGHud::ADTGHud()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADTGHud::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	Face = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Game)
	{
		Game->OnNotice.AddDynamic(this, &ADTGHud::OnNotice);
	}
}

void ADTGHud::OnNotice(const FString& Text)
{
	FNotice Notice;
	Notice.Text = Text;
	Notice.Until = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + 9.0;
	Notices.Add(Notice);
	while (Notices.Num() > 6)
	{
		Notices.RemoveAt(0);
	}
}

void ADTGHud::Text(const FString& Line, float X, float Y, const FLinearColor& Colour, float Scale)
{
	if (!Canvas)
	{
		return;
	}
	UFont* const Use = Face ? Face.Get() : (GEngine ? GEngine->GetMediumFont() : nullptr);
	if (!Use)
	{
		return;
	}
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Line), Use, Colour);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
	Canvas->DrawItem(Item);
}

void ADTGHud::Panel(float X, float Y, float W, float H, const FLinearColor& Colour)
{
	if (!Canvas)
	{
		return;
	}
	FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Colour);
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
}

void ADTGHud::DrawHUD()
{
	Super::DrawHUD();
	if (!Game || !Game->IsContentLoaded() || !Canvas)
	{
		return;
	}
	DrawStatusBar();
	const ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
	if (Player && Player->DrivingNow())
	{
		DrawDriving(*Player->DrivingNow());
	}
	else if (Player)
	{
		DrawOnFoot(*Player);
	}
	DrawMinimap();
	DrawFight();
	DrawNotices();
	if (Current != EDTGScreen::None)
	{
		DrawMenu();
	}
}

void ADTGHud::DrawStatusBar()
{
	const GameState& Sim = Game->Sim();
	Panel(0.0f, 0.0f, Canvas->SizeX, 34.0f, PanelColour);
	const FString Clock = FString::Printf(TEXT("Day %d  %02d:%02d"), Sim.Clock.Day() + 1, Sim.Clock.Hour(), Sim.Clock.Minute());
	Text(Clock, 16.0f, 8.0f, Dim);
	Text(Cash(Sim.Funds.Balance()), 170.0f, 8.0f, Accent);
	{
		// Which town the player is actually in, not where they started. While driving that is the
		// car: the character is hidden back at the kerb they got in at and never moves.
		const ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
		const FVector2D Where = Game->ToSim(ADTGCharacter::InterestPointFor(Player));
		const CityInfo* Here = Sim.City.CityAt(Where.X, Where.Y);
		Text(Utf8(Here ? Here->Name : std::string("open country")), 320.0f, 8.0f, Dim);
	}
	if (Sim.Wanted.IsWanted())
	{
		FString Stars;
		for (int32 Star = 0; Star < Sim.Wanted.Stars(); ++Star)
		{
			Stars += TEXT("*");
		}
		Text(Stars, Canvas->SizeX - 220.0f, 8.0f, FLinearColor(0.95f, 0.3f, 0.25f), 1.4f);
	}
	if (Sim.Justice.IsFugitive())
	{
		Text(TEXT("WANTED: ESCAPEE"), Canvas->SizeX - 420.0f, 8.0f, FLinearColor(0.95f, 0.3f, 0.25f));
	}
	// Health, but only once something has happened to it.
	if (Sim.Health() < CombatSystem::PlayerMaxHealth - 0.5)
	{
		const float BarW = 220.0f;
		const float Fraction = static_cast<float>(Sim.Health() / CombatSystem::PlayerMaxHealth);
		Panel(16.0f, 42.0f, BarW, 14.0f, FLinearColor(0.1f, 0.03f, 0.03f, 0.85f));
		Panel(16.0f, 42.0f, BarW * Fraction, 14.0f,
			Fraction > 0.5f ? FLinearColor(0.3f, 0.7f, 0.3f) : FLinearColor(0.85f, 0.35f, 0.2f));
	}
	// Weather, and what it is doing to the road, once it is doing anything.
	const Weather Sky = Sim.Weather.Current();
	if (Sky != Weather::Clear)
	{
		const double Grip = WeatherGrip(Sky);
		Text(FString::Printf(TEXT("%s%s"), UTF8_TO_TCHAR(WeatherName(Sky)),
				Grip < 0.95 ? *FString::Printf(TEXT("  --  %.0f%% grip"), Grip * 100.0) : TEXT("")),
			470.0f, 8.0f, Grip < 0.8 ? FLinearColor(0.9f, 0.7f, 0.3f) : Dim);
	}
	const int Unread = Sim.Phone.Unread();
	if (Unread > 0)
	{
		Text(FString::Printf(TEXT("%d new message(s)  [Tab]"), Unread), Canvas->SizeX - 640.0f, 8.0f, Accent);
	}
}

void ADTGHud::DrawDriving(const ADTGVehicle& Car)
{
	const DynamicsTelemetry& Telemetry = Car.Chassis().Dynamics().Telemetry();
	const float X = Canvas->SizeX - 290.0f;
	const float Y = Canvas->SizeY - 150.0f;
	Panel(X - 14.0f, Y - 14.0f, 280.0f, 136.0f, PanelColour);
	Text(FString::Printf(TEXT("%3.0f km/h"), Car.SpeedKph()), X, Y, FLinearColor::White, 2.0f);
	Text(FString::Printf(TEXT("%5.0f rpm"), Telemetry.EngineRpm), X, Y + 52.0f, Telemetry.bRevLimiter ? FLinearColor::Red : Dim);

	FString Gear = TEXT("N");
	if (Telemetry.Gear > 0)
	{
		Gear = FString::FromInt(Telemetry.Gear);
	}
	else if (Telemetry.Gear < 0)
	{
		Gear = TEXT("R");
	}
	const bool bManual = !Car.Chassis().Dynamics().bAutomatic;
	Text(FString::Printf(TEXT("%s  %s"), *Gear, bManual ? TEXT("MT") : TEXT("AT")), X + 190.0f, Y, Accent, 2.0f);
	if (bManual)
	{
		Text(TEXT("[Q] down  [E] up  [G] auto"), X, Y + 82.0f, Dim, 0.85f);
	}
	else
	{
		Text(TEXT("[G] manual"), X, Y + 82.0f, Dim, 0.85f);
	}
	if (Telemetry.Boost > 0.01)
	{
		Text(FString::Printf(TEXT("boost %.2f bar"), Telemetry.BoostBar), X, Y + 104.0f, Dim, 0.85f);
	}
	if (Car.IsSwamped())
	{
		Text(TEXT("The engine is drowning -- get out of the water."), 40.0f, Canvas->SizeY * 0.5f, FLinearColor(0.95f, 0.4f, 0.3f));
	}
}

void ADTGHud::DrawOnFoot(const ADTGCharacter& Player)
{
	const FDTGInteraction& Focus = Player.Focus();
	if (!Focus.IsValid())
	{
		return;
	}
	const float X = Canvas->SizeX * 0.5f - 200.0f;
	const float Y = Canvas->SizeY - 160.0f;
	Panel(X - 12.0f, Y - 10.0f, 420.0f, 60.0f, PanelColour);
	Text(Focus.Title, X, Y, FLinearColor::White);
	Text(FString::Printf(TEXT("[E] %s"), *Focus.Prompt), X, Y + 24.0f, Accent);
}

void ADTGHud::DrawMinimap()
{
	const GameState& Sim = Game->Sim();
	const ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
	const FVector2D Here = Game->ToSim(ADTGCharacter::InterestPointFor(Player));

	const float Size = 190.0f;
	const float X = 16.0f;
	const float Y = Canvas->SizeY - Size - 16.0f;
	Panel(X, Y, Size, Size, PanelColour);
	// 400 m of ground across the map, north up (the simulation's Y points south, so screen Y matches).
	const double Span = 400.0;
	auto Project = [&](double SimX, double SimY, FVector2D& Out)
	{
		const double Dx = (SimX - Here.X) / Span;
		const double Dy = (SimY - Here.Y) / Span;
		if (FMath::Abs(Dx) > 0.5 || FMath::Abs(Dy) > 0.5)
		{
			return false;
		}
		Out = FVector2D(X + Size * (0.5 + Dx), Y + Size * (0.5 + Dy));
		return true;
	};
	// Only what is on the map. Walking the whole state's roads and buildings to draw two hundred
	// metres of them was the most expensive thing the HUD did, every frame, for a 190-pixel square.
	for (const Intersection* Node : Sim.City.NodesNear(Here.X, Here.Y, Span))
	{
		FVector2D Point;
		if (Node && Project(Node->X, Node->Y, Point))
		{
			Panel(Point.X - 1.0f, Point.Y - 1.0f, 3.0f, 3.0f, FLinearColor(0.45f, 0.45f, 0.5f, 0.9f));
		}
	}
	for (const BuildingPlan* Building : Sim.City.BuildingsNear(Here.X, Here.Y, Span))
	{
		FVector2D Point;
		if (Building && Project(Building->DoorX, Building->DoorY, Point))
		{
			const FLinearColor Colour = IsResidential(Building->Role) ? FLinearColor(0.4f, 0.6f, 0.9f, 0.9f) : FLinearColor(0.8f, 0.75f, 0.6f, 0.9f);
			Panel(Point.X - 2.0f, Point.Y - 2.0f, 4.0f, 4.0f, Colour);
		}
	}
	Panel(X + Size * 0.5f - 3.0f, Y + Size * 0.5f - 3.0f, 6.0f, 6.0f, Accent);
}

void ADTGHud::DrawFight()
{
	const GameState& Sim = Game->Sim();
	if (Sim.Fights.Fighters().empty())
	{
		return;
	}
	// Who you are up against, and how they are doing.
	float Y = Canvas->SizeY * 0.3f;
	const float X = Canvas->SizeX - 290.0f;
	for (const Fighter& Who : Sim.Fights.Fighters())
	{
		if (Who.IsDown())
		{
			continue;
		}
		Panel(X - 10.0f, Y - 4.0f, 270.0f, 36.0f, PanelColour);
		Text(Utf8(Who.Name), X, Y, Who.bSuperhuman ? FLinearColor(0.95f, 0.55f, 0.95f) : FLinearColor::White, 0.9f);
		const float Fraction = static_cast<float>(Who.Health / FMath::Max(1.0, Who.MaxHealth));
		Panel(X, Y + 20.0f, 250.0f, 8.0f, FLinearColor(0.1f, 0.03f, 0.03f, 0.9f));
		Panel(X, Y + 20.0f, 250.0f * Fraction, 8.0f, FLinearColor(0.8f, 0.3f, 0.25f));
		Y += 42.0f;
	}
}

void ADTGHud::DrawNotices()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Notices.RemoveAll([Now](const FNotice& Notice) { return Notice.Until < Now; });
	float Y = 48.0f;
	for (const FNotice& Notice : Notices)
	{
		Panel(12.0f, Y - 4.0f, 560.0f, 26.0f, PanelColour);
		Text(Notice.Text, 20.0f, Y, FLinearColor::White, 0.95f);
		Y += 30.0f;
	}
}

// ============================================================================================ Menus

void ADTGHud::Open(EDTGScreen InScreen, int32 Context)
{
	Current = InScreen;
	ContextId = Context;
	Selection = 0;
	if (ADTGPlayerController* Controller = Cast<ADTGPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetMenuOpen(InScreen != EDTGScreen::None);
	}
}

void ADTGHud::Close()
{
	Open(EDTGScreen::None);
}

void ADTGHud::MoveSelection(int32 Delta)
{
	const int32 Count = Options().Num();
	if (Count > 0)
	{
		Selection = (Selection + Delta + Count) % Count;
	}
}

const TArray<FString>& ADTGHud::Options() const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// Rebuilt when the screen changes, when something told it to, or a few times a second anyway so a
	// menu still reflects the world moving underneath it.
	if (Now >= OptionsUntil || CachedFor != Current || CachedContext != ContextId)
	{
		CachedOptions = BuildOptions();
		CachedFor = Current;
		CachedContext = ContextId;
		OptionsUntil = Now + 0.2;
	}
	return CachedOptions;
}

TArray<FString> ADTGHud::BuildOptions() const
{
	TArray<FString> Options;
	if (!Game || !Game->IsContentLoaded())
	{
		return Options;
	}
	const GameState& Sim = Game->Sim();
	switch (Current)
	{
	case EDTGScreen::Building:
	{
		if (const BuildingPlan* Building = Sim.City.FindBuilding(ContextId))
		{
			for (const std::string& Service : ServicesForRole(Building->Role))
			{
				Options.Add(Utf8(Service));
			}
			const ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
			if (Player && Player->IsIndoors())
			{
				Options.Add(TEXT("Step outside"));
			}
		}
		break;
	}
	case EDTGScreen::Phone:
	{
		for (const Message& Entry : Sim.Phone.All())
		{
			const TCHAR* Kind = Entry.Via == Channel::Call ? TEXT("Call") : (Entry.Via == Channel::Text ? TEXT("Text") : TEXT("Email"));
			Options.Add(FString::Printf(TEXT("%s %s  %s%s"), Kind, *Utf8(Entry.FromName),
				*Utf8(Entry.Subject.empty() ? Entry.Text.substr(0, 48) : Entry.Subject), Entry.bAwaitingReply ? TEXT("  [reply]") : TEXT("")));
		}
		if (Options.IsEmpty())
		{
			Options.Add(TEXT("Nothing yet."));
		}
		break;
	}
	case EDTGScreen::Court:
		Options.Add(TEXT("Hire a defense attorney"));
		Options.Add(FString::Printf(TEXT("Take the plea (%d days)"), Sim.Justice.PleaOffer(Sim.Player.SkillLevel(Skill::People))));
		Options.Add(FString::Printf(TEXT("Go to trial (conviction odds %.0f%%)"), Sim.Justice.ConvictionChance(Sim.Player.SkillLevel(Skill::People)) * 100.0));
		if (!Sim.Justice.Case().CoDefendantId.empty())
		{
			Options.Add(FString::Printf(TEXT("Testify against %s"), *Utf8(Sim.Justice.Case().CoDefendantName)));
			Options.Add(TEXT("Keep quiet"));
		}
		break;
	case EDTGScreen::Jail:
		Options.Add(TEXT("Keep your head down"));
		Options.Add(TEXT("Work your jail job"));
		Options.Add(TEXT("Take classes"));
		Options.Add(TEXT("Work out in the yard"));
		Options.Add(TEXT("Request a parole hearing"));
		Options.Add(TEXT("File an appeal"));
		// The ways out, each with what it would take.
		for (int32 Plan = 1; Plan < static_cast<int32>(EscapePlan::Count); ++Plan)
		{
			const EscapeOdds Odds = Sim.QuoteEscape(static_cast<EscapePlan>(Plan));
			const FString Name = Utf8(EscapePlanName(static_cast<EscapePlan>(Plan)));
			Options.Add(Odds.bAvailable
				? FString::Printf(TEXT("%s -- %s, %d night(s), %.0f%%"), *Name, *Cash(Odds.Cost), Odds.PrepDays, Odds.Success * 100.0)
				: FString::Printf(TEXT("%s -- %s"), *Name, *Utf8(Odds.Reason)));
		}
		if (Sim.Justice.Escape().Plan != EscapePlan::None)
		{
			Options.Add(TEXT("Work on tonight's plan"));
			Options.Add(TEXT("Go tonight"));
			Options.Add(TEXT("Let it go"));
		}
		break;
	case EDTGScreen::Garage:
		for (const OwnedVehicle& Car : Sim.Garage)
		{
			const VehicleModel* Model = Sim.Vehicles.FindModel(Car.ModelId);
			Options.Add(FString::Printf(TEXT("%s  (%.0f%% condition, %.0f km)"), Model ? *Utf8(Model->DisplayName) : *Utf8(Car.ModelId),
				Car.Condition * 100.0, Car.MileageKm));
		}
		break;
	case EDTGScreen::Talk:
	{
		for (const DialogueOption& Option : Sim.TalkOptions(TCHAR_TO_UTF8(*TalkingTo)))
		{
			Options.Add(Option.Cost > 0
				? FString::Printf(TEXT("%s  (%s)"), *Utf8(Option.Label), *Cash(Option.Cost))
				: Utf8(Option.Label));
		}
		Options.Add(TEXT("Walk away"));
		break;
	}
	case EDTGScreen::Service:
		for (const FServiceRow& Row : ServiceRows())
		{
			Options.Add(Row.Label);
		}
		Options.Add(TEXT("Back"));
		break;
	case EDTGScreen::Events:
		for (int32 Kind = 0; Kind < static_cast<int32>(EventKind::Count); ++Kind)
		{
			const VenuePlan* Venue = Sim.City.FindVenue(static_cast<EventKind>(Kind));
			for (int32 Tier = 1; Tier <= 3; ++Tier)
			{
				Options.Add(FString::Printf(TEXT("%s, tier %d  --  %s at %s"), UTF8_TO_TCHAR(EventKindName(static_cast<EventKind>(Kind))),
					Tier, *Cash(Sim.EventEntryFee(static_cast<EventKind>(Kind), Tier)),
					Venue ? *Utf8(Venue->Name) : TEXT("no venue")));
			}
		}
		break;
	case EDTGScreen::Title:
		Options.Add(TEXT("Enter an event"));
		Options.Add(TEXT("Continue"));
		Options.Add(TEXT("New game"));
		Options.Add(TEXT("Save"));
		Options.Add(TEXT("Load"));
		// Every state the game can be played in, the one in use marked.
		for (const FString& File : Game->AvailableStates())
		{
			FString Label = File;
			Label.RemoveFromStart(TEXT("state"));
			Label.RemoveFromStart(TEXT("-"));
			Label.RemoveFromEnd(TEXT(".json"));
			if (Label.IsEmpty())
			{
				Label = TEXT("south-carolina");
			}
			Label = Label.Replace(TEXT("-"), TEXT(" "));
			Options.Add(FString::Printf(TEXT("Play in %s%s"), *Label, File == Game->StateFile() ? TEXT("  (current)") : TEXT("")));
		}
		break;
	default:
		break;
	}
	return Options;
}

void ADTGHud::DrawMenu()
{
	const TArray<FString>& Rows = Options();
	const float W = 720.0f;
	// How many rows actually fit between the title and the footer. A long list -- an inbox can hold a
	// hundred messages -- scrolls inside that window rather than running off the bottom of the screen
	// with the cursor somewhere nobody can see.
	const int32 Fits = FMath::Max(1, FMath::FloorToInt32((Canvas->SizeY - 260.0f) / 30.0f));
	const int32 Shown = FMath::Min(Rows.Num(), Fits);
	const float H = 120.0f + Shown * 30.0f + (Rows.Num() > Fits ? 26.0f : 0.0f);
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = (Canvas->SizeY - H) * 0.5f;
	Panel(X, Y, W, H, FLinearColor(0.02f, 0.02f, 0.03f, 0.93f));

	FString Title = TEXT("Menu");
	FString Subtitle;
	const GameState& Sim = Game->Sim();
	switch (Current)
	{
	case EDTGScreen::Building:
		if (const BuildingPlan* Building = Sim.City.FindBuilding(ContextId))
		{
			Title = Utf8(Building->Name);
			Subtitle = Utf8(Building->Address);
		}
		break;
	case EDTGScreen::Phone:
		Title = TEXT("Phone");
		Subtitle = FString::Printf(TEXT("%s  --  %d unread"), *Utf8(Sim.PhoneNumber), Sim.Phone.Unread());
		break;
	case EDTGScreen::Court:
		Title = TEXT("Horry County Courthouse");
		Subtitle = FString::Printf(TEXT("%d charge(s), %d days if convicted at trial"),
			static_cast<int32>(Sim.Justice.Case().Charges.size()), Sim.Justice.Case().TrialDays);
		break;
	case EDTGScreen::Jail:
		Title = TEXT("County Detention Center");
		Subtitle = FString::Printf(TEXT("Day %d of %d  --  good behavior %.0f%%"), Sim.Justice.Jail().DaysServed,
			Sim.Justice.Jail().SentenceDays, Sim.Justice.Jail().GoodBehavior * 100.0);
		break;
	case EDTGScreen::Garage:
		Title = TEXT("Your cars");
		break;
	case EDTGScreen::Talk:
		if (const NpcMind* Mind = Sim.Npcs.Find(TCHAR_TO_UTF8(*TalkingTo)))
		{
			Title = Utf8(Mind->Name);
			Subtitle = FString::Printf(TEXT("%s  --  %s"), *Utf8(Mind->Occupation),
				*Utf8(AwarenessStageName(Mind->Stage)));
			if (!Mind->Hometown.empty())
			{
				Subtitle += FString::Printf(TEXT("  --  %s"), *Utf8(Mind->Hometown));
			}
			// How they are taking the conversation: warmth first, then whether you are wearing on them.
			const TCHAR* Mood = Mind->Affinity > 0.4 ? TEXT("warm") : (Mind->Affinity < -0.2 ? TEXT("cold") : TEXT("polite"));
			Subtitle += FString::Printf(TEXT("  --  %s"), Mood);
			if (Mind->Patience < 0.5)
			{
				Subtitle += TEXT(", losing patience");
			}
		}
		break;
	case EDTGScreen::Service:
	{
		Title = ServiceName;
		const BuildingPlan* Where = Sim.City.FindBuilding(ContextId);
		Subtitle = Where ? FString::Printf(TEXT("%s  --  %s"), *Utf8(Where->Name), *Cash(Sim.Funds.Balance()))
						 : Cash(Sim.Funds.Balance());
		break;
	}
	case EDTGScreen::Events:
		Title = TEXT("What's on");
		Subtitle = TEXT("You race in whatever you drove here");
		break;
	case EDTGScreen::Title:
	{
		Title = TEXT("Dust to Glory");
		const StateCity* Start = Sim.Map.StartCity();
		Subtitle = FString::Printf(TEXT("%s, %s"), Start ? *Utf8(Start->Name) : TEXT("somewhere"), *Utf8(Sim.Map.Name));
		if (!Game->LoadedMods().IsEmpty())
		{
			Subtitle += FString::Printf(TEXT("  --  mods: %s"), *FString::Join(Game->LoadedMods(), TEXT(", ")));
		}
		break;
	}
	default:
		break;
	}
	Text(Title, X + 24.0f, Y + 20.0f, Accent, 1.5f);
	if (!Subtitle.IsEmpty())
	{
		Text(Subtitle, X + 24.0f, Y + 54.0f, Dim);
	}
	// The window follows the cursor, keeping it off the very edge where there is nothing to read ahead.
	const int32 First = FMath::Clamp(Selection - Fits / 2, 0, FMath::Max(0, Rows.Num() - Fits));
	float Row = Y + 90.0f;
	for (int32 Index = First; Index < FMath::Min(Rows.Num(), First + Fits); ++Index)
	{
		const bool bSelected = Index == Selection;
		if (bSelected)
		{
			Panel(X + 16.0f, Row - 4.0f, W - 32.0f, 28.0f, FLinearColor(0.16f, 0.15f, 0.1f, 0.9f));
		}
		Text(Rows[Index], X + 28.0f, Row, bSelected ? FLinearColor::White : Dim);
		Row += 30.0f;
	}
	if (Rows.Num() > Fits)
	{
		Text(FString::Printf(TEXT("%d of %d"), Selection + 1, Rows.Num()), X + 28.0f, Row + 2.0f, Dim, 0.9f);
	}
	if (Current == EDTGScreen::Talk && !LastReply.IsEmpty())
	{
		Text(FString::Printf(TEXT("\"%s\""), *LastReply), X + 24.0f, Y + H - 58.0f, FLinearColor(0.85f, 0.85f, 0.8f), 0.95f);
	}
	Text(TEXT("[W]/[S] choose   [E] do it   [Esc] leave"), X + 24.0f, Y + H - 32.0f, Dim, 0.9f);
}

void ADTGHud::Choose(int32 Index)
{
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	const int32 Count = Options().Num();
	if (Count <= 0)
	{
		return;
	}
	// The option list changes as the game does (a plan abandoned, a message read), so never act on a stale row.
	Selection = FMath::Clamp(Index >= 0 ? Index : Selection, 0, Count - 1);
	switch (Current)
	{
	case EDTGScreen::Building: RunBuildingService(Selection); break;
	case EDTGScreen::Talk: RunTalkOption(Selection); break;
	case EDTGScreen::Title: RunTitleOption(Selection); break;
	case EDTGScreen::Events: RunEventOption(Selection); break;
	case EDTGScreen::Service: RunServiceOption(Selection); break;
	case EDTGScreen::Court: RunCourtOption(Selection); break;
	case EDTGScreen::Jail: RunJailOption(Selection); break;
	case EDTGScreen::Phone:
	{
		const std::vector<Message>& All = Game->Sim().Phone.All();
		if (Selection < static_cast<int32>(All.size()))
		{
			const Message& Entry = All[static_cast<std::size_t>(Selection)];
			Game->Sim().Phone.MarkRead(Entry.Id);
			OnNotice(Utf8(Entry.Text));
			if (Entry.bAwaitingReply)
			{
				// A deal waiting on a yes: taking it is what the simulation calls responding.
				const ActionResult Reply = Game->Sim().RespondToMessage(Entry.Id, /*bAccept*/ true);
				OnNotice(Utf8(Reply.Message));
			}
		}
		break;
	}
	case EDTGScreen::Garage:
	{
		const std::vector<OwnedVehicle>& Cars = Game->Sim().Garage;
		if (Selection < static_cast<int32>(Cars.size()))
		{
			// Picking a car brings it to you: the simulation remembers which one is yours, and the
			// world puts that one on the kerb in place of whatever of yours was standing there.
			const FString Uid = Utf8(Cars[static_cast<std::size_t>(Selection)].Uid);
			Game->Sim().ActiveVehicleUid = Cars[static_cast<std::size_t>(Selection)].Uid;
			ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
			ADTGGameMode* Mode = Cast<ADTGGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
			if (Mode && Player)
			{
				if (Player->DrivingNow())
				{
					Player->ToggleVehicle(); // you cannot be sitting in the one being replaced
				}
				Mode->PlaceOwnedCar(Uid, Player->GetActorLocation());
				OnNotice(TEXT("It's outside."));
			}
			else
			{
				OnNotice(TEXT("That's the one you'll be driving."));
			}
		}
		break;
	}
	default:
		break;
	}
}

void ADTGHud::RunBuildingService(int32 Index)
{
	GameState& Sim = Game->Sim();
	const BuildingPlan* Building = Sim.City.FindBuilding(ContextId);
	if (!Building)
	{
		return;
	}
	const std::vector<std::string> Services = ServicesForRole(Building->Role);
	if (Index == static_cast<int32>(Services.size()))
	{
		// The extra row the menu adds while indoors.
		if (ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn()))
		{
			Player->LeaveBuilding();
		}
		Close();
		return;
	}
	if (Index < 0 || Index >= static_cast<int32>(Services.size()))
	{
		return;
	}
	const FString Service = Utf8(Services[static_cast<std::size_t>(Index)]);
	// Services that are a list of things -- the job board, the showroom floor, the stock desk -- open
	// a screen of their own. The rest act here.
	ServiceName = Service;
	if (ServiceRows().Num() > 0)
	{
		Selection = 0;
		InvalidateOptions();
		Open(EDTGScreen::Service, ContextId);
		return;
	}
	ServiceName.Reset();
	if (Service == TEXT("Serve your time") || Service == TEXT("Plan a way out"))
	{
		Open(EDTGScreen::Jail);
		return;
	}
	if (Service == TEXT("Court and records") || Service == TEXT("Public records"))
	{
		Open(EDTGScreen::Court);
		return;
	}
	if (Service == TEXT("Turn yourself in"))
	{
		const ActionResult Result = Sim.Surrender();
		OnNotice(Utf8(Result.Message));
		Close();
		return;
	}
	if (Service.StartsWith(TEXT("Buy or enter")))
	{
		// Standing in a house or apartment: buy this one.
		const ActionResult Result = Sim.BuyHome(Building->Id);
		OnNotice(Utf8(Result.Message));
		return;
	}
	if (Service == TEXT("Work a shift"))
	{
		// Only a job whose workplace is this kind of building can be worked here.
		const std::string Role = LotRoleName(Building->Role);
		for (const CareerProgress& Job : Sim.Jobs)
		{
			const CareerDef* Def = Sim.Careers.FindCareer(Job.CareerId);
			if (!Def || Def->Workplace != Role)
			{
				continue;
			}
			const ShiftResult Result = Sim.WorkShift(Job.CareerId, 0.7);
			OnNotice(Result.bValid
				? FString::Printf(TEXT("%d hours as %s: %s%s"), Result.HoursWorked, *Utf8(Result.Title), *Cash(Result.Pay),
					  Result.bPromoted ? TEXT("  --  and a promotion!") : TEXT(""))
				: FString(TEXT("Not on the schedule today.")));
			return;
		}
		OnNotice(TEXT("You don't work here. The career center has the job board."));
		return;
	}
	// The everyday reasons to walk in somewhere. The simulation owns what each costs and gives.
	using Amenity = GameState::Amenity;
	static const TMap<FString, Amenity> Amenities = {
		{TEXT("Eat"), Amenity::Meal},
		{TEXT("Rent a room"), Amenity::Lodging},
		{TEXT("Get treated"), Amenity::Treatment},
		{TEXT("Pay fines"), Amenity::Fines},
		{TEXT("Train"), Amenity::Gym},
		{TEXT("Compete"), Amenity::Track},
		{TEXT("Tryouts and games"), Amenity::Track},
		{TEXT("Study"), Amenity::Library},
		{TEXT("Attend service"), Amenity::Service},
		{TEXT("Watch a game"), Amenity::Leisure},
		{TEXT("Explore exhibits"), Amenity::Leisure},
		{TEXT("Browse"), Amenity::Leisure},
		{TEXT("Shop plants"), Amenity::Leisure},
		{TEXT("Gadgets"), Amenity::Leisure},
	};
	if (const Amenity* Kind = Amenities.Find(Service))
	{
		OnNotice(Utf8(Sim.UseAmenity(*Kind).Message));
		return;
	}
	if (Service == TEXT("Attend class"))
	{
		const ClassResult Result = Sim.AttendClass(0.7);
		OnNotice(Result.bValid
			? FString::Printf(TEXT("%d hours of %s.%s"), Result.HoursAdded, *Utf8(Result.EducationName),
				Result.bGraduated ? TEXT(" And that's the qualification finished.") : TEXT(""))
			: FString(TEXT("You're not enrolled in anything.")));
		return;
	}
	if (Service == TEXT("Career advice"))
	{
		FString Best;
		int BestLevel = -1;
		for (int32 Which = 0; Which < static_cast<int32>(Skill::Count); ++Which)
		{
			const int Level = Sim.Player.SkillLevel(static_cast<Skill>(Which));
			if (Level > BestLevel)
			{
				BestLevel = Level;
				Best = UTF8_TO_TCHAR(SkillName(static_cast<Skill>(Which)));
			}
		}
		OnNotice(FString::Printf(TEXT("Your best card is %s at level %d. The board has what it'll open."), *Best, BestLevel));
		return;
	}
	if (Service == TEXT("Account summary"))
	{
		OnNotice(FString::Printf(TEXT("%s in the account, %s in shares."), *Cash(Sim.Funds.Balance()),
			*Cash(Sim.Markets.PortfolioValue())));
		return;
	}
	if (Service == TEXT("Dyno"))
	{
		const OwnedVehicle* Car = Sim.ActiveVehicle();
		const VehicleSetup Setup = Car ? Sim.SetupFor(Car->Uid) : VehicleSetup();
		OnNotice(Car ? FString::Printf(TEXT("%.0f hp at the wheels, %.0f Nm, redline %.0f."),
			Setup.Engine.PeakPowerKw * 1.341, Setup.Engine.PeakTorqueNm, Setup.Engine.RedlineRpm)
			: FString(TEXT("Nothing on the rollers.")));
		return;
	}
	if (Service == TEXT("Track deliveries"))
	{
		if (Sim.Homes.Orders().empty())
		{
			OnNotice(TEXT("Nothing on its way."));
			return;
		}
		const FurnitureOrder& Latest = Sim.Homes.Orders().back();
		const std::int64_t Left = Latest.DeliverMinute - Sim.Clock.TotalMinutes();
		OnNotice(Left > 0
			? FString::Printf(TEXT("%d items, arriving in about %lld hours."), static_cast<int32>(Latest.Items.size()), Left / 60)
			: FString(TEXT("It's been delivered.")));
		return;
	}
	if (Service == TEXT("Recruit a lab contact"))
	{
		// Whoever is nearest: the scheme needs somebody who can reach a sample.
		const ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
		const FString Who = Player ? Player->Focus().MindId : FString();
		OnNotice(Who.IsEmpty()
			? FString(TEXT("Nobody here to ask. Find somebody who works in the building."))
			: Utf8(Sim.RecruitInsider(TCHAR_TO_UTF8(*Who), InsiderRole::LabTech).Message));
		return;
	}
	if (Service == TEXT("Restoration shop"))
	{
		const OwnedVehicle* Car = Sim.ActiveVehicle();
		if (!Car)
		{
			OnNotice(TEXT("Bring the car in and they'll quote it."));
			return;
		}
		const RestorationQuote Quote = Sim.QuoteRestore(Car->Uid, RestorationLevel::Driver, false);
		if (!Quote.bAllowed)
		{
			OnNotice(Utf8(Quote.Reason));
			return;
		}
		OnNotice(FString::Printf(TEXT("A driver-quality restoration: %s, %d days."), *Cash(Quote.ShopCost), Quote.ShopDays));
		OnNotice(Utf8(Sim.RestoreAtShop(Car->Uid, RestorationLevel::Driver, false).Message));
		return;
	}
	if (Service == TEXT("Arrange furniture"))
	{
		OnNotice(FString::Printf(TEXT("%d pieces in this place."), static_cast<int32>(Sim.Homes.Orders().size())));
		return;
	}
	if (Service == TEXT("Business consulting"))
	{
		// Somebody failing who would rather pay for help than go under.
		for (const Business& Company : Sim.Markets.Businesses())
		{
			if (Company.IsFailing() && !Company.bDissolved)
			{
				const TurnaroundOffer Offer = Sim.Markets.ProposeTurnaround(Company.Id);
				OnNotice(FString::Printf(TEXT("%s is in trouble. They'll pay %s, or %.0f%% of the business."),
					*Utf8(Company.Name), *Cash(Offer.Cash), Offer.Equity * 100.0));
				return;
			}
		}
		OnNotice(TEXT("Nobody's asking for help this week."));
		return;
	}
	if (Service == TEXT("Volunteer"))
	{
		OnNotice(Utf8(Sim.RunOutreach(OutreachKind::CommunityCleanup).Message));
		return;
	}
	if (Service == TEXT("Visit") || Service == TEXT("Visit a neighbor"))
	{
		OnNotice(Utf8(Sim.UseAmenity(Amenity::Leisure).Message));
		return;
	}
	OnNotice(FString::Printf(TEXT("%s: %s"), *Utf8(Building->Name), *Service));
}

void ADTGHud::OpenTalk(const FString& MindId)
{
	if (!Game || !Game->IsContentLoaded())
	{
		return;
	}
	TalkingTo = MindId;
	LastReply.Reset();
	GameState& Sim = Game->Sim();
	// Meeting somebody properly settles where they are from and who they know, which is what gives
	// them a town to have an opinion about and people to carry what you say to them.
	const FVector2D Where = Game->ToSim(ADTGCharacter::InterestPointFor(GetOwningPawn()));
	NpcMind& Mind = Sim.Meet(TCHAR_TO_UTF8(*MindId), Where.X, Where.Y);
	LastReply = Utf8(Sim.Npcs.Line(Mind, Sim.Random));
	Open(EDTGScreen::Talk);
}

void ADTGHud::RunTalkOption(int32 Index)
{
	GameState& Sim = Game->Sim();
	const std::string Who = TCHAR_TO_UTF8(*TalkingTo);
	const std::vector<DialogueOption> Choices = Sim.TalkOptions(Who);
	if (Index < 0 || Index >= static_cast<int32>(Choices.size()))
	{
		Close(); // "Walk away"
		return;
	}
	const std::string Chosen = Choices[static_cast<std::size_t>(Index)].Id;
	const TalkTurn Turn = Sim.Talk(Who, Chosen);
	LastReply = Utf8(Turn.Reply);
	// Paying for a tip or covering somebody's rent takes that line off the menu, which would otherwise
	// slide a different option under the cursor. Follow the one the player was on, or the row above it.
	const std::vector<DialogueOption> After = Sim.TalkOptions(Who);
	int32 Moved = FMath::Min(Index, FMath::Max(0, static_cast<int32>(After.size()) - 1));
	for (int32 Row = 0; Row < static_cast<int32>(After.size()); ++Row)
	{
		if (After[static_cast<std::size_t>(Row)].Id == Chosen)
		{
			Moved = Row;
			break;
		}
	}
	Selection = Moved;
	if (Turn.Cost > 0)
	{
		OnNotice(FString::Printf(TEXT("-%s"), *Cash(Turn.Cost)));
	}
	if (Turn.bEnded)
	{
		// They have walked off. The last thing they said is worth showing on the way out.
		OnNotice(LastReply);
		Close();
	}
}


// ================================================================================ Building services
//
// Everything a building offers is a list of things the simulation can actually do. ServiceRows builds
// that list and RunServiceOption acts on it by index, both from the same function, so the row the
// player is looking at and the row the game acts on cannot drift apart.

TArray<ADTGHud::FServiceRow> ADTGHud::ServiceRows() const
{
	TArray<FServiceRow> Rows;
	if (!Game || !Game->IsContentLoaded())
	{
		return Rows;
	}
	GameState& Sim = Game->Sim();
	const BuildingPlan* Building = Sim.City.FindBuilding(ContextId);
	const FString& What = ServiceName;

	// ---- Work and study
	if (What == TEXT("Job board"))
	{
		for (const CareerDef& Career : Sim.Careers.Careers())
		{
			std::string Why;
			if (!Sim.Careers.CanApply(Sim.Player, Career.Id, &Why))
			{
				continue;
			}
			bool bHave = false;
			for (const CareerProgress& Job : Sim.Jobs)
			{
				bHave = bHave || Job.CareerId == Career.Id;
			}
			if (bHave)
			{
				continue;
			}
			const Money Wage = Career.Levels.empty() ? Money(0) : Career.Levels.front().HourlyPay;
			Rows.Add({FString::Printf(TEXT("%s  --  %s an hour, %s"), *Utf8(Career.Name), *Cash(Wage),
				UTF8_TO_TCHAR(SkillName(Career.PrimarySkill))), Utf8(Career.Id), 0});
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("Nothing here you qualify for yet."), FString(), 0});
		}
		return Rows;
	}
	if (What == TEXT("Enroll"))
	{
		for (const EducationDef& Course : Sim.Careers.Educations())
		{
			std::string Why;
			if (Sim.Player.HasEducation(Course.Id) || !Sim.Careers.CanEnroll(Sim.Player, Course.Id, &Why))
			{
				continue;
			}
			Rows.Add({FString::Printf(TEXT("%s  --  %s, %d class hours"), *Utf8(Course.Name), *Cash(Course.Tuition),
				Course.StudyHours), Utf8(Course.Id), Course.Tuition});
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("Nothing left here to enroll in."), FString(), 0});
		}
		return Rows;
	}

	// ---- Cars
	if (What == TEXT("Browse every model") || What == TEXT("Test drive"))
	{
		// What this showroom's own brands sell new. The whole catalog would be two thousand rows.
		const std::string ShowroomId = Building ? Building->Param : std::string();
		for (const Brand* Make : Sim.BrandsAtShowroom(ShowroomId))
		{
			if (!Make)
			{
				continue;
			}
			for (const VehicleModel& Model : Sim.Vehicles.Models())
			{
				if (Model.BrandId != Make->Id || !Model.IsCurrent() || Model.bEventOnly)
				{
					continue;
				}
				const Money Price = Sim.NewCarPrice(Model);
				Rows.Add({FString::Printf(TEXT("%s  --  %s"), *Utf8(Model.DisplayName), *Cash(Price)), Utf8(Model.Id), Price});
			}
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("The floor is empty this week."), FString(), 0});
		}
		return Rows;
	}
	if (What == TEXT("Browse used cars") || What == TEXT("Browse classics") || What == TEXT("Used car lot"))
	{
		for (const UsedListing& Listing : Sim.UsedCars.Listings())
		{
			const VehicleModel* Model = Sim.Vehicles.FindModel(Listing.ModelId);
			Rows.Add({FString::Printf(TEXT("%s  --  %s, %.0fk km, condition %.0f%%"),
				Model ? *Utf8(Model->DisplayName) : *Utf8(Listing.ModelId), *Cash(Listing.AskingPrice),
				Listing.MileageKm / 1000.0, Listing.Condition * 100.0), Utf8(Listing.Id), Listing.AskingPrice});
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("Nothing on the lot right now."), FString(), 0});
		}
		return Rows;
	}
	if (What == TEXT("Sell a car"))
	{
		for (const OwnedVehicle& Car : Sim.Garage)
		{
			const VehicleModel* Model = Sim.Vehicles.FindModel(Car.ModelId);
			Rows.Add({FString::Printf(TEXT("%s  --  they'll give you %s"),
				Model ? *Utf8(Model->DisplayName) : *Utf8(Car.ModelId),
				Model ? *Cash(TradeInOffer(*Model, Car)) : TEXT("an offer")), Utf8(Car.Uid), 0});
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("You have nothing to sell."), FString(), 0});
		}
		return Rows;
	}
	if (What == TEXT("Parts and tuning") || What == TEXT("DIY garage"))
	{
		const OwnedVehicle* Car = Sim.ActiveVehicle();
		const VehicleModel* Model = Car ? Sim.Vehicles.FindModel(Car->ModelId) : nullptr;
		if (!Car || !Model)
		{
			Rows.Add({TEXT("Bring a car and they'll fit something to it."), FString(), 0});
			return Rows;
		}
		for (const PartDef& Part : Sim.Parts.All())
		{
			if (Part.bEvOnly != Model->bElectric && (Part.bEvOnly || Part.bIceOnly))
			{
				continue;
			}
			const Money Price = PartPrice(Part, *Model);
			Rows.Add({FString::Printf(TEXT("%s %s  --  %s"), *Utf8(Part.Maker), *Utf8(Part.Name), *Cash(Price)),
				Utf8(Part.Id), Price});
		}
		return Rows;
	}
	if (What == TEXT("Paint shop"))
	{
		static const TCHAR* const Names[] = {TEXT("Black"), TEXT("White"), TEXT("Red"), TEXT("Blue"), TEXT("Silver"), TEXT("Green")};
		static const uint32 Colours[] = {0x101014u, 0xe8e8e8u, 0xa8171cu, 0x1b3f8bu, 0xb4b8bcu, 0x1d5c36u};
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Names)); ++Index)
		{
			Rows.Add({Names[Index], FString::Printf(TEXT("%u"), Colours[Index]), Dollars(400)});
		}
		return Rows;
	}

	// ---- Money
	if (What == TEXT("Stock exchange"))
	{
		for (const Stock& Listed : Sim.Markets.Stocks())
		{
			const int64 Held = static_cast<int64>(Sim.Markets.SharesHeld(Listed.Ticker));
			const double Move = Listed.PreviousClose > 0.0
				? 100.0 * (Listed.Price - Listed.PreviousClose) / Listed.PreviousClose : 0.0;
			Rows.Add({FString::Printf(TEXT("%s  $%.2f  %+.1f%%%s"), *Utf8(Listed.Ticker), Listed.Price, Move,
				Held > 0 ? *FString::Printf(TEXT("   you hold %lld"), Held) : TEXT("")), Utf8(Listed.Ticker), 0});
		}
		return Rows;
	}
	if (What == TEXT("Buy a business"))
	{
		for (const Business& Company : Sim.Markets.Businesses())
		{
			if (Company.bDissolved || Company.PlayerEquity > 0.99)
			{
				continue;
			}
			const Money Ask = Sim.Markets.AskingPrice(Company.Id);
			Rows.Add({FString::Printf(TEXT("%s  --  %s, %s"), *Utf8(Company.Name), *Utf8(Company.Sector), *Cash(Ask)),
				Utf8(Company.Id), Ask});
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("Nothing for sale today."), FString(), 0});
		}
		return Rows;
	}
	if (What == TEXT("Browse homes"))
	{
		const FVector2D Here = Game->ToSim(ADTGCharacter::InterestPointFor(GetOwningPawn()));
		for (const BuildingPlan* Home : Sim.City.BuildingsNear(Here.X, Here.Y, 3000.0))
		{
			if (!Home || !IsResidential(Home->Role) || Sim.Homes.OwnsProperty(Sim.PropertyIdFor(Home->Id)))
			{
				continue;
			}
			Rows.Add({FString::Printf(TEXT("%s  --  %s"), *Utf8(Home->Address.empty() ? Home->Name : Home->Address),
				*Cash(Home->Price)), FString::FromInt(Home->Id), Home->Price});
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("Nothing listed nearby."), FString(), 0});
		}
		return Rows;
	}
	if (What == TEXT("Shop furniture"))
	{
		for (const FurnitureItem& Item : Sim.Homes.Items())
		{
			Rows.Add({FString::Printf(TEXT("%s  --  %s, %s"), *Utf8(Item.Name), *Utf8(Item.Category), *Cash(Item.Price)),
				Utf8(Item.Id), Item.Price});
		}
		return Rows;
	}

	// ---- Work of your own
	if (What == TEXT("Hacking contracts"))
	{
		for (const HackContract& Job : Sim.HackBoard)
		{
			Rows.Add({FString::Printf(TEXT("%s  --  %s for %s, tier %d"), *Utf8(Job.Title), *Utf8(Job.Client),
				*Cash(Job.Payout), Job.Tier), Utf8(Job.Id), 0});
		}
		if (Rows.IsEmpty())
		{
			Rows.Add({TEXT("The board is empty. Try tomorrow."), FString(), 0});
		}
		return Rows;
	}
	if (What == TEXT("Hacking tools"))
	{
		Rows.Add({TEXT("Probe kit"), TEXT("0"), 0});
		Rows.Add({TEXT("Ghost relay"), TEXT("1"), 0});
		Rows.Add({TEXT("Pattern engine"), TEXT("2"), 0});
		return Rows;
	}
	if (What == TEXT("Create IP"))
	{
		Rows.Add({TEXT("Write a song"), TEXT("0"), 0});
		Rows.Add({TEXT("Write a book"), TEXT("1"), 0});
		Rows.Add({TEXT("Make a film"), TEXT("2"), 0});
		Rows.Add({TEXT("Build an app"), TEXT("3"), 0});
		Rows.Add({TEXT("Design an invention"), TEXT("4"), 0});
		return Rows;
	}
	if (What == TEXT("Research a medicine"))
	{
		Rows.Add({TEXT("A general treatment"), TEXT("general"), 0});
		if (Sim.Outbreak.IsActive())
		{
			Rows.Add({FString::Printf(TEXT("Target %s -- fast-tracked while the outbreak runs"),
				*Utf8(Sim.Outbreak.Strain().Name)), TEXT("outbreak"), 0});
		}
		return Rows;
	}

	// ---- Office
	if (What == TEXT("Run for office"))
	{
		for (int32 Which = 1; Which < static_cast<int32>(Office::Count); ++Which)
		{
			const OfficeInfo& Info = GetOfficeInfo(static_cast<Office>(Which));
			Rows.Add({FString::Printf(TEXT("%s  --  filing fee %s, %d followers needed"), UTF8_TO_TCHAR(Info.Title),
				*Cash(Info.FilingFee), Info.FollowersNeeded), FString::FromInt(Which), Info.FilingFee});
		}
		return Rows;
	}
	if (What == TEXT("Legislature"))
	{
		for (const LawDef& Law : Sim.LawBook)
		{
			Rows.Add({FString::Printf(TEXT("%s%s"), *Utf8(Law.Name),
				Sim.Politics.IsEnacted(Law.Id) ? TEXT("   (in force)") : TEXT("   -- lobby for it")), Utf8(Law.Id), 0});
		}
		return Rows;
	}

	// ---- Faith
	if (What == TEXT("Ministry office"))
	{
		if (Sim.PlayerMinistry.Name.empty())
		{
			Rows.Add({TEXT("Found a ministry here"), TEXT("found"), 0});
			return Rows;
		}
		for (int32 Kind = 0; Kind < static_cast<int32>(OutreachKind::Count); ++Kind)
		{
			const OutreachInfo& Info = GetOutreachInfo(static_cast<OutreachKind>(Kind));
			Rows.Add({FString::Printf(TEXT("%s  --  %s, %d hours"), UTF8_TO_TCHAR(Info.Name), *Cash(Info.Cost), Info.Hours),
				FString::Printf(TEXT("outreach:%d"), Kind), Info.Cost});
		}
		Rows.Add({TEXT("Hold a telethon"), TEXT("telethon"), 0});
		Rows.Add({TEXT("Open the books"), TEXT("accountability"), 0});
		return Rows;
	}
	return Rows;
}

void ADTGHud::RunServiceOption(int32 Index)
{
	GameState& Sim = Game->Sim();
	const TArray<FServiceRow> Rows = ServiceRows();
	if (Index < 0 || Index >= Rows.Num())
	{
		// The row past the end is "Back".
		Open(EDTGScreen::Building, ContextId);
		return;
	}
	const FServiceRow& Row = Rows[Index];
	if (Row.Key.IsEmpty())
	{
		return; // an explanatory line, not a choice
	}
	const std::string Key = TCHAR_TO_UTF8(*Row.Key);
	const FString& What = ServiceName;
	InvalidateOptions();

	if (What == TEXT("Job board"))
	{
		OnNotice(Utf8(Sim.ApplyForCareer(Key).Message));
		return;
	}
	if (What == TEXT("Enroll"))
	{
		OnNotice(Utf8(Sim.Enroll(Key).Message));
		return;
	}
	if (What == TEXT("Browse every model"))
	{
		OnNotice(Utf8(Sim.BuyVehicle(Key).Message));
		return;
	}
	if (What == TEXT("Test drive"))
	{
		// A showroom car brought round for a lap of the block: configured from the catalog, not owned,
		// so it is not in the garage and cannot be driven away for good.
		if (ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn()))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADTGVehicle* Demo = GetWorld()->SpawnActor<ADTGVehicle>(ADTGVehicle::StaticClass(), FTransform::Identity, Params))
			{
				Demo->ConfigureFromModel(Row.Key);
				const FVector At = Player->GetActorLocation() + Player->GetActorForwardVector() * 450.0f;
				Demo->PlaceAt(At, Player->GetActorRotation().Yaw);
				OnNotice(FString::Printf(TEXT("They've brought the %s round. It's outside."), *Row.Label));
				Close();
				return;
			}
		}
		OnNotice(TEXT("No room out front for a demonstrator."));
		return;
	}
	if (What == TEXT("Browse used cars") || What == TEXT("Browse classics") || What == TEXT("Used car lot"))
	{
		// Offer nine tenths of the asking price: the simulation decides whether they take it.
		const HaggleResult Haggled = Sim.HaggleUsed(Key, (Row.Cost * 9) / 10);
		OnNotice(Utf8(Haggled.Reply));
		// A counter is a price they named, so taking it is the sale; walking away is not.
		if (Haggled.Outcome != HaggleOutcome::WalkedAway)
		{
			OnNotice(Utf8(Sim.BuyUsed(Key, Haggled.Price).Message));
		}
		return;
	}
	if (What == TEXT("Sell a car"))
	{
		OnNotice(Utf8(Sim.SellVehicle(Key).Message));
		return;
	}
	if (What == TEXT("Parts and tuning") || What == TEXT("DIY garage"))
	{
		const OwnedVehicle* Car = Sim.ActiveVehicle();
		OnNotice(Car ? Utf8(Sim.InstallPart(Car->Uid, Key).Message) : FString(TEXT("No car here to work on.")));
		return;
	}
	if (What == TEXT("Paint shop"))
	{
		const OwnedVehicle* Car = Sim.ActiveVehicle();
		if (!Car)
		{
			OnNotice(TEXT("Bring the car round and they'll shoot it."));
			return;
		}
		OnNotice(Utf8(Sim.Paint(Car->Uid, static_cast<uint32>(FCString::Strtoui64(*Row.Key, nullptr, 10)), 0).Message));
		return;
	}
	if (What == TEXT("Stock exchange"))
	{
		// Ten shares a press: enough to build a position without a number entry box.
		OnNotice(Utf8(Sim.BuyShares(Key, 10).Message));
		return;
	}
	if (What == TEXT("Buy a business"))
	{
		OnNotice(Utf8(Sim.BuyBusiness(Key).Message));
		return;
	}
	if (What == TEXT("Browse homes"))
	{
		OnNotice(Utf8(Sim.BuyHome(FCString::Atoi(*Row.Key)).Message));
		return;
	}
	if (What == TEXT("Shop furniture"))
	{
		const int32 HomeId = Sim.Homes.OwnedProperties().empty() ? -1 : ContextId;
		OnNotice(Utf8(Sim.OrderFurniture({Key}, HomeId, DeliverySpeed::Standard).Message));
		return;
	}
	if (What == TEXT("Hacking contracts"))
	{
		// The simulation says what the job needs; whether the player's tools and skill are up to it
		// decides whether it lands.
		const HackContract* Contract = nullptr;
		for (const HackContract& Job : Sim.HackBoard)
		{
			if (Job.Id == Key)
			{
				Contract = &Job;
				break;
			}
		}
		if (!Contract)
		{
			OnNotice(TEXT("That one's been taken."));
			return;
		}
		// The job's own difficulty against what the player brought to it: more layers and a longer
		// code are what the toolkit and the skill level have to cover.
		const HackConfig Setup = Sim.ConfigFor(*Contract);
		const int32 Needed = Setup.Layers * Setup.CodeLength;
		const int32 Brought = (1 + Sim.Player.SkillLevel(Skill::Tech)) * (2 + Setup.PreRevealed);
		OnNotice(Utf8(Sim.CompleteHack(Key, Brought >= Needed).Message));
		return;
	}
	if (What == TEXT("Hacking tools"))
	{
		OnNotice(Utf8(Sim.BuyHackTool(FCString::Atoi(*Row.Key)).Message));
		return;
	}
	if (What == TEXT("Create IP"))
	{
		static const WorkKind Kinds[] = {WorkKind::Song, WorkKind::Book, WorkKind::Film, WorkKind::App, WorkKind::Invention};
		const int32 Which = FMath::Clamp(FCString::Atoi(*Row.Key), 0, static_cast<int32>(UE_ARRAY_COUNT(Kinds)) - 1);
		const CreativeWork& Made = Sim.CreateWork(Kinds[Which], TCHAR_TO_UTF8(*FString::Printf(TEXT("Untitled %s"), *Row.Label)),
			4, 0.7 + 0.03 * Sim.Player.SkillLevel(Skill::Creativity));
		OnNotice(FString::Printf(TEXT("Four hours on %s. Quality %.0f%%."), *Utf8(Made.Title), Made.Quality * 100.0));
		return;
	}
	if (What == TEXT("Research a medicine"))
	{
		const bool bTarget = Row.Key == TEXT("outbreak");
		OnNotice(Utf8(Sim.DevelopMedicine(bTarget ? "Targeted antiviral" : "General treatment", 6,
			0.6 + 0.05 * Sim.Player.SkillLevel(Skill::Medicine), bTarget).Message));
		return;
	}
	if (What == TEXT("Run for office"))
	{
		OnNotice(Utf8(Sim.RunForOffice(static_cast<Office>(FCString::Atoi(*Row.Key))).Message));
		return;
	}
	if (What == TEXT("Legislature"))
	{
		const LawVote Vote = Sim.LobbyForLaw(Key, Dollars(2500));
		OnNotice(Utf8(Vote.Message));
		return;
	}
	if (What == TEXT("Ministry office"))
	{
		if (Row.Key == TEXT("found"))
		{
			OnNotice(Utf8(Sim.FoundMinistry(TCHAR_TO_UTF8(*FString::Printf(TEXT("%s Fellowship"), *Utf8(Sim.PlayerName)))).Message));
			return;
		}
		if (Row.Key == TEXT("telethon"))
		{
			OnNotice(Utf8(Sim.Telethon().Message));
			return;
		}
		if (Row.Key == TEXT("accountability"))
		{
			OnNotice(Utf8(Sim.Accountability().Message));
			return;
		}
		if (Row.Key.StartsWith(TEXT("outreach:")))
		{
			const int32 Kind = FCString::Atoi(*Row.Key.RightChop(9));
			OnNotice(Utf8(Sim.RunOutreach(static_cast<OutreachKind>(Kind)).Message));
			return;
		}
	}
}

void ADTGHud::RunTitleOption(int32 Index)
{
	switch (Index)
	{
	case 0: Open(EDTGScreen::Events); break;
	case 1: Close(); break;
	case 2:
	{
		Game->StartNewGame(TEXT("Newcomer"));
		// The simulation has a new garage, new money and a new home, so the old world's cars must go.
		RestartWorld(/*bRebuildTerrain*/ false);
		const StateCity* Start = Game->Sim().Map.StartCity();
		OnNotice(FString::Printf(TEXT("A new life in %s."), Start ? *Utf8(Start->Name) : TEXT("town")));
		Close();
		break;
	}
	case 3: OnNotice(Game->SaveGame() ? TEXT("Saved.") : TEXT("Couldn't write the save file.")); break;
	case 4: OnNotice(Game->LoadGame() ? TEXT("Loaded.") : TEXT("No save to load.")); break;
	default:
	{
		// Switching states rebuilds the world and starts a new life in it.
		// Copied, not referenced: switching state is entitled to rebuild the list this came from.
		const TArray<FString> States = Game->AvailableStates();
		const int32 Which = Index - 5;
		if (!States.IsValidIndex(Which) || States[Which] == Game->StateFile())
		{
			break;
		}
		if (!Game->SwitchState(States[Which], TEXT("Newcomer")))
		{
			OnNotice(TEXT("That state wouldn't load."));
			break;
		}
		// The simulation now holds a different world: the old one has to be taken down and rebuilt.
		RestartWorld(/*bRebuildTerrain*/ true);
		OnNotice(TEXT("A new life, somewhere else entirely."));
		Close();
		break;
	}
	}
}

void ADTGHud::RunEventOption(int32 Index)
{
	const int32 Kind = Index / 3;
	const int32 Tier = 1 + Index % 3;
	if (Kind < 0 || Kind >= static_cast<int32>(EventKind::Count))
	{
		return;
	}
	ADTGEventDirector* Director = Cast<ADTGEventDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGEventDirector::StaticClass()));
	if (Director && Director->StartEvent(static_cast<EventKind>(Kind), Tier))
	{
		Close();
	}
}

void ADTGHud::RestartWorld(bool bRebuildTerrain)
{
	// The directors own what they spawned, so they clear it themselves rather than leaving this to the
	// garbage collector to notice.
	if (ADTGTrafficDirector* Traffic = Cast<ADTGTrafficDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGTrafficDirector::StaticClass())))
	{
		Traffic->Reset();
	}
	if (ADTGEventDirector* Events = Cast<ADTGEventDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGEventDirector::StaticClass())))
	{
		Events->Reset();
	}
	// The player has to get out before their car is destroyed. ADTGCharacter::Driving is a UPROPERTY,
	// so it keeps a destroyed actor alive and non-null for good -- the HUD would go on drawing frozen
	// instruments for a car that no longer exists and never show the on-foot view again.
	ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
	if (Player && Player->DrivingNow())
	{
		Player->ToggleVehicle();
	}
	// Anything else still standing: the old cars, a stray pedestrian.
	for (TActorIterator<ADTGVehicle> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsPlayerControlled())
		{
			It->Destroy();
		}
	}
	if (bRebuildTerrain)
	{
		if (ADTGWorldBuilder* Builder = Cast<ADTGWorldBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGWorldBuilder::StaticClass())))
		{
			Builder->Rebuild();
		}
	}
	const CityPlan& Town = Game->Sim().City;
	const double GroundZ = Game->Sim().Map.HeightAt(Town.SpawnX, Town.SpawnY);
	const FVector Start = Game->ToUnreal(Town.SpawnX, Town.SpawnY, GroundZ + 1.0);
	if (ADTGWorldBuilder* Builder = Cast<ADTGWorldBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGWorldBuilder::StaticClass())))
	{
		// Ground under the new start before anybody stands on it.
		Builder->EnsureGroundAt(Start);
	}
	if (Player)
	{
		Player->LeaveBuilding();
		Player->SetActorLocation(Start);
	}
	// The simulation gives a new game a car; the world has to be given one too, or the player starts a
	// fresh game owning a car that exists only in the garage menu.
	if (ADTGGameMode* Mode = Cast<ADTGGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		Mode->PlaceOwnedCar(Game->Sim().Garage.empty() ? FString() : Utf8(Game->Sim().Garage.front().Uid), Start);
	}
}

void ADTGHud::RunCourtOption(int32 Index)
{
	GameState& Sim = Game->Sim();
	switch (Index)
	{
	case 0: OnNotice(Utf8(Sim.HireLawyer().Message)); break;
	case 1: Sim.TakePlea(); Close(); break;
	case 2: Sim.GoToTrial(); Close(); break;
	case 3: Sim.CoDefendantChoice(/*bTestify*/ true); Close(); break;
	case 4: Sim.CoDefendantChoice(/*bTestify*/ false); Close(); break;
	default: break;
	}
}

void ADTGHud::RunJailOption(int32 Index)
{
	GameState& Sim = Game->Sim();
	const int32 Plans = static_cast<int32>(EscapePlan::Count) - 1;
	if (Index >= 0 && Index <= 3)
	{
		const JailDayResult Result = Sim.ServeJailDay(static_cast<JailActivity>(Index));
		OnNotice(Utf8(Result.Summary));
		if (Result.bReleased)
		{
			Close();
		}
		return;
	}
	if (Index == 4)
	{
		OnNotice(Utf8(Sim.RequestParole().Message));
		return;
	}
	if (Index == 5)
	{
		OnNotice(Utf8(Sim.FileAppeal().Message));
		return;
	}
	if (Index >= 6 && Index < 6 + Plans)
	{
		const EscapePlan Plan = static_cast<EscapePlan>(Index - 5); // slot 6 is plan 1
		OnNotice(Utf8(Sim.PlanEscape(Plan).Message));
		return;
	}
	const int32 Extra = Index - (6 + Plans);
	if (Extra == 0)
	{
		OnNotice(Utf8(Sim.PrepareEscape().Summary));
	}
	else if (Extra == 1)
	{
		const EscapeResult Result = Sim.AttemptEscape();
		OnNotice(Utf8(Result.Summary));
		if (Result.bEscaped)
		{
			Close();
		}
	}
	else if (Extra == 2)
	{
		OnNotice(Utf8(Sim.AbandonEscape().Message));
	}
}
