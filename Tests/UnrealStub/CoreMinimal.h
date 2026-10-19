// A stand-in for Unreal's core, just large enough to compile Source/DustToGlory/Game.
//
// WHAT THIS IS FOR. The engine layer has never been built by an actual engine, and every compile
// error in it so far was found by a person reading the code. This is a real compiler reading it
// instead. It catches what a compiler catches: misspelled members, wrong argument types and counts,
// unqualified dtg::sim names, narrowing conversions, missing overloads, bad returns.
//
// WHAT IT IS NOT. It does not run, it draws nothing, and it knows nothing about how Unreal behaves.
// A clean build here means the code is well-formed C++ against these declarations -- not that it
// works, and not that these declarations are right. Where a signature below is wrong, this will
// cheerfully disagree with the real engine in both directions. The real build is still the only
// proof, and docs/REALISM.md says so.
//
// FIDELITY. The signatures that matter are the ones with a trap in them, and they are copied as
// faithfully as I can state them -- above all FMath's integer conversions, where float and double
// return different widths. That exact asymmetry produced a template deduction failure in this
// codebase before, and a loose stub would have hidden it. Where a return type here is a guess it is
// marked with a NOTE.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

// ------------------------------------------------------------------ the reflection macros
//
// In the real engine these expand to generated code. Here they expand to nothing, which is what
// makes a .generated.h stub possible at all. Variadic because they take arbitrary tagged arguments
// with commas in them: UPROPERTY(EditAnywhere, Category = "x").
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UINTERFACE(...)

// SlateCore's Rendering/SlateRenderer.h forward-declares a global `struct Rect;`, and Slate/UMG
// bring it into every file. With `using namespace dtg::sim;` in a .cpp, a bare `Rect` is then
// ambiguous. Declared here so check_engine.py fails the same way the real engine does.
struct Rect;
// GENERATED_BODY() resolves, exactly as the real one does, through the file id the .generated.h
// most recently defined and the line it appears on -- which is how `Super` gets a per-class value.
// Tests/make_unreal_stub_headers.py writes those definitions.
#define BODY_MACRO_COMBINE_INNER(A, B, C, D) A##B##C##D
#define BODY_MACRO_COMBINE(A, B, C, D) BODY_MACRO_COMBINE_INNER(A, B, C, D)
#define GENERATED_BODY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY)
#define GENERATED_USTRUCT_BODY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY)
#define UMETA(...)

// The module export macro. Real builds define it per-module via the build tool.
#define DUSTTOGLORY_API
#define DLLEXPORT
#define DLLIMPORT

// ------------------------------------------------------------------ primitive types
using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using TCHAR = wchar_t;
using ANSICHAR = char;
using SIZE_T = std::size_t;

#define TEXT(x) L##x
#define UE_PI 3.1415926535897932
#define UE_SMALL_NUMBER 1.e-8
#define UE_KINDA_SMALL_NUMBER 1.e-4
#define UE_BIG_NUMBER 3.4e+38
#define SMALL_NUMBER 1.e-8
#define KINDA_SMALL_NUMBER 1.e-4
#define PI 3.1415926535897932
#define SCENE_QUERY_STAT(Name) FName(TEXT(#Name))
#define UE_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define FORCEINLINE inline
#define checkf(expr, ...) ((void)0)
#define check(expr) ((void)0)
#define ensure(expr) (!!(expr))

// Dynamic delegates. The engine layer declares one and broadcasts it; the parameter types still
// have to be real, so the declared Broadcast takes them.
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE(Name) \
	struct Name { void Broadcast() {} \
		template <typename T, typename M> void AddDynamic(T*, M) {} };
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(Name, T1, N1) \
	struct Name { void Broadcast(T1 N1) { (void)N1; } \
		template <typename T, typename M> void AddDynamic(T*, M) {} };
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(Name, T1, N1, T2, N2) \
	struct Name { void Broadcast(T1 N1, T2 N2) { (void)N1; (void)N2; } \
		template <typename T, typename M> void AddDynamic(T*, M) {} };
#define DECLARE_MULTICAST_DELEGATE_OneParam(Name, T1) \
	struct Name { void Broadcast(T1) {} };

enum ELevelTick
{
	LEVELTICK_TimeOnly,
	LEVELTICK_ViewportsOnly,
	LEVELTICK_All,
};

enum ETickingGroup
{
	TG_PrePhysics,
	TG_StartPhysics,
	TG_DuringPhysics,
	TG_EndPhysics,
	TG_PostPhysics,
	TG_PostUpdateWork,
	TG_LastDemotable,
	TG_NewlySpawned,
};

