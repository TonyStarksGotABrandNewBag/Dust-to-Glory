// What a generated surface is made of.
//
// Geometry is still generated from the simulation's numbers (see DTGMeshBuilder), but each piece is now
// tagged with a surface kind and drawn with that kind's material from /Game/Art/Materials -- photo-scanned
// textures projected from world space, so the generated meshes need no UVs or tangents. The vertex colour
// is kept: it tints the surfaces that vary building to building, and it is the whole look if the art is
// missing, because every kind falls back to the plain vertex-colour material rather than to nothing.
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

enum class EDTGSurface : uint8
{
	Asphalt,
	Sidewalk,
	Concrete,
	Brick,
	Stucco,
	WoodSiding,
	ShingleRoof,
	MetalRoof,
	Grass,
	Sand,
	Dirt,
	ForestFloor,
	PineBark,
	PalmBark,
	Bark,
	Matte,
	Glass,
	Window, // building glass: reflective, with a room mapped behind it
	Chrome,
	Rubber,
	Skin,
	Cloth,
	Leaves,
	Water,
	Paint,
	Terrain,
	Count
};

inline constexpr int32 DTGSurfaceCount = static_cast<int32>(EDTGSurface::Count);

namespace DTGSurface
{
	/** The kind's name, which is also its material's: MI_<Name>. */
	const TCHAR* Name(EDTGSurface Kind);

	/**
	 * The material a surface of this kind is drawn with. Loaded once and kept. Falls back to the engine's
	 * vertex-colour material, warning once per kind, if the art has not been imported -- a clone without
	 * its LFS files still runs and still reads, just untextured.
	 */
	UMaterialInterface* Material(EDTGSurface Kind);
}
