#include "Game/DTGWorldBuilder.h"

#include "Sim/RoadMarkings.h"
#include "Sim/StreetFurniture.h"
#include "Sim/Vegetation.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

#include "Game/DTGCharacter.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGMeshBuilder.h"
#include "Game/DTGPlantModels.h"
#include "Game/DTGPropModels.h"

using namespace dtg::sim;

ADTGWorldBuilder::ADTGWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// The terrain is a pool of components created as chunks are needed (see TakeTerrainPart), so the
	// root is a plain scene component and everything hangs off it.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Surfaces = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surfaces"));
	Surfaces->SetupAttachment(Root);
	Surfaces->bUseAsyncCooking = true;
	Surfaces->SetCollisionProfileName(TEXT("BlockAll"));

	Buildings = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Buildings"));
	Buildings->SetupAttachment(Root);
	Buildings->bUseAsyncCooking = true;
	Buildings->SetCollisionProfileName(TEXT("BlockAll"));

	Water = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(Root);
	// Water is see-through and swum through, not stood on; vehicles test for it through the world builder.
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADTGWorldBuilder::BeginPlay()
{
	Super::BeginPlay();
	EnsureGame();
	if (!Game || !Game->IsContentLoaded())
	{
		UE_LOG(LogDTG, Error, TEXT("World builder has no loaded content; the world will be empty."));
		return;
	}
	BuildStaticWorld();
	bBuilt = true;
}

// ============================================================================================ Static world

void ADTGWorldBuilder::AddSlab(FDTGMesh& Mesh, double MinX, double MinY, double MaxX, double MaxY, double Z, const FLinearColor& Color)
{
	const double S = DTGMetresToUnreal;
	Mesh.AddQuad(FVector(MinX * S, MinY * S, Z * S), FVector(MaxX * S, MinY * S, Z * S), FVector(MaxX * S, MaxY * S, Z * S),
		FVector(MinX * S, MaxY * S, Z * S), FVector::UpVector, Color);
}

void ADTGWorldBuilder::Rebuild()
{
	// Switching states replaces the whole world, so everything standing has to go first.
	for (int32 Index = Chunks.Num() - 1; Index >= 0; --Index)
	{
		ReleaseChunk(Index);
	}
	Surfaces->ClearAllMeshSections();
	Buildings->ClearAllMeshSections();
	Water->ClearAllMeshSections();
	Chunks.Reset();
	ChunkAt.Reset();
	Pending.Reset();
	bBuilt = false;
	if (Game && Game->IsContentLoaded())
	{
		BuildStaticWorld();
		bBuilt = true;
	}
}

void ADTGWorldBuilder::BuildStaticWorld()
{
	// Streets, highways, paint and venues share the Surfaces component, a section per surface kind each,
	// handed out in order. Fixed section numbers used to let the venues overwrite the road paint.
	NextSurfaceSection = 0;
	BuildTownStreets();
	BuildHighways();
	BuildRoadMarkings();
	BuildBuildings();
	BuildVenues();
	BuildWater();
}

