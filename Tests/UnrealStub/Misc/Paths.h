#pragma once
#include "CoreMinimal.h"
struct FPaths
{
	static FString ProjectDir();
	static FString ProjectContentDir();
	static FString ProjectSavedDir();
	static FString GetCleanFilename(const FString& Path);
	static FString GetPath(const FString& Path);
	static FString GetExtension(const FString& Path, bool bIncludeDot = false);
	static bool FileExists(const FString& Path);
	static bool DirectoryExists(const FString& Path);
	/** NOTE: the real one is a variadic template over path-like arguments. */
	template <typename... Args>
	static FString Combine(Args... InArgs);
};

namespace dtg_stub
{
	inline void AppendPath(FString&, bool) {}
	inline FString JoinPaths() { return FString(); }
	template <typename First, typename... Rest>
	inline FString JoinPaths(const First& InFirst, const Rest&... InRest)
	{
		FString Out = FString(InFirst);
		const FString Tail = JoinPaths(InRest...);
		if (!Tail.IsEmpty())
		{
			Out += TEXT("/");
			Out += Tail;
		}
		return Out;
	}
}

template <typename... Args>
inline FString FPaths::Combine(Args... InArgs)
{
	return dtg_stub::JoinPaths(InArgs...);
}
