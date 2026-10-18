#include "Game/DTGInterior.h"

#include "DustToGlory.h"
#include "Components/PointLightComponent.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

#include "Game/DTGGameSubsystem.h"
#include "Game/DTGMeshBuilder.h"

using namespace dtg::sim;

ADTGInterior::ADTGInterior()
{
	PrimaryActorTick.bCanEverTick = false;

	Room = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Room"));
	SetRootComponent(Room);
	Room->bUseAsyncCooking = false; // rooms are small and are needed the instant the door opens
	Room->SetCollisionProfileName(TEXT("BlockAll"));
}

void ADTGInterior::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	SetActorLocation(FVector(0.0, 0.0, PocketZ));
}

FVector ADTGInterior::Open(int32 InBuildingId)
{
	if (!Game || !Game->IsContentLoaded())
	{
		return FVector::ZeroVector;
	}
	const BuildingPlan* Building = Game->Sim().City.FindBuilding(InBuildingId);
	if (!Building)
	{
		return FVector::ZeroVector;
	}
	BuildingId = InBuildingId;
	Room->ClearAllMeshSections();

	FDTGMesh Mesh;
	DTGMeshBuilder::BuildInterior(*Building, DTGMetresToUnreal, Mesh);

	// A home is furnished with whatever the player has actually bought and placed.
	if (IsResidential(Building->Role))
	{
		const std::string PropertyId = Game->Sim().PropertyIdFor(Building->Id);
		const double HalfW = 0.5 * FMath::Max(4.0, Building->Footprint.Width() - 0.6) * DTGMetresToUnreal;
		const double HalfD = 0.5 * FMath::Max(4.0, Building->Footprint.Depth() - 0.6) * DTGMetresToUnreal;
		int32 Placed = 0;
		for (const PlacedFurniture& Piece : Game->Sim().Homes.Placed())
		{
			if (Piece.PropertyId != PropertyId)
			{
				continue;
			}
			const FurnitureItem* Item = Game->Sim().Homes.FindItem(Piece.ItemId);
			if (!Item)
			{
				continue;
			}
			// Placements are metres from the room's corner; the mesh is built around its middle.
			const FVector At(Piece.X * DTGMetresToUnreal - HalfW, Piece.Y * DTGMetresToUnreal - HalfD, 0.0);
			const FLinearColor Colour(FColor(static_cast<uint8>((Item->ColorRgb >> 16) & 0xFF),
				static_cast<uint8>((Item->ColorRgb >> 8) & 0xFF), static_cast<uint8>(Item->ColorRgb & 0xFF)));
			DTGMeshBuilder::BuildFurniture(UTF8_TO_TCHAR(Item->Category.c_str()), At, Piece.YawDeg, Colour, Mesh);
			++Placed;
		}
		if (Placed == 0)
		{
			// An empty home is still a home: a bed in the corner so it isn't a bare box.
			DTGMeshBuilder::BuildFurniture(TEXT("Bedroom"), FVector(-HalfW * 0.55, HalfD * 0.5, 0.0), 0.0,
				FLinearColor(0.3f, 0.25f, 0.22f), Mesh);
		}
	}

	Room->CreateMeshSection_LinearColor(0, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors, Mesh.Tangents, true);
	// A closed box five kilometres under the sky sees none of it, so the room brings its own light.
	if (!Lamp)
	{
		Lamp = NewObject<UPointLightComponent>(this);
		if (Lamp)
		{
			Lamp->SetupAttachment(GetRootComponent());
			Lamp->RegisterComponent();
			Lamp->SetMobility(EComponentMobility::Movable);
			Lamp->SetCastShadows(false);
		}
	}
	if (Lamp)
	{
		const double Ceiling = 2.6 * DTGMetresToUnreal;
		Lamp->SetRelativeLocation(FVector(0.0, 0.0, Ceiling));
		Lamp->SetAttenuationRadius(static_cast<float>(32.0 * DTGMetresToUnreal));
		Lamp->SetIntensity(14000.0f);
		Lamp->SetLightColor(FLinearColor(1.0f, 0.95f, 0.86f)); // warm, like a room light
	}
	if (UMaterialInterface* Painted = DTGMeshBuilder::VertexColorMaterial())
	{
		Room->SetMaterial(0, Painted);
	}

	// The door is in the middle of the -Y wall (see BuildInterior), so that is where you come in and go out.
	const double HalfDepth = 0.5 * FMath::Max(4.0, Building->Footprint.Depth() - 0.6) * DTGMetresToUnreal;
	const FVector Origin = GetActorLocation();
	Exit = Origin + FVector(0.0, -HalfDepth + 60.0, 100.0);
	return Origin + FVector(0.0, -HalfDepth + 220.0, 100.0);
}

void ADTGInterior::Close()
{
	BuildingId = -1;
	Room->ClearAllMeshSections();
}
