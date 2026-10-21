// Builds the visible world from the simulation's plan.
//
// The state's terrain is a height field (StatePlan::SurfaceAt), so the ground is streamed as procedural mesh
// chunks around whoever is playing: a chunk is built once, kept while it is near, and thrown away when it is
// far. Everything that doesn't move -- town streets, highway ribbons with their bridge decks, building shells,
// venue slabs, water -- is placed once at startup as instanced meshes, which is what lets a whole state exist
// at the same time as a full-size city.
#pragma once

#include "CoreMinimal.h"

#include "Sim/Lots.h"
#include "Sim/Vegetation.h"

#include <vector>
#include "GameFramework/Actor.h"

#include "Game/DTGMeshBuilder.h"

#include "DTGWorldBuilder.generated.h"

class UDTGGameSubsystem;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class ADTGSky;
class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * One streamed square of ground, and which of the terrain components is currently drawing it.
 *
 * Each chunk gets a component of its own rather than a section of one big one. A procedural mesh
 * component re-cooks the collision for *all* of its sections and recreates its whole scene proxy every
 * time any one section changes, so with one component a single arriving chunk paid for the entire
 * streamed world -- twice a frame, every frame, for as long as the player kept moving.
 *
 * Plain data: the components are held in a UPROPERTY array, which is what the collector follows.
 */
struct FDTGTerrainChunk
{
	int32 X = 0;
	int32 Y = 0;
	int32 Part = -1;      // index into TerrainParts
	double LastSeen = 0.0;
};

UCLASS()
class DUSTTOGLORY_API ADTGWorldBuilder : public AActor
{
	GENERATED_BODY()

public:
	ADTGWorldBuilder();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Builds the ground under a point and its neighbours right now, rather than waiting for the streamer.
	 * Used before the player is placed: a pawn spawned a metre above ground that has not arrived yet
	 * falls through the world while it waits.
	 */
	void EnsureGroundAt(const FVector& Where);
	/** Streets, highway decks and venue aprons: driven on, not driven into. */
	UProceduralMeshComponent* SurfacesComponent() const { return Surfaces; }
	/** Throws away everything built and builds it again: the state changed under us. */
	void Rebuild();

	/** Ground height (Unreal Z) under a world point, from the simulation's terrain, roads and water. */
	double GroundHeightAt(const FVector& World) const;
	/** True when the point is under open water (for swimming, drowning engines and boat-less bridges). */
	bool IsWaterAt(const FVector& World, double* OutSurfaceZ = nullptr) const;

	/** Metres of ground per terrain vertex. Smaller is prettier and slower. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Terrain")
	float VertexSpacingM = 3.0f;

	/** Side of one streamed chunk, in metres. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Terrain")
	float ChunkSizeM = 192.0f;

	/**
	 * Trees and brush on the streamed ground. Vegetation belongs to its chunk and is built and
	 * thrown away with it, so it needs no draw distance of its own -- a separate one would mean a
	 * chunk built while far away never growing its trees, because a chunk is never rebuilt in place.
	 */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Terrain")
	bool bPlantVegetation = true;

	/** How far from the player chunks are kept, in metres. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Terrain")
	float ViewDistanceM = 900.0f;

	/** Chunks built per frame; the rest wait their turn so a long drive never hitches. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Terrain")
	int32 ChunksPerFrame = 2;

private:
	void BuildStaticWorld();
	void BuildTownStreets();
	void BuildHighways();
	void BuildBuildings();
	void BuildVenues();
	void BuildWater();
	/** Cooks the building shells' collision once, after every section is in; per section it took six minutes. */
	void CookBuildingCollision();
	/** The paint on the highways and the town streets, from Sim/RoadMarkings. */
	void BuildRoadMarkings();
	/**
	 * Trees and scrub for one streamed chunk: instanced real models (DTGPlantModels) when they have been
	 * imported, otherwise generated geometry as a second section on that chunk's component.
	 */
	void BuildChunkPlants(int32 ChunkX, int32 ChunkY, int32 Part, UProceduralMeshComponent* Mesh, const std::vector<dtg::sim::Plant>& Grown);
	/**
	 * What the properties add round their buildings (Sim/Lots): drives, pads and walks as sections of the
	 * chunk's component, mailboxes, culvert headwalls and sheds as generated geometry, fences as instances.
	 */
	void BuildChunkLots(int32 ChunkX, int32 ChunkY, int32 Part, UProceduralMeshComponent* Mesh, const std::vector<dtg::sim::LotFeatures>& Lots);
	/** The instanced component drawing Model for terrain part Part, made on first use and kept with the part. */
	UInstancedStaticMeshComponent* PlantComponent(int32 Part, int32 Model);
	/** Empties and hides a part's plant instances, when its chunk is released. */
	void ClearPlants(int32 Part);
	/** Utility poles, street lamps, hydrants and deck barriers for one streamed chunk (Sim/StreetFurniture). */
	void BuildChunkProps(int32 ChunkX, int32 ChunkY, int32 Part);
	/** The instanced component drawing furniture mesh Mesh for terrain part Part, made on first use. */
	UInstancedStaticMeshComponent* PropComponent(int32 Part, int32 Mesh);
	/** Empties and hides a part's furniture, when its chunk is released. */
	void ClearProps(int32 Part);
	/** The lines between the utility poles, as a third section on the chunk's component (Sim/StreetFurniture WiresIn). */
	void BuildChunkWires(int32 ChunkX, int32 ChunkY, UProceduralMeshComponent* Mesh);
	void UpdateChunks(const FVector& Around);
	void BuildChunk(int32 ChunkX, int32 ChunkY);
	void ReleaseChunk(int32 Index);
	/** True when that square of ground is already built. */
	bool HasChunk(int32 ChunkX, int32 ChunkY) const;
	/** Finds the simulation subsystem if it has not been found yet. True when there is content to build. */
	bool EnsureGame();
	/** A spare terrain component from the pool, or a new one. */
	UProceduralMeshComponent* TakeTerrainPart(int32& OutIndex);
	/** A flat, axis-aligned slab (streets, parking, venue aprons). */
	void AddSlab(struct FDTGMesh& Mesh, double MinX, double MinY, double MaxX, double MaxY, double Z, const FLinearColor& Color);

	/** One component per live chunk, reused as the player moves. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> TerrainParts;

	/** Plant instances, one component per model per terrain part: index Part * DTGPlantModels::Count() + Model. */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PlantInstances;

	/** Furniture instances, one component per mesh per terrain part: index Part * DTGPropModels::MeshCount() + Mesh. */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PropInstances;

	/** One light at the head of every street lamp, with the terrain part it belongs to alongside. */
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> LampLights;
	TArray<int32> LampLightParts;

	/** How far up the lamps are, 0 by day to 1 at night, following the sky; set on the lights when it moves. */
	float LampLevel = 0.0f;

	UPROPERTY()
	TObjectPtr<ADTGSky> Sky;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Surfaces; // streets, highways, venue slabs: built once

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Water;

	/** Building shells: per town, one mesh section per surface kind. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Buildings;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	/** The next free section on Surfaces while the static world is being built. */
	int32 NextSurfaceSection = 0;

	TArray<FDTGTerrainChunk> Chunks;
	TArray<FIntPoint> Pending;
	TArray<int32> FreeParts;
	/** Chunk coordinates to index in Chunks, so "do I have this one" is not a scan of the whole ring. */
	TMap<FIntPoint, int32> ChunkAt;
	bool bBuilt = false;
};