void ADTGWorldBuilder::BuildTownStreets()
{
	const CityPlan& City = Game->Sim().City;
	const StatePlan& Map = Game->Sim().Map;
	const FLinearColor Asphalt(0.055f, 0.055f, 0.06f);
	const FLinearColor Sidewalk(0.42f, 0.41f, 0.39f);
	const FLinearColor Grass(0.16f, 0.27f, 0.11f);

	FDTGSurfaceMeshes Meshes;
	FDTGMesh& Mesh = Meshes[EDTGSurface::Asphalt];
	for (const RoadPlan& Road : City.Roads)
	{
		// A street sits a hair above its town's pad so it reads as a surface, not z-fighting with the ground.
		const double Z = Map.HeightAt(Road.Area.CenterX(), Road.Area.CenterY()) + 0.04;
		AddSlab(Mesh, Road.Area.MinX, Road.Area.MinY, Road.Area.MaxX, Road.Area.MaxY, Z, Asphalt);
	}
	// A block is a kerbed sidewalk round its edge and a lawn inside, as a Carolina street is. Paved edge to
	// edge, a block read as one enormous parking lot. Downtown blocks, and blocks too small for a lawn, stay
	// paved. The kerb stands 15 cm proud of the road.
	const double S = DTGMetresToUnreal;
	const double KerbM = 0.15;
	const double WalkM = 2.5;
	FDTGMesh& Walks = Meshes[EDTGSurface::Sidewalk];
	auto AddKerb = [&Walks, S, &Sidewalk](double X0, double Y0, double X1, double Y1, double Low, double High, const FVector& Facing)
	{
		Walks.AddQuad(FVector(X0 * S, Y0 * S, Low * S), FVector(X1 * S, Y1 * S, Low * S), FVector(X1 * S, Y1 * S, High * S),
			FVector(X0 * S, Y0 * S, High * S), Facing, Sidewalk);
	};
	for (const dtg::sim::Rect& Block : City.Blocks)
	{
		const double Base = Map.HeightAt(Block.CenterX(), Block.CenterY());
		const double Top = Base + KerbM;
		bool bPaved = Block.Width() < 4.0 * WalkM || Block.Depth() < 4.0 * WalkM;
		for (const BuildingPlan& Building : City.Buildings)
		{
			if (bPaved)
			{
				break;
			}
			bPaved = Building.bDowntown && Block.Contains(Building.Footprint.CenterX(), Building.Footprint.CenterY());
		}
		if (bPaved)
		{
			AddSlab(Walks, Block.MinX, Block.MinY, Block.MaxX, Block.MaxY, Top, Sidewalk);
		}
		else
		{
			const double InMinX = Block.MinX + WalkM;
			const double InMinY = Block.MinY + WalkM;
			const double InMaxX = Block.MaxX - WalkM;
			const double InMaxY = Block.MaxY - WalkM;
			AddSlab(Walks, Block.MinX, Block.MinY, Block.MaxX, InMinY, Top, Sidewalk);
			AddSlab(Walks, Block.MinX, InMaxY, Block.MaxX, Block.MaxY, Top, Sidewalk);
			AddSlab(Walks, Block.MinX, InMinY, InMinX, InMaxY, Top, Sidewalk);
			AddSlab(Walks, InMaxX, InMinY, Block.MaxX, InMaxY, Top, Sidewalk);
			const double Lawn = Base + 0.08;
			AddSlab(Meshes[EDTGSurface::Grass], InMinX, InMinY, InMaxX, InMaxY, Lawn, Grass);
			// The sidewalk's inner edge, stepping down to the lawn.
			AddKerb(InMinX, InMinY, InMaxX, InMinY, Lawn, Top, FVector(0.0, 1.0, 0.0));
			AddKerb(InMinX, InMaxY, InMaxX, InMaxY, Lawn, Top, FVector(0.0, -1.0, 0.0));
			AddKerb(InMinX, InMinY, InMinX, InMaxY, Lawn, Top, FVector(1.0, 0.0, 0.0));
			AddKerb(InMaxX, InMinY, InMaxX, InMaxY, Lawn, Top, FVector(-1.0, 0.0, 0.0));
		}
		// The kerb itself, facing the road on all four sides.
		AddKerb(Block.MinX, Block.MinY, Block.MaxX, Block.MinY, Base, Top, FVector(0.0, -1.0, 0.0));
		AddKerb(Block.MinX, Block.MaxY, Block.MaxX, Block.MaxY, Base, Top, FVector(0.0, 1.0, 0.0));
		AddKerb(Block.MinX, Block.MinY, Block.MinX, Block.MaxY, Base, Top, FVector(-1.0, 0.0, 0.0));
		AddKerb(Block.MaxX, Block.MinY, Block.MaxX, Block.MaxY, Base, Top, FVector(1.0, 0.0, 0.0));
	}
	for (const dtg::sim::Rect& Park : City.Parks)
	{
		const double Z = Map.HeightAt(Park.CenterX(), Park.CenterY()) + 0.08;
		AddSlab(Meshes[EDTGSurface::Grass], Park.MinX, Park.MinY, Park.MaxX, Park.MaxY, Z, Grass);
	}
	NextSurfaceSection = Meshes.Apply(Surfaces, NextSurfaceSection, /*bCollision*/ true);
}

void ADTGWorldBuilder::BuildHighways()
{
	const CityPlan& City = Game->Sim().City;
	const double S = DTGMetresToUnreal;
	const FLinearColor Asphalt(0.06f, 0.06f, 0.065f);
	FDTGSurfaceMeshes Meshes;
	FDTGMesh& Mesh = Meshes[EDTGSurface::Asphalt];

	for (const HighwayPlan& Road : City.Highways)
	{
		const double Length = Road.LengthM();
		if (Length < 1.0 || Road.Points.size() < 2)
		{
			continue;
		}
		// Walk the road at the spacing its deck heights were sampled at, so bridges and their ramps come
		// through exactly as the simulation graded them.
		const double Step = FMath::Max(2.0, Road.DeckStepM);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Length / Step));
		const double Half = 0.5 * Road.WidthM;
		FVector PrevLeft = FVector::ZeroVector;
		FVector PrevRight = FVector::ZeroVector;
		bool bHavePrevious = false;
		for (int32 Index = 0; Index <= Steps; ++Index)
		{
			const double Along = FMath::Min(Length, Index * Step);
			const std::array<double, 3> Here = Road.At(Along);
			const std::array<double, 3> Ahead = Road.At(FMath::Min(Length, Along + 1.0));
			double DirX = Ahead[0] - Here[0];
			double DirY = Ahead[1] - Here[1];
			if (FMath::Abs(DirX) + FMath::Abs(DirY) < 1e-6)
			{
				const std::array<double, 3> Behind = Road.At(FMath::Max(0.0, Along - 1.0));
				DirX = Here[0] - Behind[0];
				DirY = Here[1] - Behind[1];
			}
			const double Len = FMath::Max(1e-6, FMath::Sqrt(DirX * DirX + DirY * DirY));
			const double Nx = -DirY / Len;
			const double Ny = DirX / Len;
			const double Z = (Here[2] + 0.05) * S;
			const FVector Left((Here[0] - Nx * Half) * S, (Here[1] - Ny * Half) * S, Z);
			const FVector Right((Here[0] + Nx * Half) * S, (Here[1] + Ny * Half) * S, Z);
			// Markings are laid down by BuildRoadMarkings, which follows the US convention per road
			// class; this pass is only the carriageway itself.
			if (bHavePrevious)
			{
				Mesh.AddQuad(PrevLeft, Left, Right, PrevRight, FVector::UpVector, Asphalt);
			}
			PrevLeft = Left;
			PrevRight = Right;
			bHavePrevious = true;
		}
	}
	NextSurfaceSection = Meshes.Apply(Surfaces, NextSurfaceSection, /*bCollision*/ true);
}