struct FActorComponentTickFunction
{
	bool bCanEverTick = true;
	ETickingGroup TickGroup = TG_DuringPhysics;
};

/** The engine's null-and-pending-kill check. */
inline bool IsValid(const class UObject* Object) { return Object != nullptr; }
template <typename T>
inline bool IsValid(const T* Object) { return Object != nullptr; }

/** Numeric limits, used where the engine layer seeds a running minimum. */
template <typename T>
struct TNumericLimits
{
	static constexpr T Min() { return std::numeric_limits<T>::lowest(); }
	static constexpr T Max() { return std::numeric_limits<T>::max(); }
	static constexpr T Lowest() { return std::numeric_limits<T>::lowest(); }
};

/** Hashing, for anything used as a TMap key. */
inline uint32 GetTypeHash(int32 Value) { return static_cast<uint32>(Value); }
inline uint32 GetTypeHash(uint32 Value) { return Value; }
inline uint32 GetTypeHash(int64 Value) { return static_cast<uint32>(Value); }
inline uint32 GetTypeHash(const void* Value) { return static_cast<uint32>(reinterpret_cast<std::uintptr_t>(Value)); }
inline uint32 GetTypeHash(const struct FIntPoint& Value);

enum EMouseLockMode
{
	DoNotLock,
	LockOnCapture,
	LockAlways,
	LockInFullscreen,
};

enum EMovementMode
{
	MOVE_None,
	MOVE_Walking,
	MOVE_NavWalking,
	MOVE_Falling,
	MOVE_Swimming,
	MOVE_Flying,
	MOVE_Custom,
};

enum ESimpleElementBlendMode
{
	SE_BLEND_Opaque,
	SE_BLEND_Masked,
	SE_BLEND_Translucent,
	SE_BLEND_Additive,
	SE_BLEND_AlphaBlend,
};

struct FTimerHandle
{
	bool IsValid() const { return Handle != 0; }
	void Invalidate() { Handle = 0; }
	uint64 Handle = 0;
};

/** Unreal's memory helpers, used by the audio components to clear a buffer. */
struct FMemory
{
	static void Memzero(void* Destination, SIZE_T Count) { std::memset(Destination, 0, Count); }
	static void Memcpy(void* Destination, const void* Source, SIZE_T Count)
	{
		std::memcpy(Destination, Source, Count);
	}
	static void* Malloc(SIZE_T Count) { return std::malloc(Count); }
	static void Free(void* Pointer) { std::free(Pointer); }
};

// ------------------------------------------------------------------ strings
namespace dtg_stub
{
	std::string Narrow(const wchar_t* Text);
	std::wstring Widen(const char* Text);

	/** What TCHAR_TO_UTF8 produces: a temporary holding the bytes, usable as const char*. */
	struct Utf8Holder
	{
		std::string Value;
		explicit Utf8Holder(const wchar_t* Text) : Value(Narrow(Text)) {}
		operator const char*() const { return Value.c_str(); }
		operator std::string() const { return Value; }
	};
	struct TcharHolder
	{
		std::wstring Value;
		explicit TcharHolder(const char* Text) : Value(Widen(Text)) {}
		operator const wchar_t*() const { return Value.c_str(); }
	};
}

#define TCHAR_TO_UTF8(str) (dtg_stub::Utf8Holder(str))
#define TCHAR_TO_ANSI(str) (dtg_stub::Utf8Holder(str))
#define UTF8_TO_TCHAR(str) (dtg_stub::TcharHolder(str))
#define ANSI_TO_TCHAR(str) (dtg_stub::TcharHolder(str))

class FName;

class FString
{
public:
	FString() = default;
	FString(const TCHAR* InText) : Text(InText ? InText : L"") {}
	FString(const std::wstring& InText) : Text(InText) {}
	/** UTF8_TO_TCHAR(...) assigned straight into an FString, which the engine layer does a lot. */
	FString(const dtg_stub::TcharHolder& Holder) : Text(Holder.Value) {}

	const TCHAR* operator*() const { return Text.c_str(); }
	int32 Len() const { return static_cast<int32>(Text.size()); }
	bool IsEmpty() const { return Text.empty(); }
	void Empty() { Text.clear(); }
	void Reset() { Text.clear(); }
	void Reserve(int32 Count) { Text.reserve(static_cast<std::size_t>(Count)); }

