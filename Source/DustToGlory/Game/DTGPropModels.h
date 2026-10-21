// Real models for the simulation's street furniture.
//
// Sim/StreetFurniture decides what stands where, and is tested for it; this only decides which
// imported meshes draw each kind and how they sit. A prop can be several meshes: Poly Haven's utility
// poles are a modular kit, so a pole is assembled from one of the kit's presets -- pole, crossarm,
// insulators, fuses, a transformer on some, a guy wire to the ground -- placed together as the kit
// laid them out. The models are CC0 Poly Haven scans (Tools/Art/fetch_models.py, import_models.py);
// without them, Ready() is false and no furniture is drawn.
#pragma once

#include "CoreMinimal.h"

#include "Sim/StreetFurniture.h"

class UStaticMesh;

namespace DTGPropModels
{
	/** One mesh of a prop and where it sits relative to the prop's own spot and facing. */
	struct FPart
	{
		int32 Mesh = INDEX_NONE;
		FTransform Local;
	};

	/** How many distinct meshes there are; a streamed chunk keeps one instanced component for each. */
	int32 MeshCount();

	/** The mesh at Index, or null if it has not been imported. Loaded once. */
	UStaticMesh* Mesh(int32 Index);

	/** True when every kind of prop has a model. */
	bool Ready();

	/** The meshes that draw a prop of this kind and variant. Empty if the model is missing. */
	const TArray<FPart>& PartsFor(dtg::sim::PropKind Kind, int32 Variant);

	/** The prop's own transform: its spot, standing on its surface, facing as the model needs. Unreal units. */
	FTransform TransformFor(const dtg::sim::Prop& Value);

	/** A barrier model's length in metres, so the simulation lays them end to end without gaps. */
	/** The curb kit: a piece by variant, and its length in metres, so the world builder can lay them end to end along a kerb. */
	int32 CurbMesh(int32 Variant);
	double CurbLengthM(int32 Mesh);

	double BarrierLengthM();

	/** The chain-link fence panel (Poly Haven), and its length in metres, laid end to end along a lot line. */
	int32 FenceMesh();
	double FenceLengthM();
}
