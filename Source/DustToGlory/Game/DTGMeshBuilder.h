// Geometry built in code, because the repository has no art in it.
//
// Everything the player looks at is generated from the simulation's own numbers: a building from its
// footprint, floor count and role; a car from its body style and real dimensions. That keeps the download
// tiny and means a new make or a new town needs no asset work -- and when real art arrives, these are the
// shapes it replaces, at the right scale and in the right places.
//
// Each piece is tagged with what it is made of (DTGSurface) and drawn with that surface's material; the
// vertex colour rides along as a tint, and is the whole look if the art has not been imported.
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "Game/DTGSurface.h"
#include "Sim/CityLayout.h"
#include "Sim/Vehicles.h"

class AActor;
class UDTGBody;
class UMaterialInterface;

/** A mesh under construction, in Unreal units and world space. */
struct FDTGMesh
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> Uvs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	bool IsEmpty() const { return Verts.Num() == 0; }
	void Reset();
	/** Appends another mesh, offsetting its triangle indices. */
	void Append(const FDTGMesh& Other);

	/**
	 * One flat quad, wound so its face points along Normal. Corners go round the face, either way; the
	 * winding is corrected to match the normal given.
	 */
	void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal,
		const FLinearColor& Colour, double UvScale = 0.01);
	/** One triangle, wound to match the normal given. For fans and gables, where a quad would be half empty. */
	void AddTri(const FVector& A, const FVector& B, const FVector& C, const FVector& Normal, const FLinearColor& Colour);
	/** An axis-aligned box between two corners. */
	void AddBox(const FVector& Min, const FVector& Max, const FLinearColor& Colour, bool bSkipBottom = true);
	/**
	 * A box whose top face is inset and shifted, which is most of a car: a wedge, a tapered cabin, a hip roof.
	 * Insets are fractions of the box's own size.
	 */
	void AddTaperedBox(const FVector& Min, const FVector& Max, double InsetFrontX, double InsetBackX, double InsetY,
		const FLinearColor& Colour, bool bSkipBottom = true);
	/**
	 * A cylinder lying on its side along Y (a wheel), centred on Centre. The two faces go into RimOut when
	 * given, so tread and rim can be drawn with different materials; otherwise into this mesh.
	 */
	void AddWheel(const FVector& Centre, double RadiusUu, double WidthUu, const FLinearColor& Tyre, const FLinearColor& Rim, int32 Sides = 14,
		FDTGMesh* RimOut = nullptr);
	/**
	 * A simple pitched roof over a rectangle, ridge along the long axis. Without its gable ends when the
	 * caller builds those itself, in the wall, where a real house has them.
	 */
	void AddRoof(const FVector& Min, const FVector& Max, double RidgeHeightUu, const FLinearColor& Colour, bool bGableEnds = true);
};

/** Geometry sorted by what it is made of: one mesh per surface kind, each drawn with its own material. */
struct FDTGSurfaceMeshes
{
	FDTGMesh Parts[DTGSurfaceCount];

	FDTGMesh& operator[](EDTGSurface Kind) { return Parts[static_cast<int32>(Kind)]; }
	const FDTGMesh& operator[](EDTGSurface Kind) const { return Parts[static_cast<int32>(Kind)]; }
	bool IsEmpty() const;
	void Reset();
	/**
	 * Writes each non-empty kind as its own section of Target, from FirstSection up, with its material.
	 * Returns the next free section, so several builders can share one component.
	 */
	int32 Apply(UProceduralMeshComponent* Target, int32 FirstSection, bool bCollision) const;
	/** The section Apply gave Kind, or INDEX_NONE if Kind was empty. */
	int32 SectionOf(EDTGSurface Kind, int32 FirstSection) const;
};

namespace DTGMeshBuilder
{
	/** The wall, window, roof and trim colours a building of this role is built from. */
	struct FPalette
	{
		FLinearColor Wall;
		FLinearColor Window;
		FLinearColor Roof;
		FLinearColor Trim;
	};
	FPalette PaletteFor(const dtg::sim::BuildingPlan& Building);

	/** What a building's walls are made of: from its role, downtown or not, and its id for variety. */
	EDTGSurface WallSurfaceFor(const dtg::sim::BuildingPlan& Building);

	/** A building: walls, a band of windows per floor, a door on its own side, and a roof. */
	void BuildBuilding(const dtg::sim::BuildingPlan& Building, double GroundZ, double MetresToUnreal, FDTGSurfaceMeshes& Out);

	/** A car body with a cabin, glass, lights and a colour -- the silhouette of its body style. */
	void BuildCarBody(dtg::sim::BodyStyle Body, double LengthM, double WidthM, double HeightM, const FLinearColor& Paint, FDTGSurfaceMeshes& Out);

	/** One wheel, sized from the tire: rubber tread, metal rim. */
	void BuildWheel(double RadiusM, double WidthM, const FLinearColor& Rim, FDTGSurfaceMeshes& Out);

	/**
	 * A person as one static mesh: the shape without the walk. DressPerson builds the jointed version;
	 * this is for anything that just needs a body-shaped thing (and for checking the proportions offline).
	 */
	void BuildPerson(const FLinearColor& Clothes, const FLinearColor& Skin, FDTGMesh& Out);

	/**
	 * The inside of a building: floor, walls with the door cut out, a ceiling, and the fixtures its role
	 * implies (counters in a shop, pews in a church, beds in a home). Built around the origin.
	 */
	void BuildInterior(const dtg::sim::BuildingPlan& Building, double MetresToUnreal, FDTGMesh& Out);

	/** One piece of furniture the player bought, placed in their home. */
	void BuildFurniture(const FString& Category, const FVector& At, double YawDegrees, const FLinearColor& Colour, FDTGMesh& Out);

	/**
	 * Gives an actor a body that walks (see DTGBody). Seed picks their clothes, so the same person looks
	 * the same each time; 1 and 2 are reserved for a hero and a villain, who are supposed to stand out.
	 */
	UDTGBody* DressPerson(AActor* Person, int32 Seed);

	/**
	 * The material every generated mesh is drawn with: the engine's own vertex-colour material, which
	 * turns the colours baked into the geometry into what you see, with no textures anywhere.
	 *
	 * Loaded by path, because nothing in the project references it as an asset -- which is exactly why
	 * Config/DefaultGame.ini has to tell the cooker to keep it. Found once and kept; warns once if it
	 * is missing, because the symptom otherwise is a whole world of untextured grey with no clue why.
	 */
	UMaterialInterface* VertexColorMaterial();
}