	FString& operator+=(const FString& Other) { Text += Other.Text; return *this; }
	FString& operator+=(const TCHAR* Other) { Text += Other; return *this; }
	FString operator+(const FString& Other) const { return FString(Text + Other.Text); }
	FString operator+(const TCHAR* Other) const { return FString(Text + Other); }
	bool operator==(const FString& Other) const { return Text == Other.Text; }
	bool operator!=(const FString& Other) const { return Text != Other.Text; }
	bool operator<(const FString& Other) const { return Text < Other.Text; }

	bool Equals(const FString& Other) const { return Text == Other.Text; }
	bool Contains(const FString& Other) const { return Text.find(Other.Text) != std::wstring::npos; }
	bool StartsWith(const FString& Other) const { return Text.rfind(Other.Text, 0) == 0; }
	bool EndsWith(const FString& Other) const
	{
		return Text.size() >= Other.Text.size() &&
			Text.compare(Text.size() - Other.Text.size(), Other.Text.size(), Other.Text) == 0;
	}
	FString Left(int32 Count) const { return FString(Text.substr(0, static_cast<std::size_t>(Count))); }
	FString Right(int32 Count) const
	{
		const std::size_t N = static_cast<std::size_t>(Count);
		return FString(Text.size() > N ? Text.substr(Text.size() - N) : Text);
	}
	FString Mid(int32 Start, int32 Count = -1) const
	{
		const std::size_t From = static_cast<std::size_t>(Start);
		if (From >= Text.size())
		{
			return FString();
		}
		return FString(Count < 0 ? Text.substr(From) : Text.substr(From, static_cast<std::size_t>(Count)));
	}
	FString RightChop(int32 Count) const { return Mid(Count); }
	FString LeftChop(int32 Count) const { return Left(Len() - Count); }
	FString ToUpper() const;
	FString ToLower() const;
	FString TrimStartAndEnd() const;
	FString Replace(const TCHAR* From, const TCHAR* To) const;
	int32 Find(const FString& Sub) const
	{
		const std::size_t At = Text.find(Sub.Text);
		return At == std::wstring::npos ? -1 : static_cast<int32>(At);
	}
	void Append(const FString& Other) { Text += Other.Text; }
	bool RemoveFromStart(const FString& Prefix)
	{
		if (!StartsWith(Prefix))
		{
			return false;
		}
		Text = Text.substr(Prefix.Text.size());
		return true;
	}
	bool RemoveFromEnd(const FString& Suffix)
	{
		if (!EndsWith(Suffix))
		{
			return false;
		}
		Text = Text.substr(0, Text.size() - Suffix.Text.size());
		return true;
	}
	void AppendChar(TCHAR Char) { Text += Char; }

	static FString FromInt(int32 Value);
	static FString SanitizeFloat(double Value);
	FString& operator=(const dtg_stub::TcharHolder& Holder) { Text = Holder.Value; return *this; }

	/** NOTE: the real one is variadic on TCHAR*; this reproduces the shape, not the formatting. */
	template <typename... Args>
	static FString Printf(const TCHAR* Format, Args... InArgs);
	template <typename RangeType>
	static FString Join(const RangeType& Parts, const TCHAR* Separator);

	std::wstring Text;
};

inline FString operator+(const TCHAR* Left, const FString& Right) { return FString(std::wstring(Left) + Right.Text); }

class FName
{
public:
	FName() = default;
	FName(const TCHAR* InText) : Value(InText ? InText : L"") {}
	FName(const FString& InText) : Value(InText.Text) {}
	FString ToString() const { return FString(Value); }
	bool operator==(const FName& Other) const { return Value == Other.Value; }
	std::wstring Value;
};

class FText
{
public:
	static FText FromString(const FString& In) { FText Out; Out.Value = In; return Out; }
	FString ToString() const { return Value; }
	FString Value;
};

uint32 GetTypeHash(const FString& Value);
uint32 GetTypeHash(const FName& Value);

struct FCString
{
	static int32 Atoi(const TCHAR* Text);
	static uint64 Strtoui64(const TCHAR* Text, TCHAR** End, int32 Base);
	static double Atod(const TCHAR* Text);
};

// ------------------------------------------------------------------ containers
template <typename T>
class TArray
{
public:
	TArray() = default;
	TArray(std::initializer_list<T> Init) : Items(Init) {}

