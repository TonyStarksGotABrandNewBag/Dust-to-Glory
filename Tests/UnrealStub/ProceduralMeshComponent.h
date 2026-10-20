// UProceduralMeshComponent: the thing the whole world is built out of here.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

struct FProcMeshTangent
{
	FProcMeshTangent() = default;
	FProcMeshTangent(double X, double Y, double Z) : TangentX(X, Y, Z) {}
	FProcMeshTangent(const FVector& InTangent, bool bFlip = false) : TangentX(InTangent), bFlipTangentY(bFlip) {}
	FVector TangentX;
	bool bFlipTangentY = false;
};

struct FProcMeshSection
{
	bool bEnableCollision = false;
};

class UProceduralMeshComponent : public UPrimitiveComponent
{
public:
	/**
	 * NOTE: the real signature takes TArray<FVector2D> for UV0 and has UV1..UV3 overloads. Only the
	 * one the project calls is declared, so a call with the wrong argument order still fails here.
	 */
	void CreateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& Uvs,
		const TArray<FLinearColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents,
		bool bCreateCollision);
	void UpdateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices,
		const TArray<FVector>& Normals, const TArray<FVector2D>& Uvs,
		const TArray<FLinearColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents);
	void ClearMeshSection(int32 SectionIndex);
	void ClearAllMeshSections();
	int32 GetNumSections() const;
	void SetMeshSectionVisible(int32 SectionIndex, bool bVisible);

	bool bUseAsyncCooking = false;
	FProcMeshSection* GetProcMeshSection(int32 SectionIndex) { (void)SectionIndex; return nullptr; }
	void ClearCollisionConvexMeshes() {}
	bool bUseComplexAsSimpleCollision = true;
};
