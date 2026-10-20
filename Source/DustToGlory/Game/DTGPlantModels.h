// Real tree and plant models for the simulation's plants.
//
// Sim/Vegetation decides what grows where, and is tested for it; this only decides which imported model
// draws each species, and how to scale, spin and lean it to the plant the simulation asked for. The
// models are CC0 Poly Haven scans (Tools/Art/fetch_models.py, then import_models.py). If they have not
// been imported, Ready() is false and the world builder falls back to the generated plant geometry.
#pragma once

#include "CoreMinimal.h"

#include "Sim/Vegetation.h"

class UStaticMesh;

namespace DTGPlantModels
{
	/** How many distinct models there are. A streamed chunk keeps one instanced component per model. */
	int32 Count();

	/** The model at Index, or null if it has not been imported. Loaded once. */
	UStaticMesh* Mesh(int32 Index);

	/** True when every species has at least one model to draw it with. */
	bool Ready();

	/** Which model draws this plant -- the same one every time the chunk is built -- or INDEX_NONE. */
	int32 ModelFor(const dtg::sim::Plant& Value);

	/** Puts Model at the plant's spot, standing on the ground, at its height, spin and lean. Unreal units. */
	FTransform TransformFor(const dtg::sim::Plant& Value, int32 Model);
}
