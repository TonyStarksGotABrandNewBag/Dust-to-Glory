// Real tree and plant models for the simulation's plants.
//
// Sim/Vegetation decides what grows where, and is tested for it; this only decides which imported model
// draws each species, and how to scale, spin and lean it to the plant the simulation asked for. The
// models are CC0 Poly Haven scans (Tools/Art/fetch_models.py, then import_models.py), and Quixel
// Megascans added through the editor's Fab window (docs/HANDOFF.md): palm shrubs for the palmettos,
// and lawn and wild grasses for the ground cover. A model that is not there is skipped; if no species
// has a model at all, Ready() is false and the world builder falls back to the generated geometry.
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

	/** The grass clump model for a clump's variant: lawn and meadow grasses, whichever are imported. */
	int32 GrassModel(int32 Variant);

	/** Which model draws this plant -- the same one every time the chunk is built -- or INDEX_NONE. */
	int32 ModelFor(const dtg::sim::Plant& Value);

	/** Puts Model at the plant's spot, standing on the ground, at its height, spin and lean. Unreal units. */
	FTransform TransformFor(const dtg::sim::Plant& Value, int32 Model);
}
