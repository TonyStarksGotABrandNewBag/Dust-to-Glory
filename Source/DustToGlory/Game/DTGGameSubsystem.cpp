#include "Game/DTGGameSubsystem.h"

#include "DustToGlory.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UDTGGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bContentLoaded = LoadContent(CurrentState);
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

FString UDTGGameSubsystem::ModsDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Mods"));
}

std::string UDTGGameSubsystem::ReadData(const FString& Name)
{
	// A mod is a folder of data files. Several can be installed at once; later folders win, so a mod that
	// only replaces vehicles.json leaves everything else alone.
	FString Path = FPaths::Combine(DataDir(), Name);
	for (const FString& Mod : Mods)
	{
		const FString Candidate = FPaths::Combine(ModsDir(), Mod, Name);
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Candidate))
		{
			UE_LOG(LogDTG, Log, TEXT("%s comes from the mod '%s'"), *Name, *Mod);
			Path = Candidate;
		}
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogDTG, Error, TEXT("Missing content file %s"), *Name);
		return std::string();
	}
	return std::string(TCHAR_TO_UTF8(*Text));
}

const TArray<FString>& UDTGGameSubsystem::AvailableStates() const
{
	// Read from disk once: the front end asks for this every frame it is open.
	if (!StateList.IsEmpty())
	{
		return StateList;
	}
	TArray<FString>& Found = StateList;
	IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
	auto Collect = [&Found, &Platform](const FString& Dir)
	{
		TArray<FString> Names;
		Platform.FindFiles(Names, *Dir, TEXT(".json"));
		for (const FString& Full : Names)
		{
			const FString Leaf = FPaths::GetCleanFilename(Full);
			if (Leaf.StartsWith(TEXT("state")) && !Found.Contains(Leaf))
			{
				Found.Add(Leaf);
			}
		}
	};
	Collect(DataDir());
	for (const FString& Mod : Mods)
	{
		Collect(FPaths::Combine(ModsDir(), Mod));
	}
	Found.Sort();
	return Found;
}

bool UDTGGameSubsystem::SwitchState(const FString& FileName, const FString& PlayerName)
{
	const FString Previous = CurrentState;
	if (!LoadContent(FileName))
	{
		// Loading mutates the simulation in place, so put the old state back rather than leave half of one.
		bContentLoaded = LoadContent(Previous);
		return false;
	}
	bContentLoaded = true;
	CurrentState = FileName;
	StateList.Reset();
	return StartNewGame(PlayerName);
}

bool UDTGGameSubsystem::LoadContent(const FString& StateFileName)
{
	// Note which mods are installed before reading anything, so every file sees the same set.
	Mods.Reset();
	IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
	if (Platform.DirectoryExists(*ModsDir()))
	{
		TArray<FString> Folders;
		Platform.IterateDirectory(*ModsDir(), [&Folders](const TCHAR* Path, bool bIsDirectory)
		{
			if (bIsDirectory)
			{
				Folders.Add(FPaths::GetCleanFilename(FString(Path)));
			}
			return true;
		});
		Folders.Sort();
		Mods = Folders;
	}

	dtg::sim::ContentFiles Files;
	Files.Economy = ReadData(TEXT("economy.json"));
	Files.Vehicles = ReadData(TEXT("vehicles.json"));
	Files.Parts = ReadData(TEXT("parts.json"));
	Files.Careers = ReadData(TEXT("careers.json"));
	Files.Powers = ReadData(TEXT("powers.json"));
	Files.Furniture = ReadData(TEXT("furniture.json"));
	Files.Companies = ReadData(TEXT("companies.json"));
	Files.Laws = ReadData(TEXT("laws.json"));
	Files.State = ReadData(StateFileName);

	dtg::sim::ContentReport ContentReport;
	const bool bOk = State.LoadContent(Files, ContentReport);
	for (const std::string& Error : ContentReport.Errors)
	{
		UE_LOG(LogDTG, Error, TEXT("Content: %s"), UTF8_TO_TCHAR(Error.c_str()));
	}
	if (bOk)
	{
		UE_LOG(LogDTG, Log, TEXT("%s: %d brands, %d nameplates, %d models, %d buildings across %d towns%s"),
			*StateFileName, ContentReport.Brands, ContentReport.Nameplates, ContentReport.Models,
			static_cast<int32>(State.City.Buildings.size()), static_cast<int32>(State.City.Cities.size()),
			Mods.Num() > 0 ? *FString::Printf(TEXT(" (mods: %s)"), *FString::Join(Mods, TEXT(", "))) : TEXT(""));
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
