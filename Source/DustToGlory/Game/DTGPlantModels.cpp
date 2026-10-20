#include "Game/DTGPlantModels.h"

#include "DustToGlory.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

#include <cstdint>

#include "Game/DTGGameSubsystem.h"

using namespace dtg::sim;

namespace detail_plantmodels
{
	/**
	 * The models, in the order their instanced components are kept. The Poly Haven roles live at
	 * /Game/Art/Models/<Role>/SM_<Role> (Tools/Art/fetch_models.py); the Megascans at their own paths.
	 */
	struct FModel
	{
		const TCHAR* Role;
		const TCHAR* Path; // null: the Poly Haven layout
	};
	const TCHAR* const Megascans = TEXT("/Game/Fab/Megascans/Plants");
	const FModel Models[] = {
		{TEXT("Pine"), nullptr}, {TEXT("Fir"), nullptr}, {TEXT("Jacaranda"), nullptr}, {TEXT("SmallTree"), nullptr},
		{TEXT("Palm1"), nullptr}, {TEXT("Palm2"), nullptr}, {TEXT("Palm3"), nullptr}, {TEXT("Fern"), nullptr},
		{TEXT("Shrub1"), nullptr}, {TEXT("Shrub2"), nullptr}, {TEXT("Shrub3"), nullptr}, {TEXT("Shrub4"), nullptr},
		{TEXT("Bermuda"), nullptr}, {TEXT("GrassA"), nullptr}, {TEXT("GrassB"), nullptr},
		// 15-18: Alexandra palm, a metre and a half tall as scanned: the palmettos.
		{TEXT("PalmShrubA"), TEXT("Alexandra_Palm_ukgkcilia/Raw/ukgkcilia_tier_0/StaticMeshes/SM_ukgkcilia_VarA")},
		{TEXT("PalmShrubB"), TEXT("Alexandra_Palm_ukgkcilia/Raw/ukgkcilia_tier_0/StaticMeshes/SM_ukgkcilia_VarB")},
		{TEXT("PalmShrubC"), TEXT("Alexandra_Palm_ukgkcilia/Raw/ukgkcilia_tier_0/StaticMeshes/SM_ukgkcilia_VarC")},
		{TEXT("PalmShrubD"), TEXT("Alexandra_Palm_ukgkcilia/Raw/ukgkcilia_tier_0/StaticMeshes/SM_ukgkcilia_VarD")},
		// 19-26: Kikuyu, the lawn grass, in patches from a hand's width to a metre.
		{TEXT("LawnA"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarA")},
		{TEXT("LawnB"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarB")},
		{TEXT("LawnC"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarC")},
		{TEXT("LawnD"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarD")},
		{TEXT("LawnE"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarE")},
		{TEXT("LawnF"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarF")},
		{TEXT("LawnG"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarG")},
		{TEXT("LawnH"), TEXT("Kikuyu_Grass_wiljearqx/Raw/wiljearqx_tier_0/StaticMeshes/SM_wiljearqx_VarH")},
		// 27-30: wild grass tufts, the odd one in a lawn nobody mows to the edge.
		{TEXT("WildA"), TEXT("Wild_Grass_vlkhcbxia/Raw/vlkhcbxia_tier_0/StaticMeshes/SM_vlkhcbxia_VarA")},
		{TEXT("WildB"), TEXT("Wild_Grass_vlkhcbxia/Raw/vlkhcbxia_tier_0/StaticMeshes/SM_vlkhcbxia_VarB")},
		{TEXT("WildC"), TEXT("Wild_Grass_vlkhcbxia/Raw/vlkhcbxia_tier_0/StaticMeshes/SM_vlkhcbxia_VarC")},
		{TEXT("WildD"), TEXT("Wild_Grass_vlkhcbxia/Raw/vlkhcbxia_tier_0/StaticMeshes/SM_vlkhcbxia_VarD")},
	};
	constexpr int32 RoleCount = static_cast<int32>(sizeof(Models) / sizeof(Models[0]));

	FString PathOf(int32 Index)
	{
		const FModel& Model = Models[Index];
		if (Model.Path)
		{
			const FString Leaf = FString(Model.Path).RightChop(FString(Model.Path).Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1);
			return FString::Printf(TEXT("%s/%s.%s"), Megascans, Model.Path, *Leaf);
		}
		return FString::Printf(TEXT("/Game/Art/Models/%s/SM_%s.SM_%s"), Model.Role, Model.Role, Model.Role);
	}

	/**
	 * The models that may draw each species, by index into Roles. A role listed twice is picked twice as
	 * often: a pine stand is mostly pine, with the odd fir among it for a less regular skyline.
	 */
	struct FVariants
	{
		int32 Models[4];
		int32 Num;
	};
	FVariants VariantsFor(PlantKind Kind)
	{
		switch (Kind)
		{
		case PlantKind::Pine: return {{0, 0, 0, 1}, 4};
		case PlantKind::Oak: return {{2, 3, 0, 0}, 2};      // spreading crowns stand in for live oak
		case PlantKind::Palm: return {{4, 5, 6, 0}, 3};
		case PlantKind::Palmetto: return {{15, 16, 17, 18}, 4}; // the Megascans palm shrubs; the fern stands in without them
		case PlantKind::Cypress: return {{1, 0, 0, 0}, 1};  // a narrow conifer for the bald cypress
		case PlantKind::Scrub: return {{8, 9, 10, 11}, 4};
		case PlantKind::Count: break;
		}
		return {{8, 0, 0, 0}, 1};
	}

	/** The palmetto's stand-in when no palm shrub is imported: low fronds and a rounded shrub. */
	FVariants PalmettoFallback()
	{
		return {{7, 9, 0, 0}, 2};
	}

	/** Stable per plant: from where it stands, never from which chunk asked or in what order. */
	uint32 HashOf(const Plant& Value)
	{
		const int64 Cx = static_cast<int64>(Value.X * 10.0);
		const int64 Cy = static_cast<int64>(Value.Y * 10.0);
		uint64 H = static_cast<uint64>(Cx) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64>(Cy) * 0xC2B2AE3D27D4EB4Full;
		H ^= H >> 29;
		return static_cast<uint32>(H);
	}
}

namespace DTGPlantModels
{
	int32 Count()
	{
		return detail_plantmodels::RoleCount;
	}

	int32 GrassModel(int32 Variant)
	{
		// Mostly the lawn grass patches, a few meadow tufts among them; whichever of these are imported.
		// The Bermuda scan is a 6 cm patch, too small to use.
		static const int32 Grasses[] = {19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 13, 14};
		constexpr int32 Count = static_cast<int32>(sizeof(Grasses) / sizeof(Grasses[0]));
		const int32 Start = FMath::Abs(Variant) % Count;
		for (int32 Step = 0; Step < Count; ++Step)
		{
			const int32 Pick = Grasses[(Start + Step) % Count];
			if (Mesh(Pick))
			{
				return Pick;
			}
		}
		return Grasses[Start];
	}

	UStaticMesh* Mesh(int32 Index)
	{
		using namespace detail_plantmodels;
		if (Index < 0 || Index >= RoleCount)
		{
			return nullptr;
		}
		static TWeakObjectPtr<UStaticMesh> Cached[RoleCount];
		static bool bTried[RoleCount] = {};
		if (!bTried[Index])
		{
			bTried[Index] = true;
			const FString Path = PathOf(Index);
			Cached[Index] = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
			if (!Cached[Index].IsValid())
			{
				UE_LOG(LogDTG, Warning, TEXT("No plant model at %s; Tools/Art/import_models.py imports the Poly Haven ones, the editor's Fab window the Megascans."), *Path);
			}
		}
		return Cached[Index].Get();
	}

	/** The options for a species, falling back for the palmetto when no palm shrub is imported. */
	static detail_plantmodels::FVariants OptionsFor(PlantKind Kind)
	{
		detail_plantmodels::FVariants Options = detail_plantmodels::VariantsFor(Kind);
		if (Kind == PlantKind::Palmetto)
		{
			bool bAny = false;
			for (int32 Index = 0; Index < Options.Num && !bAny; ++Index)
			{
				bAny = Mesh(Options.Models[Index]) != nullptr;
			}
			if (!bAny)
			{
				Options = detail_plantmodels::PalmettoFallback();
			}
		}
		return Options;
	}

	bool Ready()
	{
		for (int32 Kind = 0; Kind < static_cast<int32>(PlantKind::Count); ++Kind)
		{
			const detail_plantmodels::FVariants Options = OptionsFor(static_cast<PlantKind>(Kind));
			bool bAny = false;
			for (int32 Index = 0; Index < Options.Num && !bAny; ++Index)
			{
				bAny = Mesh(Options.Models[Index]) != nullptr;
			}
			if (!bAny)
			{
				return false;
			}
		}
		return true;
	}

	int32 ModelFor(const Plant& Value)
	{
		const detail_plantmodels::FVariants Options = OptionsFor(Value.Kind);
		const uint32 Pick = detail_plantmodels::HashOf(Value);
		// Start at the hashed choice and take the first one that was imported.
		for (int32 Step = 0; Step < Options.Num; ++Step)
		{
			const int32 Candidate = Options.Models[(Pick + static_cast<uint32>(Step)) % static_cast<uint32>(Options.Num)];
			if (Mesh(Candidate))
			{
				return Candidate;
			}
		}
		return INDEX_NONE;
	}

	FTransform TransformFor(const Plant& Value, int32 Model)
	{
		const UStaticMesh* const Source = Mesh(Model);
		const FBoxSphereBounds Bounds = Source ? Source->GetBounds() : FBoxSphereBounds();
		const double ModelHeight = FMath::Max(1.0, 2.0 * Bounds.BoxExtent.Z);
		const double Scale = FMath::Max(0.05, Value.HeightM * DTGMetresToUnreal / ModelHeight);
		const double ModelBase = Bounds.Origin.Z - Bounds.BoxExtent.Z;
		// Spin about its own axis, then lean toward LeanAim: tilting about the horizontal axis at right
		// angles to the aim carries the top of the plant toward it.
		const FVector LeanAxis(-FMath::Sin(Value.LeanAim), FMath::Cos(Value.LeanAim), 0.0);
		const FQuat Rotation = FQuat(LeanAxis, Value.Lean) * FQuat(FVector(0.0, 0.0, 1.0), Value.Spin);
		// Standing on the ground, sunk a hand's width so a slope never shows daylight under the roots. The
		// island trees were scanned with a mound of sand round their base, which a street tree would spread
		// over the sidewalk, so those go in deeper.
		const bool bMound = Model >= 4 && Model <= 6;
		const FVector At(Value.X * DTGMetresToUnreal, Value.Y * DTGMetresToUnreal,
			(Value.GroundM - (bMound ? 0.45 : 0.1)) * DTGMetresToUnreal - ModelBase * Scale);
		return FTransform(Rotation, At, FVector(Scale, Scale, Scale));
	}
}
