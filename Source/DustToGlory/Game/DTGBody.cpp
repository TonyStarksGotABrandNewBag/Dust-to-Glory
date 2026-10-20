#include "Game/DTGBody.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UObjectGlobals.h"

#include "DustToGlory.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

#include "Game/DTGMeshBuilder.h"

namespace
{
	/** The clothes and skin a seed picks out. */
	void PaletteFor(int32 Seed, FLinearColor& OutClothes, FLinearColor& OutSkin)
	{
		static const FLinearColor Wardrobe[] = {
			FLinearColor(0.16f, 0.2f, 0.3f), FLinearColor(0.3f, 0.12f, 0.12f), FLinearColor(0.15f, 0.26f, 0.18f),
			FLinearColor(0.35f, 0.33f, 0.28f), FLinearColor(0.1f, 0.1f, 0.12f), FLinearColor(0.42f, 0.38f, 0.2f),
		};
		static const FLinearColor Skins[] = {
			FLinearColor(0.72f, 0.56f, 0.44f), FLinearColor(0.5f, 0.36f, 0.26f), FLinearColor(0.32f, 0.22f, 0.16f),
			FLinearColor(0.85f, 0.7f, 0.58f),
		};
		OutClothes = Wardrobe[FMath::Abs(Seed) % UE_ARRAY_COUNT(Wardrobe)];
		if (Seed == 1)
		{
			OutClothes = FLinearColor(0.05f, 0.25f, 0.6f);   // a hero
		}
		else if (Seed == 2)
		{
			OutClothes = FLinearColor(0.32f, 0.04f, 0.3f);   // a villain
		}
		OutSkin = Skins[FMath::Abs(Seed / 7) % UE_ARRAY_COUNT(Skins)];
	}

}

UDTGBody::UDTGBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

namespace detail_body
{
	UAnimSequence* LoadClip(const TCHAR* Path)
	{
		return Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, Path));
	}
}

void UDTGBody::Build(int32 Seed)
{
	// Epic's mannequins, Manny or Quinn by the seed, with their unarmed clips. They stand with their feet
	// at the capsule's bottom and face +Y as authored, so the figure sits a half-height down and turns to
	// face the actor's forward. Missing (the assets are copied in by Tools/fetch_mannequins.py, not
	// committed), the generated box figure below stands in.
	const TCHAR* const MeshPath = (Seed % 2 == 0) ? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")
												  : TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
	if (USkeletalMesh* Mannequin = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, MeshPath)))
	{
		IdleAnim = detail_body::LoadClip(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
		WalkAnim = detail_body::LoadClip(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
		JogAnim = detail_body::LoadClip(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));
		SwingAnim = detail_body::LoadClip(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
		if (IdleAnim && WalkAnim && JogAnim)
		{
			if (Figure)
			{
				Figure->DestroyComponent();
			}
			Figure = NewObject<USkeletalMeshComponent>(this, TEXT("Figure"));
			Figure->SetupAttachment(this);
			Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Figure->RegisterComponent();
			Figure->SetSkeletalMesh(Mannequin);
			Figure->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -90.0), FRotator(0.0, -90.0, 0.0));
			Figure->PlayAnimation(IdleAnim, true);
			if (UAnimInstance* Anim = Figure->GetAnimInstance())
			{
				// The walk and jog clips carry root motion; played as clips it would walk the figure off its capsule.
				Anim->RootMotionMode = ERootMotionMode::IgnoreRootMotion;
			}
			Playing = 0;
			LastLocation = GetComponentLocation();
			return;
		}
	}

	FLinearColor Clothes;
	FLinearColor Skin;
	PaletteFor(Seed, Clothes, Skin);
	// Whatever was built before has to go, or a rebuilt body leaves its old limbs standing.
	for (const TObjectPtr<UProceduralMeshComponent>& Old : Limbs)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	Limbs.Reset();
	UMaterialInterface* const ClothMaterial = DTGSurface::Material(EDTGSurface::Cloth);
	constexpr double S = 100.0;
	// Parts are built around their own pivot -- a hip for a leg, a shoulder for an arm -- so swinging one
	// is a rotation rather than a translation.
	const double HipZ = -0.05 * S;
	const double ShoulderZ = 0.47 * S;

	auto MakePart = [&](const TCHAR* Name, const FDTGMesh& Mesh, const FVector& At, UMaterialInterface* Material)
	{
		UProceduralMeshComponent* Part = NewObject<UProceduralMeshComponent>(this, Name);
		Part->SetupAttachment(this);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->RegisterComponent();
		Part->SetRelativeLocation(At);
		Part->CreateMeshSection_LinearColor(0, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors, Mesh.Tangents, false);
		Part->SetMaterial(0, Material);
		return Part;
	};

	FDTGMesh TorsoMesh;
	TorsoMesh.AddBox(FVector(-0.16 * S, -0.22 * S, 0.0), FVector(0.16 * S, 0.22 * S, 0.57 * S), Clothes, false);
	Torso = MakePart(TEXT("Torso"), TorsoMesh, FVector(0.0, 0.0, -0.05 * S), ClothMaterial);

	FDTGMesh HeadMesh;
	HeadMesh.AddBox(FVector(-0.11 * S, -0.11 * S, 0.0), FVector(0.11 * S, 0.11 * S, 0.24 * S), Skin, false);
	Head = MakePart(TEXT("Head"), HeadMesh, FVector(0.0, 0.0, 0.52 * S), DTGSurface::Material(EDTGSurface::Skin));

	const FLinearColor Trousers(Clothes.R * 0.55f, Clothes.G * 0.55f, Clothes.B * 0.6f);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const double Y = (Side == 0 ? -1.0 : 1.0) * 0.1 * S;
		FDTGMesh Leg;
		// Hanging down from the pivot at the hip.
		Leg.AddBox(FVector(-0.07 * S, -0.06 * S, -0.85 * S), FVector(0.07 * S, 0.06 * S, 0.0), Trousers, false);
		Limbs.Add(MakePart(*FString::Printf(TEXT("Leg%d"), Side), Leg, FVector(0.0, Y, HipZ), ClothMaterial));
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const double Y = (Side == 0 ? -1.0 : 1.0) * 0.25 * S;
		FDTGMesh Arm;
		Arm.AddBox(FVector(-0.05 * S, -0.05 * S, -0.48 * S), FVector(0.05 * S, 0.05 * S, 0.0), Clothes, false);
		Limbs.Add(MakePart(*FString::Printf(TEXT("Arm%d"), Side), Arm, FVector(0.0, Y, ShoulderZ), ClothMaterial));
	}
	LastLocation = GetComponentLocation();
}

