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
#include "Game/DTGInterior.h"
#include "Game/DTGWorldBuilder.h"
#include "Game/DTGPlayerController.h"
#include "Game/DTGVehicle.h"

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
		// Which town the player is actually standing in, not where they started.
		const ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn());
		const FVector At = Player ? Player->GetActorLocation() : FVector::ZeroVector;
		const FVector2D Where = Game->ToSim(At);
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
	const FVector At = Player ? (Player->DrivingNow() ? Player->DrivingNow()->GetActorLocation() : Player->GetActorLocation()) : FVector::ZeroVector;
	const FVector2D Here = Game->ToSim(At);

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
	for (const RoadPlan& Road : Sim.City.Roads)
	{
		FVector2D Point;
		if (Project(Road.Area.CenterX(), Road.Area.CenterY(), Point))
		{
			Panel(Point.X - 1.0f, Point.Y - 1.0f, 3.0f, 3.0f, FLinearColor(0.45f, 0.45f, 0.5f, 0.9f));
		}
	}
	for (const BuildingPlan& Building : Sim.City.Buildings)
	{
		FVector2D Point;
		if (Project(Building.DoorX, Building.DoorY, Point))
		{
			const FLinearColor Colour = IsResidential(Building.Role) ? FLinearColor(0.4f, 0.6f, 0.9f, 0.9f) : FLinearColor(0.8f, 0.75f, 0.6f, 0.9f);
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
	const int32 Count = BuildOptions().Num();
	if (Count > 0)
	{
		Selection = (Selection + Delta + Count) % Count;
	}
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
		if (const NpcMind* Mind = Sim.Npcs.Find(TCHAR_TO_UTF8(*TalkingTo)))
		{
			for (const DialogueOption& Option : Sim.Npcs.Options(*Mind))
			{
				Options.Add(Utf8(Option.Label));
			}
		}
		Options.Add(TEXT("Walk away"));
		break;
	}
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
	const TArray<FString> Options = BuildOptions();
	const float W = 720.0f;
	const float H = FMath::Min(Canvas->SizeY - 120.0f, 120.0f + Options.Num() * 30.0f);
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
		}
		break;
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
	float Row = Y + 90.0f;
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const bool bSelected = Index == Selection;
		if (bSelected)
		{
			Panel(X + 16.0f, Row - 4.0f, W - 32.0f, 28.0f, FLinearColor(0.16f, 0.15f, 0.1f, 0.9f));
		}
		Text(Options[Index], X + 28.0f, Row, bSelected ? FLinearColor::White : Dim);
		Row += 30.0f;
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
	const int32 Count = BuildOptions().Num();
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
			// Picking a car makes it the one you drive.
			Game->Sim().ActiveVehicleUid = Cars[static_cast<std::size_t>(Selection)].Uid;
			OnNotice(TEXT("That's the one you'll be driving."));
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
	// The service names come from the simulation, so this is a straight hand-off to it.
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
	OnNotice(FString::Printf(TEXT("%s: %s"), *Utf8(Building->Name), *Service));
}

void ADTGHud::OpenTalk(const FString& MindId)
{
	TalkingTo = MindId;
	LastReply.Reset();
	GameState& Sim = Game->Sim();
	// Opening a conversation is itself something they remember.
	NpcMind& Mind = Sim.Npcs.GetOrCreate(TCHAR_TO_UTF8(*MindId));
	LastReply = Utf8(Sim.Npcs.Line(Mind, Sim.Random));
	Open(EDTGScreen::Talk);
}

void ADTGHud::RunTalkOption(int32 Index)
{
	GameState& Sim = Game->Sim();
	NpcMind* Mind = Sim.Npcs.Find(TCHAR_TO_UTF8(*TalkingTo));
	if (!Mind)
	{
		Close();
		return;
	}
	const std::vector<DialogueOption> Choices = Sim.Npcs.Options(*Mind);
	if (Index < 0 || Index >= static_cast<int32>(Choices.size()))
	{
		Close(); // "Walk away"
		return;
	}
	LastReply = Utf8(Sim.Npcs.Respond(*Mind, Choices[static_cast<std::size_t>(Index)].Id, Sim.Clock.TotalMinutes(), Sim.Random, Sim.Careers));
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
		const TArray<FString>& States = Game->AvailableStates();
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
		// The simulation now holds a different world, so everything standing in the old one has to go.
		if (ADTGWorldBuilder* Builder = Cast<ADTGWorldBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGWorldBuilder::StaticClass())))
		{
			Builder->Rebuild();
		}
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
		if (ADTGCharacter* Player = Cast<ADTGCharacter>(GetOwningPawn()))
		{
			const CityPlan& World = Game->Sim().City;
			const double GroundZ = Game->Sim().Map.HeightAt(World.SpawnX, World.SpawnY);
			Player->LeaveBuilding();
			Player->SetActorLocation(Game->ToUnreal(World.SpawnX, World.SpawnY, GroundZ + 1.0));
		}
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