void ADTGWorldBuilder::BuildRoadMarkings()
{
	// The paint. Decided in the simulation (Sim/RoadMarkings) so it can be tested without an engine,
	// and so what is checked offline is exactly what gets drawn here.
	const dtg::sim::CityPlan& City = Game->Sim().City;
	const dtg::sim::StatePlan& Map = Game->Sim().Map;
	const double S = DTGMetresToUnreal;
	FDTGSurfaceMeshes Meshes;
	FDTGMesh& Mesh = Meshes[EDTGSurface::Matte];

	auto Paint = [&Mesh, S](const dtg::sim::RoadMark& Mark)
	{
		const double Dx = Mark.Bx - Mark.Ax;
		const double Dy = Mark.By - Mark.Ay;
		const double Length = FMath::Max(1e-9, FMath::Sqrt(Dx * Dx + Dy * Dy));
		const double Nx = -Dy / Length * Mark.WidthM * 0.5;
		const double Ny = Dx / Length * Mark.WidthM * 0.5;
		const FVector A((Mark.Ax - Nx) * S, (Mark.Ay - Ny) * S, Mark.Az * S);
		const FVector B((Mark.Ax + Nx) * S, (Mark.Ay + Ny) * S, Mark.Az * S);
		const FVector C((Mark.Bx + Nx) * S, (Mark.By + Ny) * S, Mark.Bz * S);
		const FVector D((Mark.Bx - Nx) * S, (Mark.By - Ny) * S, Mark.Bz * S);
		Mesh.AddQuad(A, B, C, D, FVector::UpVector,
			FLinearColor(static_cast<float>(Mark.Rgb[0]), static_cast<float>(Mark.Rgb[1]),
				static_cast<float>(Mark.Rgb[2])));
	};

	for (const dtg::sim::HighwayPlan& Road : City.Highways)
	{
		for (const dtg::sim::RoadMark& Mark : dtg::sim::MarkHighway(Road))
		{
			Paint(Mark);
		}
	}
	// Town streets sit on the town's level pad, so their heights come from the terrain.
	const auto HeightAt = [](double X, double Y, const void* User) -> double
	{
		return static_cast<const dtg::sim::StatePlan*>(User)->HeightAt(X, Y);
	};
	for (const dtg::sim::RoadMark& Mark : dtg::sim::MarkStreets(City, HeightAt, &Map))
	{
		Paint(Mark);
	}

	NextSurfaceSection = Meshes.Apply(Surfaces, NextSurfaceSection, /*bCollision*/ false);
}

UInstancedStaticMeshComponent* ADTGWorldBuilder::PlantComponent(int32 Part, int32 Model)
{
	const int32 Index = Part * DTGPlantModels::Count() + Model;
	if (Index >= PlantInstances.Num())
	{
		PlantInstances.SetNum(Index + 1);
	}
	if (!PlantInstances[Index])
	{
		UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
		Instances->SetupAttachment(GetRootComponent());
		Instances->RegisterComponent();
		Instances->SetStaticMesh(DTGPlantModels::Mesh(Model));
		// As before: no collision. A car that clips a pine is a smaller problem than cooking collision
		// for hundreds of trees every time a chunk streams in.
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCastShadow(true);
		PlantInstances[Index] = Instances;
	}
	return PlantInstances[Index].Get();
}

void ADTGWorldBuilder::ClearPlants(int32 Part)
{
	for (int32 Model = 0; Model < DTGPlantModels::Count(); ++Model)
	{
		const int32 Index = Part * DTGPlantModels::Count() + Model;
		if (PlantInstances.IsValidIndex(Index) && PlantInstances[Index])
		{
			PlantInstances[Index]->ClearInstances();
			PlantInstances[Index]->SetVisibility(false);
		}
	}
}

UInstancedStaticMeshComponent* ADTGWorldBuilder::PropComponent(int32 Part, int32 Mesh)
{
	const int32 Index = Part * DTGPropModels::MeshCount() + Mesh;
	if (Index >= PropInstances.Num())
	{
		PropInstances.SetNum(Index + 1);
	}
	if (!PropInstances[Index])
	{
		UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
		Instances->SetupAttachment(GetRootComponent());
		Instances->RegisterComponent();
		Instances->SetStaticMesh(DTGPropModels::Mesh(Mesh));
		// Drawn, not collided with, for now: a car body sweep against thousands of poles is its own job.
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCastShadow(true);
		PropInstances[Index] = Instances;
	}
	return PropInstances[Index].Get();
}