	int32 Num() const { return static_cast<int32>(Items.size()); }
	bool IsEmpty() const { return Items.empty(); }
	bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < Num(); }
	T& operator[](int32 Index) { return Items[static_cast<std::size_t>(Index)]; }
	const T& operator[](int32 Index) const { return Items[static_cast<std::size_t>(Index)]; }

	int32 Add(const T& Item) { Items.push_back(Item); return Num() - 1; }
	int32 Add(T&& Item) { Items.push_back(std::move(Item)); return Num() - 1; }
	int32 Emplace(const T& Item) { Items.push_back(Item); return Num() - 1; }
	template <typename... Args>
	int32 Emplace(Args&&... InArgs) { Items.emplace_back(std::forward<Args>(InArgs)...); return Num() - 1; }
	int32 AddUnique(const T& Item)
	{
		for (int32 Index = 0; Index < Num(); ++Index)
		{
			if (Items[static_cast<std::size_t>(Index)] == Item)
			{
				return Index;
			}
		}
		return Add(Item);
	}
	void Append(const TArray<T>& Other) { Items.insert(Items.end(), Other.Items.begin(), Other.Items.end()); }
	void Append(std::initializer_list<T> Other) { Items.insert(Items.end(), Other.begin(), Other.end()); }
	void Reserve(int32 Count) { Items.reserve(static_cast<std::size_t>(Count)); }
	void SetNum(int32 Count) { Items.resize(static_cast<std::size_t>(Count)); }
	void SetNumUninitialized(int32 Count) { Items.resize(static_cast<std::size_t>(Count)); }
	void SetNumZeroed(int32 Count) { Items.assign(static_cast<std::size_t>(Count), T()); }
	void Init(const T& Item, int32 Count) { Items.assign(static_cast<std::size_t>(Count), Item); }
	void Reset() { Items.clear(); }
	void Empty() { Items.clear(); }
	void RemoveAt(int32 Index) { Items.erase(Items.begin() + Index); }
	void RemoveAtSwap(int32 Index)
	{
		Items[static_cast<std::size_t>(Index)] = Items.back();
		Items.pop_back();
	}
	void Insert(const T& Item, int32 Index) { Items.insert(Items.begin() + Index, Item); }
	bool Contains(const T& Item) const
	{
		return std::find(Items.begin(), Items.end(), Item) != Items.end();
	}
	T& Last() { return Items.back(); }
	const T& Last() const { return Items.back(); }
	T& Top() { return Items.back(); }
	T Pop() { T Out = Items.back(); Items.pop_back(); return Out; }
	T* GetData() { return Items.data(); }
	const T* GetData() const { return Items.data(); }
	template <typename Predicate>
	void Sort(Predicate Pred) { std::stable_sort(Items.begin(), Items.end(), Pred); }
	void Sort() { std::stable_sort(Items.begin(), Items.end()); }
	template <typename Predicate>
	T* FindByPredicate(Predicate Pred)
	{
		auto It = std::find_if(Items.begin(), Items.end(), Pred);
		return It == Items.end() ? nullptr : &*It;
	}
	template <typename Predicate>
	const T* FindByPredicate(Predicate Pred) const
	{
		auto It = std::find_if(Items.begin(), Items.end(), Pred);
		return It == Items.end() ? nullptr : &*It;
	}
	template <typename Predicate>
	int32 IndexOfByPredicate(Predicate Pred) const
	{
		auto It = std::find_if(Items.begin(), Items.end(), Pred);
		return It == Items.end() ? -1 : static_cast<int32>(It - Items.begin());
	}
	template <typename Predicate>
	int32 RemoveAll(Predicate Pred)
	{
		const std::size_t Before = Items.size();
		Items.erase(std::remove_if(Items.begin(), Items.end(), Pred), Items.end());
		return static_cast<int32>(Before - Items.size());
	}

	typename std::vector<T>::iterator begin() { return Items.begin(); }
	typename std::vector<T>::iterator end() { return Items.end(); }
	typename std::vector<T>::const_iterator begin() const { return Items.begin(); }
	typename std::vector<T>::const_iterator end() const { return Items.end(); }

	std::vector<T> Items;
};

template <typename K, typename V>
class TMap
{
public:
	struct Pair
	{
		K Key;
		V Value;
	};

	int32 Num() const { return static_cast<int32>(Items.size()); }
	void Add(const K& Key, const V& Value) { Items[Key] = Value; }
	V& FindOrAdd(const K& Key) { return Items[Key]; }
	V* Find(const K& Key)
	{
		auto It = Items.find(Key);
		return It == Items.end() ? nullptr : &It->second;
	}
	const V* Find(const K& Key) const
	{
		auto It = Items.find(Key);
		return It == Items.end() ? nullptr : &It->second;
	}
	bool Contains(const K& Key) const { return Items.find(Key) != Items.end(); }
	void Remove(const K& Key) { Items.erase(Key); }
	void Reset() { Items.clear(); }
	void Empty() { Items.clear(); }
	V& operator[](const K& Key) { return Items.at(Key); }
	const V& operator[](const K& Key) const { return Items.at(Key); }

