#include "Game/DTGPropModels.h"

#include "DustToGlory.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

#include "Game/DTGGameSubsystem.h"

using namespace dtg::sim;

namespace detail_propmodels
{
	const TCHAR* const KitDir = TEXT("/Game/Art/Models/UtilityPoles/modular_electricity_poles_2k/StaticMeshes/");

	/** Every mesh furniture uses, by asset path. Kit parts first, then the single-mesh props. */
	const TCHAR* const Paths[] = {
		// Preset 1: a pole with a transformer.
		TEXT("preset_01_pole"), TEXT("preset_01_cap"), TEXT("preset_01_connection_large_02"), TEXT("preset_01_connection_small_02"),
		TEXT("preset_01_connection_single_01"), TEXT("preset_01_fuse_01"), TEXT("preset_01_fuse_02"), TEXT("preset_01_transformer_01"),
		TEXT("preset_01_transformer_02"), TEXT("preset_01_fastener"), TEXT("preset_01_fastener_loop"), TEXT("preset_01_fastener_connection"),
		// Preset 2: a plain line pole.
		TEXT("preset_02_pole"), TEXT("preset_02_cap"), TEXT("preset_02_connection_large_01"), TEXT("preset_02_connection_small_01"),
		TEXT("preset_02_connection_single_01"), TEXT("preset_02_connection_single_01_02"),
		// Single meshes.
		TEXT("/Game/Art/Models/StreetLamp1/SM_StreetLamp1"), TEXT("/Game/Art/Models/Hydrant/SM_Hydrant"),
		TEXT("/Game/Art/Models/Barrier1/SM_Barrier1"),
		// Megascans, added through the editor's Fab window (docs/HANDOFF.md): a trash can, and a curb kit of six.
		TEXT("/Game/Fab/Megascans/3D/Trash_Can_ucynedrfa/Raw/SM_ucynedrfa_tier_0/StaticMeshes/SM_ucynedrfa_tier_0"),
		TEXT("/Game/Fab/Megascans/3D/Modular_Curb_Kit_xbttdihdw/Raw/xbttdihdw_tier_0/StaticMeshes/xbttdihdw_LOD0_TIER0_000"),
		TEXT("/Game/Fab/Megascans/3D/Modular_Curb_Kit_xbttdihdw/Raw/xbttdihdw_tier_0/StaticMeshes/xbttdihdw_LOD0_TIER0_001"),
		TEXT("/Game/Fab/Megascans/3D/Modular_Curb_Kit_xbttdihdw/Raw/xbttdihdw_tier_0/StaticMeshes/xbttdihdw_LOD0_TIER0_002"),
		TEXT("/Game/Fab/Megascans/3D/Modular_Curb_Kit_xbttdihdw/Raw/xbttdihdw_tier_0/StaticMeshes/xbttdihdw_LOD0_TIER0_003"),
		TEXT("/Game/Fab/Megascans/3D/Modular_Curb_Kit_xbttdihdw/Raw/xbttdihdw_tier_0/StaticMeshes/xbttdihdw_LOD0_TIER0_004"),
		TEXT("/Game/Fab/Megascans/3D/Modular_Curb_Kit_xbttdihdw/Raw/xbttdihdw_tier_0/StaticMeshes/xbttdihdw_LOD0_TIER0_005")};
	constexpr int32 PathCount = static_cast<int32>(sizeof(Paths) / sizeof(Paths[0]));
	constexpr int32 Preset1First = 0;
	constexpr int32 Preset1Count = 12;
	constexpr int32 Preset2First = 12;
	constexpr int32 Preset2Count = 6;
	constexpr int32 LampMesh = 18;
	constexpr int32 HydrantMesh = 19;
	constexpr int32 BarrierMesh = 20;
	constexpr int32 BinMesh = 21;
	constexpr int32 CurbFirst = 22;
	constexpr int32 CurbCount = 6;

	/** The kit builds its presets 6 m tall; Carolina distribution poles stand 10-12 m. */
	constexpr double PoleScale = 1.75;

	FString FullPath(int32 Index)
	{
		FString Path = Paths[Index];
		if (!Path.StartsWith(TEXT("/")))
		{
			Path = FString(KitDir) + Path;
		}
		const FString Name = FPaths::GetCleanFilename(Path);
		return Path + TEXT(".") + Name;
	}

	/** A kit preset: its parts, moved from where the kit lays it out to the prop's own origin, and scaled. */
	TArray<DTGPropModels::FPart> Preset(int32 First, int32 Count, double KitX)
	{
		TArray<DTGPropModels::FPart> Parts;
		for (int32 Index = First; Index < First + Count; ++Index)
		{
			if (DTGPropModels::Mesh(Index))
			{
				Parts.Add({Index, FTransform(FQuat::Identity, FVector(-KitX * PoleScale, 0.0, 0.0), FVector(PoleScale, PoleScale, PoleScale))});
			}
		}
		return Parts;
	}