void ADTGWorldBuilder::ClearProps(int32 Part)
{
	for (int32 Mesh = 0; Mesh < DTGPropModels::MeshCount(); ++Mesh)
	{
		const int32 Index = Part * DTGPropModels::MeshCount() + Mesh;
		if (PropInstances.IsValidIndex(Index) && PropInstances[Index])
		{
			PropInstances[Index]->ClearInstances();
			PropInstances[Index]->SetVisibility(false);
		}
	}
}

void ADTGWorldBuilder::BuildChunkProps(int32 ChunkX, int32 ChunkY, int32 Part)
{
	ClearProps(Part);
	if (!DTGPropModels::Ready())
	{
		return;
	}
	const double OriginX = ChunkX * static_cast<double>(ChunkSizeM);
	const double OriginY = ChunkY * static_cast<double>(ChunkSizeM);
	const dtg::sim::Rect Area{OriginX, OriginY, OriginX + ChunkSizeM, OriginY + ChunkSizeM};
	dtg::sim::FurnitureRules Rules;
	// Barriers laid end to end at the model's own length, so a raised deck has a continuous wall.
	Rules.BarrierLengthM = DTGPropModels::BarrierLengthM();
	const std::vector<dtg::sim::Prop> Props = dtg::sim::FurnitureIn(Game->Sim().Map, Game->Sim().City, Area, Rules);

	TArray<TArray<FTransform>> PerMesh;
	PerMesh.SetNum(DTGPropModels::MeshCount());
	for (const dtg::sim::Prop& Value : Props)
	{
		const FTransform Placed = DTGPropModels::TransformFor(Value);
		for (const DTGPropModels::FPart& Piece : DTGPropModels::PartsFor(Value.Kind, Value.Variant))
		{
			PerMesh[Piece.Mesh].Add(Piece.Local * Placed);
		}
	}
	for (int32 Mesh = 0; Mesh < PerMesh.Num(); ++Mesh)
	{
		if (PerMesh[Mesh].IsEmpty())
		{
			continue;
		}
		UInstancedStaticMeshComponent* Instances = PropComponent(Part, Mesh);
		Instances->AddInstances(PerMesh[Mesh], /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
		Instances->SetVisibility(true);
	}
}

void ADTGWorldBuilder::BuildChunkPlants(int32 ChunkX, int32 ChunkY, int32 Part, UProceduralMeshComponent* Mesh)
{
	if (!bPlantVegetation || Mesh == nullptr)
	{
		return;
	}
	const dtg::sim::StatePlan& Map = Game->Sim().Map;
	const dtg::sim::CityPlan& City = Game->Sim().City;
	const double S = DTGMetresToUnreal;
	const double OriginX = ChunkX * static_cast<double>(ChunkSizeM);
	const double OriginY = ChunkY * static_cast<double>(ChunkSizeM);
	const dtg::sim::Rect Area{OriginX, OriginY, OriginX + ChunkSizeM, OriginY + ChunkSizeM};

	dtg::sim::PlantingRules Rules;
	const std::vector<dtg::sim::Plant> Grown = dtg::sim::PlantsIn(Map, City, Area, Rules);
	ClearPlants(Part);
	if (Grown.empty())
	{
		Mesh->ClearMeshSection(1);
		return;
	}

	// Real models when they have been imported: the same plants in the same places, drawn as instances.
	if (DTGPlantModels::Ready())
	{
		TArray<TArray<FTransform>> PerModel;
		PerModel.SetNum(DTGPlantModels::Count());
		for (const dtg::sim::Plant& Value : Grown)
		{
			const int32 Model = DTGPlantModels::ModelFor(Value);
			if (Model != INDEX_NONE)
			{
				PerModel[Model].Add(DTGPlantModels::TransformFor(Value, Model));
			}
		}
		for (int32 Model = 0; Model < PerModel.Num(); ++Model)
		{
			if (PerModel[Model].IsEmpty())
			{
				continue;
			}
			UInstancedStaticMeshComponent* Instances = PlantComponent(Part, Model);
			Instances->AddInstances(PerModel[Model], /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
			Instances->SetVisibility(true);
		}
		Mesh->ClearMeshSection(1);
		return;
	}

	FDTGMesh Trees;
	std::vector<dtg::sim::PlantTri> Tris;
	for (const dtg::sim::Plant& Value : Grown)
	{
		Tris.clear();
		dtg::sim::BuildPlant(Value, Tris);
		for (const dtg::sim::PlantTri& Tri : Tris)
		{
			const FVector A(Tri.P[0][0] * S, Tri.P[0][1] * S, Tri.P[0][2] * S);
			const FVector B(Tri.P[1][0] * S, Tri.P[1][1] * S, Tri.P[1][2] * S);
			const FVector C(Tri.P[2][0] * S, Tri.P[2][1] * S, Tri.P[2][2] * S);
			// The triangle's own normal: foliage is flat-shaded, which at these sizes reads as
			// leaf clusters catching the light rather than as faceting.
			const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
			Trees.AddTri(A, B, C, Normal.IsNearlyZero() ? FVector::UpVector : Normal,
				FLinearColor(static_cast<float>(Tri.Rgb[0]), static_cast<float>(Tri.Rgb[1]),
					static_cast<float>(Tri.Rgb[2])));
		}
	}
	if (!Trees.IsEmpty())
	{
		// No collision: a car that clips a pine is a far smaller problem than cooking collision for
		// several hundred trees every time a chunk streams in.
		Mesh->CreateMeshSection_LinearColor(1, Trees.Verts, Trees.Tris, Trees.Normals, Trees.Uvs,
			Trees.Colors, Trees.Tangents, /*bCreateCollision*/ false);
		Mesh->SetMaterial(1, DTGSurface::Material(EDTGSurface::Leaves));
	}
	else
	{
		Mesh->ClearMeshSection(1);
	}
}

void ADTGWorldBuilder::BuildBuildings()
{
	const CityPlan& City = Game->Sim().City;
	const StatePlan& Map = Game->Sim().Map;
	// One mesh section per town: a town's worth of shells is a few thousand triangles, and keeping them
	// apart means a section can be rebuilt later without touching the rest of the state.
	int32 Section = 0;
	int32 Total = 0;
	auto BuildRange = [&](const dtg::sim::Rect* Bounds)
	{
		FDTGSurfaceMeshes Meshes;
		for (const BuildingPlan& Building : City.Buildings)
		{
			if (Bounds && !Bounds->Contains(Building.Footprint.CenterX(), Building.Footprint.CenterY()))
			{
				continue;
			}
			const double GroundZ = Map.HeightAt(Building.Footprint.CenterX(), Building.Footprint.CenterY());
			DTGMeshBuilder::BuildBuilding(Building, GroundZ, DTGMetresToUnreal, Meshes);
			++Total;
		}
		Section = Meshes.Apply(Buildings, Section, /*bCollision*/ true);
	};
	if (City.Cities.empty())
	{
		BuildRange(nullptr);
	}
	else
	{
		for (const CityInfo& Town : City.Cities)
		{
			BuildRange(&Town.Bounds);
		}
	}
	UE_LOG(LogDTG, Log, TEXT("Built %d building shells across %d sections"), Total, Section);
}

void ADTGWorldBuilder::BuildVenues()
{
	FDTGSurfaceMeshes StandMeshes;
	FDTGMesh& Stands = StandMeshes[EDTGSurface::Concrete];
	const CityPlan& City = Game->Sim().City;
	const StatePlan& Map = Game->Sim().Map;
	const FLinearColor Apron(0.21f, 0.2f, 0.19f);
	const FLinearColor Track(0.05f, 0.05f, 0.055f);
	const FLinearColor Stand(0.3f, 0.3f, 0.32f);
	const double S = DTGMetresToUnreal;
	FDTGSurfaceMeshes Meshes;
	FDTGMesh& Mesh = Meshes[EDTGSurface::Asphalt];
	for (const VenuePlan& Venue : City.Venues)
	{
		const double Z = Map.HeightAt(Venue.Area.CenterX(), Venue.Area.CenterY());
		AddSlab(Meshes[EDTGSurface::Concrete], Venue.Area.MinX, Venue.Area.MinY, Venue.Area.MaxX, Venue.Area.MaxY, Z + 0.02, Apron);
		// The racing surface follows the venue's centreline.
		const double Half = 0.5 * Venue.TrackWidthM;
		for (std::size_t Index = 0; Index + 1 < Venue.Path.size(); ++Index)
		{
			const std::array<double, 2>& A = Venue.Path[Index];
			const std::array<double, 2>& B = Venue.Path[Index + 1];
			const double Dx = B[0] - A[0];
			const double Dy = B[1] - A[1];
			const double Len = FMath::Max(1e-6, FMath::Sqrt(Dx * Dx + Dy * Dy));
			const double Nx = -Dy / Len * Half;
			const double Ny = Dx / Len * Half;
			Mesh.AddQuad(FVector((A[0] - Nx) * S, (A[1] - Ny) * S, (Z + 0.06) * S),
				FVector((A[0] + Nx) * S, (A[1] + Ny) * S, (Z + 0.06) * S),
				FVector((B[0] + Nx) * S, (B[1] + Ny) * S, (Z + 0.06) * S),
				FVector((B[0] - Nx) * S, (B[1] - Ny) * S, (Z + 0.06) * S), FVector::UpVector, Track);
		}
		// Grandstands down the long side, so an event has somewhere for a crowd to be. They go in with
		// the buildings rather than the surfaces: a car's body sweep ignores surfaces, because a car is
		// meant to drive on a road, and a grandstand is the one thing in here it should hit.
		const double StandDepth = FMath::Min(14.0, Venue.Area.Depth() * 0.12);
		for (int32 Tier = 0; Tier < 4; ++Tier)
		{
			const double Inset = Tier * StandDepth * 0.25;
			Stands.AddBox(FVector(Venue.Area.MinX * S, (Venue.Area.MinY - StandDepth + Inset) * S, (Z + Tier * 0.9) * S),
				FVector(Venue.Area.MaxX * S, (Venue.Area.MinY - StandDepth + Inset + StandDepth * 0.25) * S, (Z + (Tier + 1) * 0.9) * S),
				Stand);
		}
	}
	NextSurfaceSection = Meshes.Apply(Surfaces, NextSurfaceSection, /*bCollision*/ true);
	// Sections past the towns', which BuildBuildings has already handed out.
	StandMeshes.Apply(Buildings, Buildings->GetNumSections(), /*bCollision*/ true);
}

void ADTGWorldBuilder::BuildWater()
{
	const StatePlan& Map = Game->Sim().Map;
	const FLinearColor Sea(0.02f, 0.09f, 0.16f);
	const FLinearColor Fresh(0.05f, 0.14f, 0.15f);
	const double S = DTGMetresToUnreal;
	FDTGMesh Mesh;

	// The ocean is one sheet at sea level, reaching well past the coast.
	const dtg::sim::Rect& Bounds = Map.Bounds;
	const double Margin = 4000.0;
	AddSlab(Mesh, Bounds.MinX - Margin, Bounds.MinY - Margin, Bounds.MaxX + Margin, Bounds.MaxY + Margin, 0.0, Sea);
	for (const StateLake& Lake : Map.Lakes)
	{
		// A disc rather than a square, so a lake has a shoreline.
		constexpr int32 Sides = 24;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const double A0 = 2.0 * UE_PI * Side / Sides;
			const double A1 = 2.0 * UE_PI * (Side + 1) / Sides;
			Mesh.AddTri(FVector(Lake.X * S, Lake.Y * S, Lake.LevelM * S),
				FVector((Lake.X + Lake.RadiusM * FMath::Cos(A0)) * S, (Lake.Y + Lake.RadiusM * FMath::Sin(A0)) * S, Lake.LevelM * S),
				FVector((Lake.X + Lake.RadiusM * FMath::Cos(A1)) * S, (Lake.Y + Lake.RadiusM * FMath::Sin(A1)) * S, Lake.LevelM * S),
				FVector::UpVector, Fresh);
		}
	}
	for (const StateRiver& River : Map.Rivers)
	{
		for (std::size_t Index = 0; Index + 1 < River.Points.size(); ++Index)
		{
			const Point2& A = River.Points[Index];
			const Point2& B = River.Points[Index + 1];
			const double Dx = B[0] - A[0];
			const double Dy = B[1] - A[1];
			const double Len = FMath::Max(1e-6, FMath::Sqrt(Dx * Dx + Dy * Dy));
			const double Half = 0.5 * River.WidthM;
			const double Nx = -Dy / Len * Half;
			const double Ny = Dx / Len * Half;
			const double ZA = FMath::Max(0.2, Map.NaturalHeight(A[0], A[1]) - 0.8);
			const double ZB = FMath::Max(0.2, Map.NaturalHeight(B[0], B[1]) - 0.8);
			Mesh.AddQuad(FVector((A[0] - Nx) * S, (A[1] - Ny) * S, ZA * S), FVector((A[0] + Nx) * S, (A[1] + Ny) * S, ZA * S),
				FVector((B[0] + Nx) * S, (B[1] + Ny) * S, ZB * S), FVector((B[0] - Nx) * S, (B[1] - Ny) * S, ZB * S),
				FVector::UpVector, Fresh);
		}
	}
	Water->CreateMeshSection_LinearColor(0, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors, Mesh.Tangents, false);
	Water->SetMaterial(0, DTGSurface::Material(EDTGSurface::Water));
}

// ============================================================================================ Terrain streaming

void ADTGWorldBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	FVector Around = FVector::ZeroVector;
	if (const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const APawn* Pawn = Controller->GetPawn())
		{
			// The car while driving, and the street outside while indoors -- interiors are a pocket
			// far below the map, and streaming the ground around *that* would unload the whole state.
			Around = ADTGCharacter::InterestPointFor(Pawn);
		}
		else
		{
			FRotator Unused;
			Controller->GetPlayerViewPoint(Around, Unused);
		}
	}
	UpdateChunks(Around);
}

