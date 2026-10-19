#include "Game/DTGSurface.h"

#include "DustToGlory.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

#include "Game/DTGMeshBuilder.h"

namespace DTGSurface
{
	const TCHAR* Name(EDTGSurface Kind)
	{
		switch (Kind)
		{
		case EDTGSurface::Asphalt: return TEXT("Asphalt");
		case EDTGSurface::Sidewalk: return TEXT("Sidewalk");
		case EDTGSurface::Concrete: return TEXT("Concrete");
		case EDTGSurface::Brick: return TEXT("Brick");
		case EDTGSurface::Stucco: return TEXT("Stucco");
		case EDTGSurface::WoodSiding: return TEXT("WoodSiding");
		case EDTGSurface::ShingleRoof: return TEXT("ShingleRoof");
		case EDTGSurface::MetalRoof: return TEXT("MetalRoof");
		case EDTGSurface::Grass: return TEXT("Grass");
		case EDTGSurface::Sand: return TEXT("Sand");
		case EDTGSurface::Dirt: return TEXT("Dirt");
		case EDTGSurface::ForestFloor: return TEXT("ForestFloor");
		case EDTGSurface::PineBark: return TEXT("PineBark");
		case EDTGSurface::PalmBark: return TEXT("PalmBark");
		case EDTGSurface::Bark: return TEXT("Bark");
		case EDTGSurface::Matte: return TEXT("Matte");
		case EDTGSurface::Glass: return TEXT("Glass");
		case EDTGSurface::Chrome: return TEXT("Chrome");
		case EDTGSurface::Rubber: return TEXT("Rubber");
		case EDTGSurface::Skin: return TEXT("Skin");
		case EDTGSurface::Cloth: return TEXT("Cloth");
		case EDTGSurface::Leaves: return TEXT("Leaves");
		case EDTGSurface::Water: return TEXT("Water");
		case EDTGSurface::Paint: return TEXT("Paint");
		case EDTGSurface::Terrain: return TEXT("Terrain");
		case EDTGSurface::Count: break;
		}
		return TEXT("Matte");
	}

	UMaterialInterface* Material(EDTGSurface Kind)
	{
		const int32 Index = static_cast<int32>(Kind);
		if (Index < 0 || Index >= DTGSurfaceCount)
		{
			return DTGMeshBuilder::VertexColorMaterial();
		}
		// Asked for by every section of every building, car and chunk, so each is looked up once.
		static TWeakObjectPtr<UMaterialInterface> Cached[DTGSurfaceCount];
		static bool bWarned[DTGSurfaceCount] = {};
		if (Cached[Index].IsValid())
		{
			return Cached[Index].Get();
		}
		const FString Path = FString::Printf(TEXT("/Game/Art/Materials/MI_%s.MI_%s"), Name(Kind), Name(Kind));
		UMaterialInterface* Found = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *Path));
		if (!Found)
		{
			if (!bWarned[Index])
			{
				bWarned[Index] = true;
				UE_LOG(LogDTG, Warning, TEXT("No material at %s; drawing %s untextured. Run Tools/Art/fetch_textures.py and "
					"build_materials.py, or pull the LFS files."), *Path, Name(Kind));
			}
			return DTGMeshBuilder::VertexColorMaterial();
		}
		Cached[Index] = Found;
		return Found;
	}
}
