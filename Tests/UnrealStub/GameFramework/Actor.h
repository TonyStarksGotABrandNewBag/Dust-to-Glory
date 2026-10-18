// AActor, USceneComponent and the primitive/collision surface the engine layer uses.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class UMaterialInterface;

enum ECollisionChannel
{
	ECC_WorldStatic,
	ECC_WorldDynamic,
	ECC_Pawn,
	ECC_Visibility,
	ECC_Camera,
	ECC_PhysicsBody,
	ECC_Vehicle,
	ECC_Destructible,
	// Project-defined trace channels, named in DefaultEngine.ini in a real build.
	ECC_GameTraceChannel1,
	ECC_GameTraceChannel2,
	ECC_GameTraceChannel3,
	ECC_GameTraceChannel4,
};

namespace ECollisionEnabled
{
	enum Type
	{
		NoCollision,
		QueryOnly,
		PhysicsOnly,
		QueryAndPhysics,
	};
}

// Plain enum, not an enum class: Unreal's is unscoped, so ECR_Block is spelled bare at the
// call site. Scoping it here would have forced a change to the project that the real build
// would then reject.
enum ECollisionResponse
{
	ECR_Ignore,
	ECR_Overlap,
	ECR_Block,
};

namespace EComponentMobility
{
	enum Type
	{
		Static,
		Stationary,
		Movable,
	};
}

enum class ESpawnActorCollisionHandlingMethod : uint8
{
	Undefined,
	AlwaysSpawn,
	AdjustIfPossibleButAlwaysSpawn,
	AdjustIfPossibleButDontSpawnIfColliding,
	DontSpawnIfColliding,
};

enum class EAttachmentRule : uint8
{
	KeepRelative,
	KeepWorld,
	SnapToTarget,
};

struct FAttachmentTransformRules
{
	static const FAttachmentTransformRules KeepRelativeTransform;
	static const FAttachmentTransformRules SnapToTargetNotIncludingScale;
	static const FAttachmentTransformRules KeepWorldTransform;
};

struct FHitResult
{
	bool bBlockingHit = false;
	double Time = 0.0;
	double Distance = 0.0;
	FVector Location;
	FVector ImpactPoint;
	FVector Normal;
	FVector ImpactNormal;
	bool bStartPenetrating = false;
	double PenetrationDepth = 0.0;
	FVector TraceStart;
	FVector TraceEnd;
	AActor* GetActor() const { return HitActor; }
	class UPrimitiveComponent* GetComponent() const { return HitComponent; }
	AActor* HitActor = nullptr;
	UPrimitiveComponent* HitComponent = nullptr;
};

struct FCollisionQueryParams
{
	FCollisionQueryParams() = default;
	explicit FCollisionQueryParams(FName InTag, bool bInTraceComplex = false, const AActor* InIgnore = nullptr)
		: TraceTag(InTag), bTraceComplex(bInTraceComplex) { (void)InIgnore; }
	void AddIgnoredActor(const AActor* Actor) { (void)Actor; }
	void AddIgnoredComponent(const UPrimitiveComponent* Component) { (void)Component; }
	FName TraceTag;
	bool bTraceComplex = false;
	bool bReturnPhysicalMaterial = false;
};

struct FCollisionShape
{
	static FCollisionShape MakeSphere(double Radius) { (void)Radius; return FCollisionShape(); }
	static FCollisionShape MakeBox(const FVector& Extent) { (void)Extent; return FCollisionShape(); }
	static FCollisionShape MakeCapsule(double Radius, double HalfHeight)
	{
		(void)Radius;
		(void)HalfHeight;
		return FCollisionShape();
	}
};

struct FActorSpawnParameters
{
	FName Name;
	AActor* Owner = nullptr;
	class APawn* Instigator = nullptr;
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::Undefined;
};

class UActorComponent : public UObject
{
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
	{
		(void)DeltaTime;
		(void)TickType;
		(void)Function;
	}
	virtual void BeginPlay() {}
	virtual void InitializeComponent() {}
	void RegisterComponent() {}
	void UnregisterComponent() {}
	void DestroyComponent() {}
	void SetActive(bool bNewActive, bool bReset = false) { (void)bNewActive; (void)bReset; }
	bool IsActive() const { return bIsActive; }
	void SetComponentTickEnabled(bool bEnabled) { (void)bEnabled; }
	AActor* GetOwner() const { return ComponentOwner; }
	void SetMobility(EComponentMobility::Type Value) { (void)Value; }

