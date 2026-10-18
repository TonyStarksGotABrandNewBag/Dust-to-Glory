#pragma once
#include "CoreMinimal.h"

class IPlatformFile
{
public:
	bool FileExists(const TCHAR* Path) { (void)Path; return false; }
	bool DirectoryExists(const TCHAR* Path) { (void)Path; return false; }
	bool CreateDirectoryTree(const TCHAR* Path) { (void)Path; return true; }
	bool DeleteFile(const TCHAR* Path) { (void)Path; return true; }
	int64 FileSize(const TCHAR* Path) { (void)Path; return 0; }
	bool FindFiles(TArray<FString>& OutFound, const TCHAR* Directory, const TCHAR* Extension)
	{
		(void)OutFound; (void)Directory; (void)Extension;
		return true;
	}
	/** The real one takes an IPlatformFile::FDirectoryVisitor&; the project passes a lambda. */
	template <typename Visitor>
	bool IterateDirectory(const TCHAR* Directory, Visitor&& InVisitor)
	{
		(void)Directory;
		(void)InVisitor;
		return true;
	}
};

class FPlatformFileManager
{
public:
	static FPlatformFileManager& Get();
	IPlatformFile& GetPlatformFile() { return File; }
	IPlatformFile File;
};