	struct Iterator
	{
		typename std::map<K, V>::iterator It;
		bool operator!=(const Iterator& Other) const { return It != Other.It; }
		void operator++() { ++It; }
		Pair operator*() const { return Pair{It->first, It->second}; }
	};
	Iterator begin() { return Iterator{Items.begin()}; }
	Iterator end() { return Iterator{Items.end()}; }

	std::map<K, V> Items;
};

template <typename T>
class TSet
{
public:
	void Add(const T& Item) { Items.push_back(Item); }
	bool Contains(const T& Item) const { return std::find(Items.begin(), Items.end(), Item) != Items.end(); }
	int32 Num() const { return static_cast<int32>(Items.size()); }
	void Reset() { Items.clear(); }
	typename std::vector<T>::iterator begin() { return Items.begin(); }
	typename std::vector<T>::iterator end() { return Items.end(); }
	std::vector<T> Items;
};

/** A raw pointer with a UObject-shaped wrapper, which is what TObjectPtr is for our purposes. */
template <typename T>
class TObjectPtr
{
public:
	TObjectPtr() = default;
	TObjectPtr(T* InPtr) : Ptr(InPtr) {}
	TObjectPtr(decltype(nullptr)) : Ptr(nullptr) {}
	operator T*() const { return Ptr; }
	T* operator->() const { return Ptr; }
	T& operator*() const { return *Ptr; }
	T* Get() const { return Ptr; }
	explicit operator bool() const { return Ptr != nullptr; }
	TObjectPtr& operator=(T* InPtr) { Ptr = InPtr; return *this; }
	T* Ptr = nullptr;
};

template <typename T>
class TSubclassOf
{
public:
	TSubclassOf() = default;
	TSubclassOf(class UClass* InClass) : Class(InClass) {}
	operator UClass*() const { return Class; }
	UClass* Get() const { return Class; }
	UClass* Class = nullptr;
};

template <typename T>
class TWeakObjectPtr
{
public:
	TWeakObjectPtr() = default;
	TWeakObjectPtr(T* InPtr) : Ptr(InPtr) {}
	T* Get() const { return Ptr; }
	bool IsValid() const { return Ptr != nullptr; }
	T* operator->() const { return Ptr; }
	T* Ptr = nullptr;
};

template <typename T>
using TOptional = T;

// ------------------------------------------------------------------ maths
struct FVector2D
{
	double X = 0.0;
	double Y = 0.0;
	FVector2D() = default;
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
	FVector2D operator+(const FVector2D& Other) const { return FVector2D(X + Other.X, Y + Other.Y); }
	FVector2D operator-(const FVector2D& Other) const { return FVector2D(X - Other.X, Y - Other.Y); }
	FVector2D operator*(double Scale) const { return FVector2D(X * Scale, Y * Scale); }
	FVector2D& operator+=(const FVector2D& Other) { X += Other.X; Y += Other.Y; return *this; }
	double Size() const { return std::sqrt(X * X + Y * Y); }
	double SizeSquared() const { return X * X + Y * Y; }
	FVector2D GetSafeNormal() const;
	static double DistSquared(const FVector2D& A, const FVector2D& B);
	static const FVector2D ZeroVector;
};

struct FRotator;
struct FQuat;

struct FVector
{
	static const FVector ZeroVector;

	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	FVector() = default;
	explicit FVector(double InAll) : X(InAll), Y(InAll), Z(InAll) {}
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

	FVector operator+(const FVector& Other) const { return FVector(X + Other.X, Y + Other.Y, Z + Other.Z); }
	FVector operator-(const FVector& Other) const { return FVector(X - Other.X, Y - Other.Y, Z - Other.Z); }
	FVector operator-() const { return FVector(-X, -Y, -Z); }
	FVector operator*(double Scale) const { return FVector(X * Scale, Y * Scale, Z * Scale); }
	FVector operator*(const FVector& Other) const { return FVector(X * Other.X, Y * Other.Y, Z * Other.Z); }
	FVector operator/(double Scale) const { return FVector(X / Scale, Y / Scale, Z / Scale); }
	FVector& operator+=(const FVector& Other) { X += Other.X; Y += Other.Y; Z += Other.Z; return *this; }
	FVector& operator-=(const FVector& Other) { X -= Other.X; Y -= Other.Y; Z -= Other.Z; return *this; }
	FVector& operator*=(double Scale) { X *= Scale; Y *= Scale; Z *= Scale; return *this; }
	bool operator==(const FVector& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }

