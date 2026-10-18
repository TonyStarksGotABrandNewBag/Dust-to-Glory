// UObject and the handful of engine services reachable from it.
#pragma once

class UClass;
class UWorld;
class AActor;

class UObject
{
public:
	virtual ~UObject() = default;
	UWorld* GetWorld() const;
	UClass* GetClass() const;
	FString GetName() const;
	FName GetFName() const;
	UObject* GetOuter() const;
	bool IsValidLowLevel() const { return true; }
	/** Every UCLASS gets one from UHT; declared on the base so engine classes have it too. */
	static UClass* StaticClass();
	void ConditionalBeginDestroy() {}
};

class UClass : public UObject
{
public:
	FString GetName() const;
};

class UStruct : public UObject
{
};

class UFunction : public UStruct
{
};

/** T::StaticClass(), which every UCLASS gets from GENERATED_BODY in the real engine. */
namespace dtg_stub
{
	UClass* MakeClass();
}

template <typename T>
inline T* Cast(UObject* Object)
{
	return static_cast<T*>(Object);
}

template <typename T>
inline T* CastChecked(UObject* Object)
{
	return static_cast<T*>(Object);
}

/** NewObject<T>(Outer), the engine's allocator. Never actually called here. */
template <typename T>
inline T* NewObject(UObject* Outer = nullptr)
{
	(void)Outer;
	return nullptr;
}

template <typename T>
inline T* NewObject(UObject* Outer, UClass* Class, FName Name = FName())
{
	(void)Outer;
	(void)Class;
	(void)Name;
	return nullptr;
}

template <typename T>
inline T* NewObject(UObject* Outer, FName Name)
{
	(void)Outer;
	(void)Name;
	return nullptr;
}

inline UObject* StaticLoadObject(UClass* Class, UObject* Outer, const TCHAR* Path,
	const TCHAR* Filename = nullptr, uint32 LoadFlags = 0, UObject* Sandbox = nullptr)
{
	(void)Class; (void)Outer; (void)Path; (void)Filename; (void)LoadFlags; (void)Sandbox;
	return nullptr;
}

template <typename T>
inline T* LoadObject(UObject* Outer, const TCHAR* Path)
{
	(void)Outer;
	(void)Path;
	return nullptr;
}

template <typename T>
inline T* FindObject(UObject* Outer, const TCHAR* Path)
{
	(void)Outer;
	(void)Path;
	return nullptr;
}

/** Constructor helpers, used inside AActor constructors in the real engine. */
template <typename T>
inline T* CreateDefaultSubobject(const TCHAR* Name)
{
	(void)Name;
	return nullptr;
}

enum class EObjectFlags : uint32
{
	RF_NoFlags = 0,
	RF_Transient = 1,
};
