// Builds the visible world from the simulation's plan.
//
// The state's terrain is a height field (StatePlan::SurfaceAt), so the ground is streamed as procedural mesh
// chunks around whoever is playing: a chunk is built once, kept while it is near, and thrown away when it is
// far. Everything that doesn't move -- town streets, highway ribbons with their bridge decks, building shells,
// venue slabs, water -- is placed once at startup as instanced meshes, which is what lets a whole state exist
// at the same time as a full-size city.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DTGWorldBuilder.generated.h"

class UDTGGameSubsystem;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;

/** One streamed square of ground. Plain data: it holds nothing the garbage collector needs to see. */
struct FDTGTerrainChunk
{
	int32 X = 0;
	int32 Y = 0;
	int32 Section = -1;
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
	void UpdateChunks(const FVector& Around);
	void BuildChunk(int32 ChunkX, int32 ChunkY);
	void ReleaseChunk(int32 Index);
	/** A flat, axis-aligned slab (streets, parking, venue aprons). */
	void AddSlab(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& Uvs,
		TArray<FLinearColor>& Colors, double MinX, double MinY, double MaxX, double MaxY, double Z, const FLinearColor& Color);
	UMaterialInterface* TintedMaterial(const FLinearColor& Color, const TCHAR* SlotName);

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Terrain;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Surfaces; // streets, highways, venue slabs: built once

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Water;

	UPROPERTY()
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> BuildingMeshes; // one per lot role, tinted

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInterface>> Materials;

	TArray<FDTGTerrainChunk> Chunks;
	TArray<FIntPoint> Pending;
	TArray<int32> FreeSections;
	int32 NextSection = 0;
	bool bBuilt = false;
};