	double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	double Size2D() const { return std::sqrt(X * X + Y * Y); }
	double SizeSquared() const { return X * X + Y * Y + Z * Z; }
	double SizeSquared2D() const { return X * X + Y * Y; }
	/** NOTE: Unreal 5's signature -- the second argument is what you get back for a zero vector. */
	FVector GetSafeNormal(double Tolerance = 1e-8, const FVector& ResultIfZero = ZeroVector) const;
	FVector GetSafeNormal2D(double Tolerance = 1e-8, const FVector& ResultIfZero = ZeroVector) const;
	FRotator Rotation() const;
	FRotator ToOrientationRotator() const;
	FQuat ToOrientationQuat() const;
	void Normalize() { *this = GetSafeNormal(); }
	bool IsNearlyZero(double Tolerance = 1e-4) const { return Size() <= Tolerance; }
	bool IsZero() const { return X == 0.0 && Y == 0.0 && Z == 0.0; }
	FVector RotateAngleAxis(double Degrees, const FVector& Axis) const;

	static double Dist(const FVector& A, const FVector& B) { return (A - B).Size(); }
	static double Dist2D(const FVector& A, const FVector& B) { return (A - B).Size2D(); }
	static double DistSquared(const FVector& A, const FVector& B) { return (A - B).SizeSquared(); }
	static double DotProduct(const FVector& A, const FVector& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static FVector CrossProduct(const FVector& A, const FVector& B)
	{
		return FVector(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X);
	}

	static const FVector OneVector;
	static const FVector UpVector;
	static const FVector DownVector;
	static const FVector ForwardVector;
	static const FVector RightVector;
};

inline FVector operator*(double Scale, const FVector& Value) { return Value * Scale; }

struct FIntPoint
{
	int32 X = 0;
	int32 Y = 0;
	FIntPoint() = default;
	FIntPoint(int32 InX, int32 InY) : X(InX), Y(InY) {}
	bool operator==(const FIntPoint& Other) const { return X == Other.X && Y == Other.Y; }
	bool operator<(const FIntPoint& Other) const { return X != Other.X ? X < Other.X : Y < Other.Y; }
};

inline uint32 GetTypeHash(const FIntPoint& Value)
{
	return static_cast<uint32>(Value.X * 73856093) ^ static_cast<uint32>(Value.Y * 19349663);
}

struct FIntVector
{
	int32 X = 0;
	int32 Y = 0;
	int32 Z = 0;
	FIntVector() = default;
	FIntVector(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}
};

struct FRotator
{
	double Pitch = 0.0;
	double Yaw = 0.0;
	double Roll = 0.0;
	FRotator() = default;
	FRotator(double InPitch, double InYaw, double InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
	FRotator operator+(const FRotator& Other) const
	{
		return FRotator(Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll);
	}
	FRotator operator-(const FRotator& Other) const
	{
		return FRotator(Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll);
	}
	FVector Vector() const;
	FVector RotateVector(const FVector& In) const;
	FVector UnrotateVector(const FVector& In) const;
	struct FQuat Quaternion() const;
	FRotator GetNormalized() const;
	static const FRotator ZeroRotator;
};

struct FQuat
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	double W = 1.0;
	FQuat() = default;
	FQuat(double InX, double InY, double InZ, double InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
	FQuat(const FVector& Axis, double Angle);
	FVector RotateVector(const FVector& In) const;
	FVector UnrotateVector(const FVector& In) const;
	FRotator Rotator() const;
	FQuat operator*(const FQuat& Other) const;
	void Normalize(double Tolerance = 1e-8);
	static const FQuat Identity;
};

struct FTransform
{
	FTransform() = default;
	FTransform(const FQuat& InRotation, const FVector& InTranslation, const FVector& InScale = FVector(1.0, 1.0, 1.0))
		: Rotation(InRotation), Translation(InTranslation), Scale3D(InScale) {}
	FTransform(const FRotator& InRotation, const FVector& InTranslation)
		: Rotation(InRotation.Quaternion()), Translation(InTranslation) {}
	explicit FTransform(const FVector& InTranslation) : Translation(InTranslation) {}

	FVector GetLocation() const { return Translation; }
	void SetLocation(const FVector& In) { Translation = In; }
	FQuat GetRotation() const { return Rotation; }
	void SetRotation(const FQuat& In) { Rotation = In; }
	FVector GetScale3D() const { return Scale3D; }
	void SetScale3D(const FVector& In) { Scale3D = In; }
	FVector TransformPosition(const FVector& In) const;
	FVector InverseTransformPosition(const FVector& In) const;
	FVector TransformVector(const FVector& In) const;

