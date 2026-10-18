#include "Game/DTGGameSubsystem.h"

#include "DustToGlory.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UDTGGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bContentLoaded = LoadContent();
}

void UDTGGameSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

FString UDTGGameSubsystem::DataDir()
{
	// Packaged builds stage Content/Data next to the executable (see DefaultGame.ini); in the editor it
	// sits under the project.
	const FString Staged = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"));
	if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Staged))
	{
		return Staged;
	}
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
}

bool UDTGGameSubsystem::LoadContent()
{
	const FString Dir = DataDir();
	auto Read = [&Dir](const TCHAR* Name)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, Name)))
		{
			UE_LOG(LogDTG, Error, TEXT("Missing content file %s in %s"), Name, *Dir);
			return std::string();
		}
		return std::string(TCHAR_TO_UTF8(*Text));
	};

	dtg::sim::ContentFiles Files;
	Files.Economy = Read(TEXT("economy.json"));
	Files.Vehicles = Read(TEXT("vehicles.json"));
	Files.Parts = Read(TEXT("parts.json"));
	Files.Careers = Read(TEXT("careers.json"));
	Files.Powers = Read(TEXT("powers.json"));
	Files.Furniture = Read(TEXT("furniture.json"));
	Files.Companies = Read(TEXT("companies.json"));
	Files.Laws = Read(TEXT("laws.json"));
	Files.State = Read(TEXT("state.json"));

	dtg::sim::ContentReport ContentReport;
	const bool bOk = State.LoadContent(Files, ContentReport);
	for (const std::string& Error : ContentReport.Errors)
	{
		UE_LOG(LogDTG, Error, TEXT("Content: %s"), UTF8_TO_TCHAR(Error.c_str()));
	}
	if (bOk)
	{
		UE_LOG(LogDTG, Log, TEXT("Loaded %d brands, %d nameplates, %d models, %d buildings across %d towns"),
			ContentReport.Brands, ContentReport.Nameplates, ContentReport.Models,
			static_cast<int32>(State.City.Buildings.size()), static_cast<int32>(State.City.Cities.size()));
	}
	return bOk;
}

bool UDTGGameSubsystem::StartNewGame(const FString& PlayerName, int64 Seed)
{
	if (!bContentLoaded)
	{
		return false;
	}
	const uint64 UseSeed = Seed != 0 ? static_cast<uint64>(Seed) : static_cast<uint64>(FDateTime::UtcNow().GetTicks());
	State.NewGame(UseSeed, TCHAR_TO_UTF8(*PlayerName));
	Carry = 0.0;
	return true;
}

FString UDTGGameSubsystem::SavePath(const FString& SlotName)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), SlotName + TEXT(".json"));
}

bool UDTGGameSubsystem::SaveGame(const FString& SlotName)
{
	const std::string Text = State.SaveToJson();
	return FFileHelper::SaveStringToFile(FString(UTF8_TO_TCHAR(Text.c_str())), *SavePath(SlotName));
}

bool UDTGGameSubsystem::LoadGame(const FString& SlotName)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SavePath(SlotName)))
	{
		return false;
	}
	std::string Error;
	if (!State.LoadFromJson(TCHAR_TO_UTF8(*Text), &Error))
	{
		UE_LOG(LogDTG, Error, TEXT("Save file rejected: %s"), UTF8_TO_TCHAR(Error.c_str()));
		return false;
	}
	return true;
}

void UDTGGameSubsystem::TickSim(float DeltaSeconds)
{
	if (!bContentLoaded)
	{
		return;
	}
	Report = State.Advance(static_cast<double>(DeltaSeconds) * FMath::Max(0.0f, TimeScale));
	for (const std::string& Text : State.TakeNotifications())
	{
		const FString Line = UTF8_TO_TCHAR(Text.c_str());
		UE_LOG(LogDTG, Verbose, TEXT("%s"), *Line);
		OnNotice.Broadcast(Line);
	}
}