void UDTGBody::Swing()
{
	Punching = 0.35f;
	if (Figure && SwingAnim)
	{
		Figure->PlayAnimation(SwingAnim, false);
		Figure->SetPlayRate(1.0f);
		Playing = 3;
		SwingLeft = SwingAnim->GetPlayLength();
	}
}

void UDTGBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (DeltaTime <= 0.0f)
	{
		return;
	}
	if (Figure)
	{
		// The mannequin: idle, walk or jog by how fast the actor is actually going, with the clip's rate
		// matched to the ground speed so the feet do not slide. A swing plays out before the pace resumes.
		const FVector Here = GetComponentLocation();
		const float Travelled = static_cast<float>(FVector::Dist2D(Here, LastLocation));
		LastLocation = Here;
		Speed = FMath::FInterpTo(Speed, Travelled / DeltaTime, DeltaTime, 8.0f);
		if (SwingLeft > 0.0f)
		{
			SwingLeft -= DeltaTime;
			return;
		}
		const int32 Wanted = Speed < 15.0f ? 0 : (Speed < 330.0f ? 1 : 2);
		if (Wanted != Playing)
		{
			Figure->PlayAnimation(Wanted == 0 ? IdleAnim : (Wanted == 1 ? WalkAnim : JogAnim), true);
			Playing = Wanted;
		}
		// The clips were captured at roughly a walk of 1.6 m/s and a jog of 4 m/s.
		Figure->SetPlayRate(Wanted == 0 ? 1.0f : FMath::Clamp(Speed / (Wanted == 1 ? 160.0f : 400.0f), 0.6f, 1.6f));
		return;
	}
	if (Limbs.Num() < 4)
	{
		return;
	}
	// Cadence from how fast they are actually going, so a stride matches the ground rather than a timer.
	const FVector Here = GetComponentLocation();
	const float Travelled = static_cast<float>(FVector::Dist2D(Here, LastLocation));
	LastLocation = Here;
	Speed = FMath::FInterpTo(Speed, Travelled / DeltaTime, DeltaTime, 8.0f);
	const float Stride = FMath::Clamp(Speed / FMath::Max(1.0f, WalkSpeed), 0.0f, 2.2f);
	Phase += DeltaTime * (2.2f + 3.4f * Stride) * (Stride > 0.05f ? 1.0f : 0.0f);

	// Legs alternate, arms counter them: the oldest trick in animation and still the readable one.
	const float Swing = FMath::Sin(Phase) * 38.0f * FMath::Min(1.0f, Stride);
	const float Lift = FMath::Abs(FMath::Cos(Phase)) * 3.0f * FMath::Min(1.0f, Stride);
	Limbs[0]->SetRelativeRotation(FRotator(Swing, 0.0f, 0.0f));
	Limbs[1]->SetRelativeRotation(FRotator(-Swing, 0.0f, 0.0f));

	if (Punching > 0.0f)
	{
		// A punch overrides the right arm until it has landed.
		Punching = FMath::Max(0.0f, Punching - DeltaTime);
		const float Through = 1.0f - Punching / 0.35f;
		Limbs[3]->SetRelativeRotation(FRotator(-100.0f * FMath::Sin(Through * UE_PI), 0.0f, 0.0f));
		Limbs[2]->SetRelativeRotation(FRotator(-Swing * 0.6f, 0.0f, 0.0f));
	}
	else
	{
		Limbs[2]->SetRelativeRotation(FRotator(-Swing * 0.6f, 0.0f, 0.0f));
		Limbs[3]->SetRelativeRotation(FRotator(Swing * 0.6f, 0.0f, 0.0f));
	}
	if (Torso)
	{
		// A small bob and lean, which is most of what sells a walk.
		Torso->SetRelativeLocation(FVector(0.0, 0.0, -0.05 * 100.0 + Lift));
		Torso->SetRelativeRotation(FRotator(FMath::Min(8.0f, Stride * 5.0f), 0.0f, 0.0f));
	}
	if (Head)
	{
		Head->SetRelativeLocation(FVector(0.0, 0.0, 0.52 * 100.0 + Lift));
	}
}