	FQuat Rotation;
	FVector Translation;
	FVector Scale3D{1.0, 1.0, 1.0};
	static const FTransform Identity;
};

struct FLinearColor
{
	float R = 0.0f;
	float G = 0.0f;
	float B = 0.0f;
	float A = 1.0f;
	FLinearColor() = default;
	FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor operator*(float Scale) const { return FLinearColor(R * Scale, G * Scale, B * Scale, A); }
	FLinearColor operator+(const FLinearColor& Other) const
	{
		return FLinearColor(R + Other.R, G + Other.G, B + Other.B, A + Other.A);
	}
	struct FColor ToFColor(bool bSrgb) const;
	/** NOTE: real signature takes uint8 H, S, V. */
	static FLinearColor MakeFromHSV8(uint8 H, uint8 S, uint8 V);
	/** Implicit FColor -> FLinearColor, which the real engine also provides. */
	FLinearColor(const struct FColor& Source);
	static const FLinearColor White;
	static const FLinearColor Black;
	static const FLinearColor Red;
	static const FLinearColor Green;
	static const FLinearColor Blue;
	static const FLinearColor Yellow;
	static const FLinearColor Gray;
	static const FLinearColor Transparent;
};

struct FColor
{
	uint8 B = 0;
	uint8 G = 0;
	uint8 R = 0;
	uint8 A = 255;
	FColor() = default;
	FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : B(InB), G(InG), R(InR), A(InA) {}
	FLinearColor ReinterpretAsLinear() const;
	static const FColor White;
	static const FColor Black;
	static const FColor Red;
};

/** Dates and times, used when naming save files. */
struct FDateTime
{
	static FDateTime Now() { return FDateTime(); }
	static FDateTime UtcNow() { return FDateTime(); }
	FString ToString() const { return FString(TEXT("2026.01.01-00.00.00")); }
	FString ToString(const TCHAR* Format) const { (void)Format; return ToString(); }
	int64 GetTicks() const { return 0; }
};

struct FTimespan
{
	double GetTotalSeconds() const { return 0.0; }
};

struct FBox
{
	FVector Min;
	FVector Max;
	FBox() = default;
	FBox(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}
};

// ------------------------------------------------------------------ FMath
//
// The integer conversions are the dangerous part and are reproduced exactly as Unreal 5 declares
// them: the float overloads give int32 and the double overloads give int64. That asymmetry is why
// FMath::Max(1, FMath::CeilToInt(SomeDouble)) fails to compile -- int versus int64 -- and it is
// precisely the error this stub exists to catch, so it must not be smoothed over.
struct FMath
{
	template <typename T>
	static T Abs(T Value) { return Value < T(0) ? -Value : Value; }
	template <typename T>
	static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T>
	static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T>
	static T Clamp(T Value, T Low, T High) { return Value < Low ? Low : (Value > High ? High : Value); }
	template <typename T>
	static T Square(T Value) { return Value * Value; }
	template <typename T, typename U>
	static T Lerp(T A, T B, U Alpha) { return static_cast<T>(A + (B - A) * Alpha); }

	static double Sqrt(double Value) { return std::sqrt(Value); }
	static float Sqrt(float Value) { return std::sqrt(Value); }
	static double Sin(double Value) { return std::sin(Value); }
	static double Cos(double Value) { return std::cos(Value); }
	static double Tan(double Value) { return std::tan(Value); }
	static double Asin(double Value) { return std::asin(Value); }
	static double Acos(double Value) { return std::acos(Value); }
	static double Atan(double Value) { return std::atan(Value); }
	static double Atan2(double Y, double X) { return std::atan2(Y, X); }
	static double Exp(double Value) { return std::exp(Value); }
	static double Loge(double Value) { return std::log(Value); }
	static double Pow(double Base, double Power) { return std::pow(Base, Power); }
	static double Fmod(double A, double B) { return std::fmod(A, B); }
	static double DegreesToRadians(double Degrees) { return Degrees * (UE_PI / 180.0); }
	static double RadiansToDegrees(double Radians) { return Radians * (180.0 / UE_PI); }
	static bool IsNearlyZero(double Value, double Tolerance = 1e-8) { return std::fabs(Value) <= Tolerance; }
	static bool IsNearlyEqual(double A, double B, double Tolerance = 1e-8) { return std::fabs(A - B) <= Tolerance; }
	static bool IsFinite(double Value) { return std::isfinite(Value); }

