#include "Game/DTGPlantModels.h"

#include "DustToGlory.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

#include <cstdint>

#include "Game/DTGGameSubsystem.h"

using namespace dtg::sim;

namespace detail_plantmodels
{
	/** The imported roles, in the order their instanced components are kept. See Tools/Art/fetch_models.py. */
	const TCHAR* const Roles[] = {
		TEXT("Pine"), TEXT("Fir"), TEXT("Jacaranda"), TEXT("SmallTree"), TEXT("Palm1"), TEXT("Palm2"), TEXT("Palm3"),
		TEXT("Fern"), TEXT("Shrub1"), TEXT("Shrub2"), TEXT("Shrub3"), TEXT("Shrub4")};
	constexpr int32 RoleCount = static_cast<int32>(sizeof(Roles) / sizeof(Roles[0]));

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
		case PlantKind::Palmetto: return {{7, 9, 0, 0}, 2}; // low fronds and a rounded shrub
		case PlantKind::Cypress: return {{1, 0, 0, 0}, 1};  // a narrow conifer for the bald cypress
		case PlantKind::Scrub: return {{8, 9, 10, 11}, 4};
		case PlantKind::Count: break;
		}
		return {{8, 0, 0, 0}, 1};
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
			const FString Path = FString::Printf(TEXT("/Game/Art/Models/%s/SM_%s.SM_%s"), Roles[Index], Roles[Index], Roles[Index]);
			Cached[Index] = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path));
			if (!Cached[Index].IsValid())
			{
				UE_LOG(LogDTG, Warning, TEXT("No plant model at %s; Tools/Art/import_models.py imports it."), *Path);
			}
		}
		return Cached[Index].Get();
	}

	bool Ready()
	{
		for (int32 Kind = 0; Kind < static_cast<int32>(PlantKind::Count); ++Kind)
		{
			const detail_plantmodels::FVariants Options = detail_plantmodels::VariantsFor(static_cast<PlantKind>(Kind));
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
		const detail_plantmodels::FVariants Options = detail_plantmodels::VariantsFor(Value.Kind);
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
		// Standing on the ground, sunk a hand's width so a slope never shows daylight under the roots.
		const FVector At(Value.X * DTGMetresToUnreal, Value.Y * DTGMetresToUnreal,
			(Value.GroundM - 0.1) * DTGMetresToUnreal - ModelBase * Scale);
		return FTransform(Rotation, At, FVector(Scale, Scale, Scale));
	}
}