	/** A single mesh, centred on the prop's spot with its base on the surface, however the model was authored. */
	TArray<DTGPropModels::FPart> Single(int32 Index)
	{
		TArray<DTGPropModels::FPart> Parts;
		if (const UStaticMesh* Source = DTGPropModels::Mesh(Index))
		{
			const FBoxSphereBounds Bounds = Source->GetBounds();
			const FVector Centre(-Bounds.Origin.X, -Bounds.Origin.Y, -(Bounds.Origin.Z - Bounds.BoxExtent.Z));
			Parts.Add({Index, FTransform(FQuat::Identity, Centre)});
		}
		return Parts;
	}
}

namespace DTGPropModels
{
	int32 MeshCount()
	{
		return detail_propmodels::PathCount;
	}

	UStaticMesh* Mesh(int32 Index)
	{
		using namespace detail_propmodels;
		if (Index < 0 || Index >= PathCount)
		{
			return nullptr;
		}
		static TWeakObjectPtr<UStaticMesh> Cached[PathCount];
		static bool bTried[PathCount] = {};
		if (!bTried[Index])
		{
			bTried[Index] = true;
			const FString Path = FullPath(Index);
			Cached[Index] = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path));
			if (!Cached[Index].IsValid())
			{
				UE_LOG(LogDTG, Warning, TEXT("No furniture model at %s; %s."), *Path,
					Path.StartsWith(TEXT("/Game/Fab/")) ? TEXT("it comes in through the editor's Fab window (docs/HANDOFF.md)") : TEXT("Tools/Art/import_models.py imports it"));
			}
		}
		return Cached[Index].Get();
	}

	const TArray<FPart>& PartsFor(PropKind Kind, int32 Variant)
	{
		using namespace detail_propmodels;
		static const TArray<FPart> PoleWithTransformer = Preset(Preset1First, Preset1Count, -250.0);
		static const TArray<FPart> PlainPole = Preset(Preset2First, Preset2Count, -450.0);
		static const TArray<FPart> Lamp = Single(LampMesh);
		static const TArray<FPart> Hydrant = Single(HydrantMesh);
		static const TArray<FPart> Barrier = Single(BarrierMesh);
		static const TArray<FPart> Bin = Single(BinMesh);
		static const TArray<FPart> None;
		switch (Kind)
		{
		case PropKind::UtilityPole: return Variant == 0 ? PoleWithTransformer : PlainPole; // one pole in four carries a transformer
		case PropKind::StreetLamp: return Lamp;
		case PropKind::Hydrant: return Hydrant;
		case PropKind::Barrier: return Barrier;
		case PropKind::Bin: return Bin; // nothing drawn without the Megascans; Ready() does not insist on it
		case PropKind::Count: break;
		}
		return None;
	}

	bool Ready()
	{
		// Only what these models draw. Street trees and grass are drawn with the plant models, and a new
		// kind added to the enum must not quietly switch every pole and hydrant off, as two have before.
		static const PropKind Drawn[] = {PropKind::UtilityPole, PropKind::StreetLamp, PropKind::Hydrant, PropKind::Barrier};
		for (const PropKind Kind : Drawn)
		{
			if (PartsFor(Kind, 1).IsEmpty())
			{
				return false;
			}
		}
		return true;
	}

	FTransform TransformFor(const Prop& Value)
	{
		// The pole kit's crossarm runs along its X, and the lines it carries follow the road, so the arm
		// turns square to the road. A barrier's length is its X, laid along the road. The lamp's head
		// faces its X toward the street. A hydrant just turns by its variant, so a row of them differ.
		double YawDegrees = FMath::RadiansToDegrees(Value.Yaw);
		switch (Value.Kind)
		{
		case PropKind::UtilityPole: YawDegrees += 90.0; break;
		case PropKind::Hydrant: YawDegrees = 90.0 * Value.Variant; break;
		default: break;
		}
		const FVector At(Value.X * DTGMetresToUnreal, Value.Y * DTGMetresToUnreal, Value.Z * DTGMetresToUnreal);
		return FTransform(FRotator(0.0, YawDegrees, 0.0), At);
	}

	int32 CurbMesh(int32 Variant)
	{
		return detail_propmodels::CurbFirst + FMath::Abs(Variant) % detail_propmodels::CurbCount;
	}

	double CurbLengthM(int32 Mesh)
	{
		const UStaticMesh* Source = DTGPropModels::Mesh(Mesh);
		return Source ? FMath::Max(0.2, 2.0 * Source->GetBounds().BoxExtent.X / DTGMetresToUnreal) : 0.7;
	}

	double BarrierLengthM()
	{
		const UStaticMesh* Source = Mesh(detail_propmodels::BarrierMesh);
		return Source ? FMath::Max(0.5, 2.0 * Source->GetBounds().BoxExtent.X / DTGMetresToUnreal) : 3.0;
	}
}