void ADTGWorldBuilder::UpdateChunks(const FVector& Around)
{
	const double ChunkUu = ChunkSizeM * DTGMetresToUnreal;
	const int32 Radius = FMath::Max(1, FMath::CeilToInt(ViewDistanceM / ChunkSizeM));
	const int32 CentreX = FMath::FloorToInt32(Around.X / ChunkUu);
	const int32 CentreY = FMath::FloorToInt32(Around.Y / ChunkUu);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Note which chunks should exist, nearest first so the ground under the player is never the last to arrive.
	Pending.Reset();
	for (int32 Y = CentreY - Radius; Y <= CentreY + Radius; ++Y)
	{
		for (int32 X = CentreX - Radius; X <= CentreX + Radius; ++X)
		{
			if (FMath::Square(X - CentreX) + FMath::Square(Y - CentreY) > FMath::Square(Radius))
			{
				continue;
			}
			if (const int32* Found = ChunkAt.Find(FIntPoint(X, Y)))
			{
				if (Chunks.IsValidIndex(*Found))
				{
					Chunks[*Found].LastSeen = Now;
					continue;
				}
			}
			Pending.Add(FIntPoint(X, Y));
		}
	}
	Pending.Sort([CentreX, CentreY](const FIntPoint& A, const FIntPoint& B)
	{
		return FMath::Square(A.X - CentreX) + FMath::Square(A.Y - CentreY) < FMath::Square(B.X - CentreX) + FMath::Square(B.Y - CentreY);
	});
	for (int32 Index = 0; Index < FMath::Min(ChunksPerFrame, Pending.Num()); ++Index)
	{
		BuildChunk(Pending[Index].X, Pending[Index].Y);
	}
	// Anything the player has driven away from goes back in the pool.
	for (int32 Index = Chunks.Num() - 1; Index >= 0; --Index)
	{
		if (Chunks[Index].LastSeen < Now - 0.5)
		{
			ReleaseChunk(Index);
		}
	}
}

