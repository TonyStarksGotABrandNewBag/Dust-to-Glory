// Geometry built in code, because the repository has no art in it.
//
// Everything the player looks at is generated from the simulation's own numbers: a building from its
// footprint, floor count and role; a car from its body style and real dimensions. That keeps the download
// tiny and means a new make or a new town needs no asset work -- and when real art arrives, these are the
// shapes it replaces, at the right scale and in the right places.
//
// All of it is vertex-coloured and drawn with the engine's vertex-colour material, so a wall, a window and
// a roof are told apart without a single texture.
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "Sim/CityLayout.h"
#include "Sim/Vehicles.h"

class AActor;
class UDTGBody;

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
	/** An axis-aligned box between two corners. */
	void AddBox(const FVector& Min, const FVector& Max, const FLinearColor& Colour, bool bSkipBottom = true);
	/**
	 * A box whose top face is inset and shifted, which is most of a car: a wedge, a tapered cabin, a hip roof.
	 * Insets are fractions of the box's own size.
	 */
	void AddTaperedBox(const FVector& Min, const FVector& Max, double InsetFrontX, double InsetBackX, double InsetY,
		const FLinearColor& Colour, bool bSkipBottom = true);
	/** A cylinder lying on its side along Y (a wheel), centred on Centre. */
	void AddWheel(const FVector& Centre, double RadiusUu, double WidthUu, const FLinearColor& Tyre, const FLinearColor& Rim, int32 Sides = 14);
	/** A simple pitched roof over a rectangle. */
	void AddRoof(const FVector& Min, const FVector& Max, double RidgeHeightUu, const FLinearColor& Colour);
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

	/** A building: walls, a band of windows per floor, a door on its own side, and a roof. */
	void BuildBuilding(const dtg::sim::BuildingPlan& Building, double GroundZ, double MetresToUnreal, FDTGMesh& Out);

	/** A car body with a cabin, glass, lights and a colour -- the silhouette of its body style. */
	void BuildCarBody(dtg::sim::BodyStyle Body, double LengthM, double WidthM, double HeightM, const FLinearColor& Paint, FDTGMesh& Out);

	/** One wheel, sized from the tire. */
	void BuildWheel(double RadiusM, double WidthM, const FLinearColor& Rim, FDTGMesh& Out);

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
}