	static int32 TruncToInt(float Value) { return static_cast<int32>(Value); }
	static int64 TruncToInt(double Value) { return static_cast<int64>(Value); }
	static int32 FloorToInt(float Value) { return static_cast<int32>(std::floor(Value)); }
	static int64 FloorToInt(double Value) { return static_cast<int64>(std::floor(Value)); }
	static int32 CeilToInt(float Value) { return static_cast<int32>(std::ceil(Value)); }
	static int64 CeilToInt(double Value) { return static_cast<int64>(std::ceil(Value)); }
	static int32 RoundToInt(float Value) { return static_cast<int32>(std::lround(Value)); }
	static int64 RoundToInt(double Value) { return static_cast<int64>(std::llround(Value)); }

	static int32 TruncToInt32(float Value) { return static_cast<int32>(Value); }
	static int32 TruncToInt32(double Value) { return static_cast<int32>(Value); }
	static int32 FloorToInt32(float Value) { return static_cast<int32>(std::floor(Value)); }
	static int32 FloorToInt32(double Value) { return static_cast<int32>(std::floor(Value)); }
	static int32 CeilToInt32(float Value) { return static_cast<int32>(std::ceil(Value)); }
	static int32 CeilToInt32(double Value) { return static_cast<int32>(std::ceil(Value)); }
	static int32 RoundToInt32(float Value) { return static_cast<int32>(std::lround(Value)); }
	static int32 RoundToInt32(double Value) { return static_cast<int32>(std::llround(Value)); }
	static int64 CeilToInt64(double Value) { return static_cast<int64>(std::ceil(Value)); }
	static int64 FloorToInt64(double Value) { return static_cast<int64>(std::floor(Value)); }

	static float FloorToFloat(float Value) { return std::floor(Value); }
	static double FloorToDouble(double Value) { return std::floor(Value); }

	static float FRand();
	static float FRandRange(float Low, float High);
	static double FRandRange(double Low, double High);
	static int32 RandRange(int32 Low, int32 High);
	static int32 RandHelper(int32 Range);
	static bool RandBool();

	template <typename T>
	static T FInterpTo(T Current, T Target, T DeltaTime, T Speed)
	{
		if (Speed <= T(0))
		{
			return Target;
		}
		const T Step = (Target - Current) * Clamp(DeltaTime * Speed, T(0), T(1));
		return Current + Step;
	}
	static double GetMappedRangeValueClamped(const FVector2D& InRange, const FVector2D& OutRange, double Value);
	static double UnwindDegrees(double Value);
	static double FindDeltaAngleDegrees(double A, double B);
};

// ------------------------------------------------------------------ logging
struct FLogCategoryBase
{
	explicit FLogCategoryBase(const TCHAR* InName) : Name(InName) {}
	const TCHAR* Name;
};
#define DECLARE_LOG_CATEGORY_EXTERN(Name, Default, Compile) extern FLogCategoryBase Name
#define DEFINE_LOG_CATEGORY(Name) FLogCategoryBase Name(TEXT(#Name))
#define DECLARE_LOG_CATEGORY_CLASS(Name, Default, Compile) static FLogCategoryBase Name(TEXT(#Name))

namespace dtg_stub
{
	/** Swallows the arguments, but only after the compiler has type-checked every one of them. */
	template <typename... Args>
	inline void Log(const FLogCategoryBase&, const TCHAR*, Args&&...) {}
}
#define UE_LOG(Category, Verbosity, Format, ...) dtg_stub::Log(Category, Format, ##__VA_ARGS__)
#define UE_CLOG(Condition, Category, Verbosity, Format, ...) dtg_stub::Log(Category, Format, ##__VA_ARGS__)

// ------------------------------------------------------------------ template helpers
template <typename T>
T* Cast(class UObject* Object);
template <typename T>
T* CastChecked(class UObject* Object);
template <typename T, typename U>
T* Cast(U* Object);

template <typename T>
struct TStatId {};

#include "UObject/ObjectBase.h"

// FString::Printf and Join need the full types above.
template <typename... Args>
inline FString FString::Printf(const TCHAR* Format, Args... InArgs)
{
	(void)sizeof...(InArgs);
	return FString(Format);
}

template <typename RangeType>
inline FString FString::Join(const RangeType& Parts, const TCHAR* Separator)
{
	FString Out;
	bool bFirst = true;
	for (const auto& Part : Parts)
	{
		if (!bFirst)
		{
			Out += Separator;
		}
		Out += Part;
		bFirst = false;
	}
	return Out;
}