	bool bIsActive = true;
	bool bAutoActivate = true;
	AActor* ComponentOwner = nullptr;
	FActorComponentTickFunction PrimaryComponentTick;
};

class USceneComponent : public UActorComponent
{
public:
	void SetupAttachment(USceneComponent* Parent, FName Socket = FName())
	{
		(void)Parent;
		(void)Socket;
	}
	bool AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& Rules, FName Socket = FName())
	{
		(void)Parent;
		(void)Rules;
		(void)Socket;
		return true;
	}
	void DetachFromComponent(const FAttachmentTransformRules& Rules) { (void)Rules; }

	void SetRelativeLocation(const FVector& Location) { RelativeLocation = Location; }
	void SetRelativeRotation(const FRotator& Rotation) { RelativeRotation = Rotation; }
	void SetRelativeRotation(const FQuat& Rotation) { RelativeRotation = Rotation.Rotator(); }
	void SetRelativeScale3D(const FVector& Scale) { RelativeScale = Scale; }
	void SetRelativeTransform(const FTransform& Transform) { (void)Transform; }
	void SetWorldLocation(const FVector& Location) { WorldLocation = Location; }
	void SetWorldRotation(const FRotator& Rotation) { WorldRotation = Rotation; }
	void SetWorldRotation(const FQuat& Rotation) { WorldRotation = Rotation.Rotator(); }
	void SetWorldScale3D(const FVector& Scale) { (void)Scale; }
	void SetWorldTransform(const FTransform& Transform) { (void)Transform; }
	void AddLocalOffset(const FVector& Delta) { (void)Delta; }
	void AddWorldOffset(const FVector& Delta) { (void)Delta; }
	void AddLocalRotation(const FRotator& Delta) { (void)Delta; }

	FVector GetRelativeLocation() const { return RelativeLocation; }
	FRotator GetRelativeRotation() const { return RelativeRotation; }
	FVector GetComponentLocation() const { return WorldLocation; }
	FRotator GetComponentRotation() const { return WorldRotation; }
	FQuat GetComponentQuat() const { return WorldRotation.Quaternion(); }
	FTransform GetComponentTransform() const { return FTransform(WorldRotation, WorldLocation); }
	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;
	void SetVisibility(bool bVisible, bool bPropagate = false) { (void)bVisible; (void)bPropagate; }
	bool GetVisibleFlag() const { return true; }
	bool IsVisible() const { return true; }
	void SetHiddenInGame(bool bHidden, bool bPropagate = false) { (void)bHidden; (void)bPropagate; }

	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale{1.0, 1.0, 1.0};
	FVector WorldLocation;
	FRotator WorldRotation;
};

class UPrimitiveComponent : public USceneComponent
{
public:
	void SetCollisionEnabled(ECollisionEnabled::Type Value) { (void)Value; }
	void SetCollisionObjectType(ECollisionChannel Channel) { (void)Channel; }
	void SetCollisionResponseToAllChannels(ECollisionResponse Response) { (void)Response; }
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response)
	{
		(void)Channel;
		(void)Response;
	}
	void SetCollisionProfileName(FName Name) { (void)Name; }
	void SetSimulatePhysics(bool bSimulate) { (void)bSimulate; }
	void SetEnableGravity(bool bEnabled) { (void)bEnabled; }
	void SetMassOverrideInKg(FName Bone, float Mass, bool bOverride = true)
	{
		(void)Bone;
		(void)Mass;
		(void)bOverride;
	}
	void AddForce(const FVector& Force, FName Bone = FName(), bool bAccelChange = false)
	{
		(void)Force;
		(void)Bone;
		(void)bAccelChange;
	}
	void AddImpulse(const FVector& Impulse, FName Bone = FName(), bool bVelChange = false)
	{
		(void)Impulse;
		(void)Bone;
		(void)bVelChange;
	}
	void SetPhysicsLinearVelocity(const FVector& Velocity, bool bAdd = false)
	{
		(void)Velocity;
		(void)bAdd;
	}
	FVector GetPhysicsLinearVelocity() const { return FVector(); }
	void SetMaterial(int32 Index, UMaterialInterface* Material) { (void)Index; (void)Material; }
	UMaterialInterface* GetMaterial(int32 Index) const { (void)Index; return nullptr; }
	void SetCastShadow(bool bCast) { (void)bCast; }
	bool bUseAsOccluder = false;
	void SetBoundsScale(float Value) { (void)Value; }
	bool bCastDynamicShadow = true;
	bool bReceivesDecals = true;
	void SetGenerateOverlapEvents(bool bGenerate) { (void)bGenerate; }
	FBox GetLocalBounds() const { return FBox(); }

	bool bUseAttachParentBound = false;
};

