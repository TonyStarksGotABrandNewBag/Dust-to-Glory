#include "Game/DTGBody.h"

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

void UDTGBody::Build(int32 Seed)
{
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
	UMaterialInterface* const Material = DTGMeshBuilder::VertexColorMaterial();
	constexpr double S = 100.0;
	// Parts are built around their own pivot -- a hip for a leg, a shoulder for an arm -- so swinging one
	// is a rotation rather than a translation.
	const double HipZ = -0.05 * S;
	const double ShoulderZ = 0.47 * S;

	auto MakePart = [&](const TCHAR* Name, const FDTGMesh& Mesh, const FVector& At)
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
	Torso = MakePart(TEXT("Torso"), TorsoMesh, FVector(0.0, 0.0, -0.05 * S));

	FDTGMesh HeadMesh;
	HeadMesh.AddBox(FVector(-0.11 * S, -0.11 * S, 0.0), FVector(0.11 * S, 0.11 * S, 0.24 * S), Skin, false);
	Head = MakePart(TEXT("Head"), HeadMesh, FVector(0.0, 0.0, 0.52 * S));

	const FLinearColor Trousers(Clothes.R * 0.55f, Clothes.G * 0.55f, Clothes.B * 0.6f);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const double Y = (Side == 0 ? -1.0 : 1.0) * 0.1 * S;
		FDTGMesh Leg;
		// Hanging down from the pivot at the hip.
		Leg.AddBox(FVector(-0.07 * S, -0.06 * S, -0.85 * S), FVector(0.07 * S, 0.06 * S, 0.0), Trousers, false);
		Limbs.Add(MakePart(*FString::Printf(TEXT("Leg%d"), Side), Leg, FVector(0.0, Y, HipZ)));
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const double Y = (Side == 0 ? -1.0 : 1.0) * 0.25 * S;
		FDTGMesh Arm;
		Arm.AddBox(FVector(-0.05 * S, -0.05 * S, -0.48 * S), FVector(0.05 * S, 0.05 * S, 0.0), Clothes, false);
		Limbs.Add(MakePart(*FString::Printf(TEXT("Arm%d"), Side), Arm, FVector(0.0, Y, ShoulderZ)));
	}
	LastLocation = GetComponentLocation();
}

void UDTGBody::Swing()
{
	Punching = 0.35f;
}

void UDTGBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Limbs.Num() < 4 || DeltaTime <= 0.0f)
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