void ADTGWorldBuilder::BuildChunk(int32 ChunkX, int32 ChunkY)
{
	const StatePlan& Map = Game->Sim().Map;
	const double S = DTGMetresToUnreal;
	// The cell count is rounded first and the spacing derived from it, so chunks tile exactly however
	// coarse the vertex spacing is set.
	const int32 Cells = FMath::Max(1, FMath::RoundToInt(ChunkSizeM / FMath::Max(1.0f, VertexSpacingM)));
	const double Spacing = static_cast<double>(ChunkSizeM) / Cells;
	const double OriginX = ChunkX * static_cast<double>(ChunkSizeM);
	const double OriginY = ChunkY * static_cast<double>(ChunkSizeM);

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> Uvs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Verts.Reserve((Cells + 1) * (Cells + 1));

	for (int32 J = 0; J <= Cells; ++J)
	{
		for (int32 I = 0; I <= Cells; ++I)
		{
			const double X = OriginX + I * Spacing;
			const double Y = OriginY + J * Spacing;
			const SurfaceInfo Info = Map.SurfaceAt(X, Y);
			Verts.Add(FVector(X * S, Y * S, Info.GroundM * S));
			Uvs.Add(FVector2D(X / 8.0, Y / 8.0));
			// Sand near the water line, grass inland, rock on the steep upstate slopes, lake and river beds dark.
			FLinearColor Colour(0.17f, 0.3f, 0.12f);
			if (Info.bWater)
			{
				Colour = Info.bOcean ? FLinearColor(0.5f, 0.45f, 0.34f) : FLinearColor(0.22f, 0.2f, 0.16f);
			}
			else if (Info.GroundM < 1.2 && Map.CoastDistance(X, Y) < 90.0)
			{
				Colour = FLinearColor(0.62f, 0.56f, 0.42f);
			}
			else if (Info.GroundM > 55.0)
			{
				Colour = FLinearColor(0.3f, 0.29f, 0.27f);
			}
			Colors.Add(Colour);
		}
	}
	for (int32 J = 0; J < Cells; ++J)
	{
		for (int32 I = 0; I < Cells; ++I)
		{
			const int32 A = J * (Cells + 1) + I;
			const int32 B = A + 1;
			const int32 C = A + Cells + 1;
			const int32 D = C + 1;
			Tris.Append({A, C, D, A, D, B});
		}
	}
	// Normals from central differences, including one ring of heights outside the chunk. Clamping at the
	// edge instead would give this chunk a one-sided difference and its neighbour the opposite one, and
	// the same vertex would end up with two different normals: a lit crease along every chunk boundary.
	Normals.Reserve(Verts.Num());
	TArray<double> Skirt;
	Skirt.SetNumUninitialized((Cells + 3) * (Cells + 3));
	for (int32 J = -1; J <= Cells + 1; ++J)
	{
		for (int32 I = -1; I <= Cells + 1; ++I)
		{
			const bool bInside = I >= 0 && I <= Cells && J >= 0 && J <= Cells;
			Skirt[(J + 1) * (Cells + 3) + (I + 1)] = bInside
				? Verts[J * (Cells + 1) + I].Z / S
				: Map.SurfaceAt(OriginX + I * Spacing, OriginY + J * Spacing).GroundM;
		}
	}
	const double Span = 2.0 * Spacing;
	for (int32 J = 0; J <= Cells; ++J)
	{
		for (int32 I = 0; I <= Cells; ++I)
		{
			const double Dx = Skirt[(J + 1) * (Cells + 3) + (I + 2)] - Skirt[(J + 1) * (Cells + 3) + I];
			const double Dy = Skirt[(J + 2) * (Cells + 3) + (I + 1)] - Skirt[J * (Cells + 3) + (I + 1)];
			Normals.Add(FVector(-Dx * Span, -Dy * Span, Span * Span).GetSafeNormal());
		}
	}

	int32 Part = -1;
	UProceduralMeshComponent* Mesh = TakeTerrainPart(Part);
	if (!Mesh)
	{
		return;
	}
	Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, Uvs, Colors, Tangents, /*bCreateCollision*/ true);
	// One material for the whole grid, blending grass, sand and clay by the class colours above.
	Mesh->SetMaterial(0, DTGSurface::Material(EDTGSurface::Terrain));
	BuildChunkPlants(ChunkX, ChunkY, Part, Mesh);
	BuildChunkProps(ChunkX, ChunkY, Part);
	Mesh->SetVisibility(true);

	FDTGTerrainChunk Chunk;
	Chunk.X = ChunkX;
	Chunk.Y = ChunkY;
	Chunk.Part = Part;
	Chunk.LastSeen = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ChunkAt.Add(FIntPoint(ChunkX, ChunkY), Chunks.Num());
	Chunks.Add(Chunk);
}