class AActor : public UObject
{
public:
	AActor() = default;

	virtual void BeginPlay() {}
	virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
	virtual void EndPlay(const uint8 Reason) { (void)Reason; }

	FVector GetActorLocation() const { return StubLocation; }
	FRotator GetActorRotation() const { return StubRotation; }
	FQuat GetActorQuat() const { return StubRotation.Quaternion(); }
	FTransform GetActorTransform() const { return FTransform(StubRotation, StubLocation); }
	bool SetActorLocation(const FVector& NewLocation, bool bSweep = false, FHitResult* OutHit = nullptr)
	{
		(void)bSweep;
		(void)OutHit;
		StubLocation = NewLocation;
		return true;
	}
	void SetActorRotation(const FRotator& NewRotation) { StubRotation = NewRotation; }
	void SetActorRotation(const FQuat& NewRotation) { StubRotation = NewRotation.Rotator(); }
	bool SetActorLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation,
		bool bSweep = false, FHitResult* OutHit = nullptr)
	{
		(void)bSweep;
		(void)OutHit;
		StubLocation = NewLocation;
		StubRotation = NewRotation;
		return true;
	}
	bool SetActorLocationAndRotation(const FVector& NewLocation, const FQuat& NewRotation,
		bool bSweep = false, FHitResult* OutHit = nullptr)
	{
		(void)bSweep;
		(void)OutHit;
		StubLocation = NewLocation;
		StubRotation = NewRotation.Rotator();
		return true;
	}
	void SetActorTransform(const FTransform& Transform) { (void)Transform; }
	void AddActorWorldOffset(const FVector& Delta, bool bSweep = false, FHitResult* OutHit = nullptr)
	{
		(void)Delta;
		(void)bSweep;
		(void)OutHit;
	}
	void SetActorHiddenInGame(bool bHidden) { (void)bHidden; }
	bool IsHidden() const { return false; }
	void SetActorEnableCollision(bool bEnabled) { (void)bEnabled; }
	void SetActorTickEnabled(bool bEnabled) { (void)bEnabled; }
	void Destroy() {}
	bool IsActorBeingDestroyed() const { return false; }
	FVector GetActorForwardVector() const;
	FVector GetActorRightVector() const;
	FVector GetActorUpVector() const;
	USceneComponent* GetRootComponent() const { return RootComponent; }
	void SetRootComponent(USceneComponent* Component) { RootComponent = Component; }
	UWorld* GetWorld() const;
	class FTimerManager& GetWorldTimerManager() const;
	AActor* GetOwner() const { return StubOwner; }
	void SetOwner(AActor* InOwner) { StubOwner = InOwner; }
	FString GetActorNameOrLabel() const { return FString(TEXT("Actor")); }
	void AttachToActor(AActor* Parent, const FAttachmentTransformRules& Rules)
	{
		(void)Parent;
		(void)Rules;
	}

	template <typename T>
	T* CreateDefaultSubobject(const TCHAR* Name)
	{
		(void)Name;
		return nullptr;
	}
	template <typename T>
	T* FindComponentByClass() const { return nullptr; }
	template <typename T>
	bool IsA() const { return false; }
	bool IsA(UClass* Class) const { (void)Class; return false; }
	bool ActorHasTag(FName Tag) const { (void)Tag; return false; }
	/** Both forms the real AActor offers: the plain one, and the templated downcast. */
	class UGameInstance* GetGameInstance() const;
	template <typename T>
	T* GetGameInstance() const { return nullptr; }

	/** Gameplay tags on the actor itself; the traffic director marks its pedestrians with these. */
	TArray<FName> Tags;
	USceneComponent* RootComponent = nullptr;

private:
	// Not fields on the real AActor -- the transform lives on the root component. Kept private and
	// oddly named so they cannot be mistaken for engine API, and so no local shadows them.
	FVector StubLocation;
	FRotator StubRotation;
	AActor* StubOwner = nullptr;

public:
	struct FActorTickFunction
	{
		bool bCanEverTick = true;
		bool bStartWithTickEnabled = true;
		float TickInterval = 0.0f;
		ETickingGroup TickGroup = TG_PrePhysics;
	} PrimaryActorTick;
};