bool ADTGWorldBuilder::HasChunk(int32 ChunkX, int32 ChunkY) const
{
	return ChunkAt.Contains(FIntPoint(ChunkX, ChunkY));
}

bool ADTGWorldBuilder::EnsureGame()
{
	if (!Game)
	{
		Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	}
	return Game != nullptr && Game->IsContentLoaded();
}

UProceduralMeshComponent* ADTGWorldBuilder::TakeTerrainPart(int32& OutIndex)
{
	while (FreeParts.Num() > 0)
	{
		const int32 Spare = FreeParts.Pop();
		if (TerrainParts.IsValidIndex(Spare) && TerrainParts[Spare])
		{
			OutIndex = Spare;
			return TerrainParts[Spare].Get();
		}
		// A pool slot whose component has gone: dropped rather than handed out, and not leaked back.
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	if (!Mesh)
	{
		return nullptr;
	}
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->RegisterComponent();
	// Cooked on the spot. A chunk is a few thousand triangles of its own now rather than a slice of the
	// whole world, so this is cheap -- and async cooking would serve the *old* body setup for several
	// frames, which means driving onto ground that is drawn but cannot be stood on.
	Mesh->bUseAsyncCooking = false;
	// Queries only. Nothing is simulated against the ground -- wheels ride on it by ray cast and
	// characters sweep against it -- so cooking a physics body as well doubles the one cost that
	// actually bites while streaming.
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// Wheels ride on the ground; car bodies are not supposed to crash into it. The body sweep uses its
	// own channel so it can pass through terrain while still hitting walls, people and other cars.
	Mesh->SetCollisionResponseToChannel(DTGBodySweepChannel, ECR_Ignore);
	Mesh->SetCastShadow(true);
	OutIndex = TerrainParts.Add(Mesh);
	return Mesh;
}

void ADTGWorldBuilder::EnsureGroundAt(const FVector& Where)
{
	// Looked up rather than assumed: this is called from InitGame, which runs before BeginPlay, so a
	// pointer cached there would still be null and the whole call would quietly do nothing -- leaving
	// the player to fall through the world it was added to prevent.
	if (!EnsureGame())
	{
		return;
	}
	const double ChunkUu = ChunkSizeM * DTGMetresToUnreal;
	const int32 CentreX = FMath::FloorToInt32(Where.X / ChunkUu);
	const int32 CentreY = FMath::FloorToInt32(Where.Y / ChunkUu);
	// The chunk under the point and the eight around it, so walking a few paces in the first second
	// does not walk off the edge of the only piece of ground that exists.
	for (int32 Y = CentreY - 1; Y <= CentreY + 1; ++Y)
	{
		for (int32 X = CentreX - 1; X <= CentreX + 1; ++X)
		{
			if (!HasChunk(X, Y))
			{
				BuildChunk(X, Y);
			}
		}
	}
}

void ADTGWorldBuilder::ReleaseChunk(int32 Index)
{
	if (!Chunks.IsValidIndex(Index))
	{
		return;
	}
	if (TerrainParts.IsValidIndex(Chunks[Index].Part) && TerrainParts[Chunks[Index].Part])
	{
		// Kept and reused rather than destroyed: creating and registering a component is the expensive
		// part, and the player will want this many again as soon as they turn round.
		TerrainParts[Chunks[Index].Part]->ClearMeshSection(0);
		// Section 1 is this chunk's vegetation. Clearing it matters even though the component is
		// hidden: the part goes back in the pool, and if the next chunk to take it happens to grow
		// nothing -- open water, a town centre, the far side of the state line -- it would otherwise
		// come back visible still carrying the last chunk's trees, standing in the sea.
		TerrainParts[Chunks[Index].Part]->ClearMeshSection(1);
		// And its instanced plants, for the same reason.
		ClearPlants(Chunks[Index].Part);
		ClearProps(Chunks[Index].Part);
		TerrainParts[Chunks[Index].Part]->SetVisibility(false);
		FreeParts.Add(Chunks[Index].Part);
	}
	ChunkAt.Remove(FIntPoint(Chunks[Index].X, Chunks[Index].Y));
	Chunks.RemoveAtSwap(Index);
	// RemoveAtSwap moved the last chunk into this slot, so its index in the map is now wrong.
	if (Chunks.IsValidIndex(Index))
	{
		ChunkAt.Add(FIntPoint(Chunks[Index].X, Chunks[Index].Y), Index);
	}
}

// ============================================================================================ Queries

double ADTGWorldBuilder::GroundHeightAt(const FVector& World) const
{
	if (!Game || !Game->IsContentLoaded())
	{
		return 0.0;
	}
	const FVector2D Sim = Game->ToSim(World);
	return Game->Sim().Map.HeightAt(Sim.X, Sim.Y) * DTGMetresToUnreal;
}

bool ADTGWorldBuilder::IsWaterAt(const FVector& World, double* OutSurfaceZ) const
{
	if (!Game || !Game->IsContentLoaded())
	{
		return false;
	}
	const FVector2D Sim = Game->ToSim(World);
	const SurfaceInfo Info = Game->Sim().Map.SurfaceAt(Sim.X, Sim.Y);
	if (OutSurfaceZ)
	{
		*OutSurfaceZ = Info.WaterM * DTGMetresToUnreal;
	}
	return Info.bWater;
}
