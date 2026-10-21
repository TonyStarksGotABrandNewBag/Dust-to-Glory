#include "Game/DTGWorldBuilder.h"

#include "Sim/Lots.h"
#include "Sim/RoadMarkings.h"
#include "Sim/StreetFurniture.h"
#include "Sim/Vegetation.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

#include <cmath>
#include <vector>

#include "Game/DTGCharacter.h"
#include "Game/DTGGameSubsystem.h"
#include "Game/DTGMeshBuilder.h"
#include "Game/DTGPlantModels.h"
#include "Game/DTGPropModels.h"
#include "Game/DTGSky.h"

using namespace dtg::sim;

namespace detail_worldbuilder
{
	/**
	 * A county road: the asphalt as a strip whose edges wander a hand's width with the ground's noise,
	 * and a sand shoulder beside each edge out to where the ditch begins (Sim/Lots DitchFromM), tucked
	 * under the asphalt's edge so no seam shows. Every two metres along the road.
	 */
	void AddRaggedRoad(FDTGMesh& Asphalt, FDTGMesh& Shoulder, const dtg::sim::StatePlan& Map, const dtg::sim::RoadPlan& Road, double Z,
		const FLinearColor& AsphaltColour, const FLinearColor& SandColour)
	{
		const double S = DTGMetresToUnreal;
		const double From = Road.bHorizontal ? Road.Area.MinX : Road.Area.MinY;
		const double To = Road.bHorizontal ? Road.Area.MaxX : Road.Area.MaxY;
		const double Lo = Road.bHorizontal ? Road.Area.MinY : Road.Area.MinX;
		const double Hi = Road.bHorizontal ? Road.Area.MaxY : Road.Area.MaxX;
		const double ShoulderM = dtg::sim::DitchFromM + 0.2;
		auto Point = [&](double Along, double Across, double Height)
		{
			return Road.bHorizontal ? FVector(Along * S, Across * S, Height * S) : FVector(Across * S, Along * S, Height * S);
		};
		auto Edge = [&](double Along, double Side)
		{
			// Outward is positive: the crumble eats into the asphalt, so the edge only ever moves in.
			const double Wander = 0.3 * (0.5 + 0.5 * Map.Noise(Road.bHorizontal ? Along : Side, Road.bHorizontal ? Side : Along, 5.0));
			return Side < 0.5 * (Lo + Hi) ? Lo + Wander : Hi - Wander;
		};
		const double Step = 2.0;
		double PrevLo = Edge(From, Lo);
		double PrevHi = Edge(From, Hi);
		for (double Along = From; Along < To - 1e-6; Along += Step)
		{
			const double Next = FMath::Min(To, Along + Step);
			const double NextLo = Edge(Next, Lo);
			const double NextHi = Edge(Next, Hi);
			// The asphalt between the two edges.
			if (Road.bHorizontal)
			{
				Asphalt.AddQuad(Point(Along, PrevLo, Z), Point(Next, NextLo, Z), Point(Next, NextHi, Z), Point(Along, PrevHi, Z), FVector::UpVector, AsphaltColour);
			}
			else
			{
				Asphalt.AddQuad(Point(Along, PrevLo, Z), Point(Along, PrevHi, Z), Point(Next, NextHi, Z), Point(Next, NextLo, Z), FVector::UpVector, AsphaltColour);
			}
			// The shoulder is the ground itself, coloured sand by the chunk (BuildChunk), not a strip laid on it:
			// a strip in any one surface read as a painted band from above.
			(void)Shoulder;
			(void)SandColour;
			(void)ShoulderM;
			PrevLo = NextLo;
			PrevHi = NextHi;
		}
	}
	/** A street lamp at full dark. Real lamps are 5000 to 10000 lumens; this sun is six units, not a hundred thousand. */
	// A real street lamp, now that the camera meters physical light: a 150 W sodium lamp puts out about
	// this much. At 2000 it was a porch light.
	constexpr float LampLumens = 12000.0f;
}

ADTGWorldBuilder::ADTGWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// The terrain is a pool of components created as chunks are needed (see TakeTerrainPart), so the
	// root is a plain scene component and everything hangs off it.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// Nothing here ever moves, and that is worth saying: a Movable mesh is drawn into the virtual shadow
	// maps again every frame, and with the whole world Movable that was 9.6 ms of a 24 ms frame on the
	// street. Static, its shadow pages are cached and only redrawn when a chunk is rebuilt.
	Root->SetMobility(EComponentMobility::Static);

	Surfaces = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surfaces"));
	Surfaces->SetupAttachment(Root);
	Surfaces->SetMobility(EComponentMobility::Static);
	Surfaces->bUseAsyncCooking = true;
	Surfaces->SetCollisionProfileName(TEXT("BlockAll"));

	Buildings = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Buildings"));
	Buildings->SetupAttachment(Root);
	Buildings->SetMobility(EComponentMobility::Static);
	Buildings->bUseAsyncCooking = true;
	Buildings->SetCollisionProfileName(TEXT("BlockAll"));

	Water = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(Root);
	Water->SetMobility(EComponentMobility::Static);
	// Water is see-through and swum through, not stood on; vehicles test for it through the world builder.
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// And casts no shadow: one sheet from four kilometres past the coast to four past the far side
	// covers every page of the shadow map at every level, which is what overflowed the virtual shadow
	// maps' non-Nanite marking queue every frame ("[VSM] Non-Nanite Marking Job Queue overflow").
	Water->SetCastShadow(false);
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
	// Timed: the whole map is built here at once, and it has been slow enough to matter.
	double Marks[8];
	Marks[0] = FPlatformTime::Seconds();
	BuildTownStreets();
	Marks[1] = FPlatformTime::Seconds();
	BuildHighways();
	Marks[2] = FPlatformTime::Seconds();
	BuildRoadMarkings();
	Marks[3] = FPlatformTime::Seconds();
	BuildBuildings();
	Marks[4] = FPlatformTime::Seconds();
	BuildVenues();
	Marks[5] = FPlatformTime::Seconds();
	BuildWater();
	Marks[6] = FPlatformTime::Seconds();
	CookBuildingCollision();
	Marks[7] = FPlatformTime::Seconds();
	UE_LOG(LogDTG, Log, TEXT("Static world: streets %.1f s, highways %.1f s, paint %.1f s, buildings %.1f s, venues %.1f s, water %.1f s, collision %.1f s"),
		Marks[1] - Marks[0], Marks[2] - Marks[1], Marks[3] - Marks[2], Marks[4] - Marks[3], Marks[5] - Marks[4], Marks[6] - Marks[5], Marks[7] - Marks[6]);
}

void ADTGWorldBuilder::CookBuildingCollision()
{
	// A section created with collision cooks the whole component's collision again, so 191 sections of
	// shells and stands created that way cooked it 191 times over: six minutes of the load. The sections
	// are created without, flagged here, and cooked once. ClearCollisionConvexMeshes -- there are none --
	// is the component's public way to re-cook.
	for (int32 Index = 0; Index < Buildings->GetNumSections(); ++Index)
	{
		if (FProcMeshSection* Part = Buildings->GetProcMeshSection(Index))
		{
			Part->bEnableCollision = true;
		}
	}
	Buildings->ClearCollisionConvexMeshes();
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
	const FLinearColor Sand(0.42f, 0.37f, 0.28f);
	for (const RoadPlan& Road : City.Roads)
	{
		// A street sits a hair above its town's pad so it reads as a surface, not z-fighting with the ground.
		const double Z = Map.HeightAt(Road.Area.CenterX(), Road.Area.CenterY()) + 0.04;
		const CityInfo* const Town = City.CityAt(Road.Area.CenterX(), Road.Area.CenterY());
		if (Town != nullptr && Town->Style == CityStyle::Rural)
		{
			// A county road: the asphalt's edge wanders a hand's width either way where it has crumbled, and
			// a sand shoulder runs beside it out to where the ditch begins, under the asphalt's edge so no
			// seam shows. The edge is a strip of quads every two metres along the road.
			detail_worldbuilder::AddRaggedRoad(Mesh, Meshes[EDTGSurface::Dirt], Map, Road, Z, Asphalt, Sand);
			continue;
		}
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
		// A crossroads community has no sidewalk and no kerb, and no lawn slab either: the ground itself,
		// with its ditch beside the street, shows through, and the lots lay their drives on it.
		const CityInfo* const Town = City.CityAt(Block.CenterX(), Block.CenterY());
		if (Town != nullptr && Town->Style == CityStyle::Rural)
		{
			continue;
		}
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

namespace detail_worldbuilder
{
	/**
	 * -DTGMatScalar=Name:Value sets one scalar on every plant material, for finding out which parameter of
	 * a pack's material does something unwanted without rebuilding assets between runs. Diagnostic only.
	 */
	void ApplyScalarOverride(UMeshComponent* Mesh, UObject* Outer)
	{
		FString Spec;
		if (!FParse::Value(FCommandLine::Get(), TEXT("DTGMatScalar="), Spec, /*bShouldStopOnSeparator*/ false) || Spec.IsEmpty())
		{
			return;
		}
		FString Name;
		FString Value;
		if (!Spec.Split(TEXT(":"), &Name, &Value))
		{
			return;
		}
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInterface* Parent = Mesh->GetMaterial(Slot))
			{
				if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Parent, Outer))
				{
					Dynamic->SetScalarParameterValue(*Name, FCString::Atof(*Value));
					Mesh->SetMaterial(Slot, Dynamic);
				}
			}
		}
	}
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
		Instances->SetMobility(EComponentMobility::Static);
		Instances->RegisterComponent();
		Instances->SetStaticMesh(DTGPlantModels::Mesh(Model));
		detail_worldbuilder::ApplyScalarOverride(Instances, this);
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
		Instances->SetMobility(EComponentMobility::Static);
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
	for (int32 Index = LampLights.Num() - 1; Index >= 0; --Index)
	{
		if (LampLightParts[Index] == Part)
		{
			if (LampLights[Index])
			{
				LampLights[Index]->DestroyComponent();
			}
			LampLights.RemoveAtSwap(Index);
			LampLightParts.RemoveAtSwap(Index);
		}
	}
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
	TArray<TArray<FTransform>> PerTree;
	PerTree.SetNum(DTGPlantModels::Count());
	for (const dtg::sim::Prop& Value : Props)
	{
		if (Value.Kind == dtg::sim::PropKind::StreetTree)
		{
			// A street tree is a plant the furniture placed: drawn with the plant models, on the plant components.
			dtg::sim::Plant Tree;
			Tree.X = Value.X;
			Tree.Y = Value.Y;
			Tree.GroundM = Value.Z;
			Tree.Kind = Value.Variant == 0 ? dtg::sim::PlantKind::Palm : dtg::sim::PlantKind::Oak;
			Tree.HeightM = Value.Variant == 0 ? 9.5 : 8.0;
			Tree.RadiusM = Value.Variant == 0 ? 2.5 : 4.0;
			Tree.Spin = Value.Yaw;
			const int32 Model = DTGPlantModels::ModelFor(Tree);
			if (Model != INDEX_NONE)
			{
				PerTree[Model].Add(DTGPlantModels::TransformFor(Tree, Model));
			}
			continue;
		}
		const FTransform Placed = DTGPropModels::TransformFor(Value);
		for (const DTGPropModels::FPart& Piece : DTGPropModels::PartsFor(Value.Kind, Value.Variant))
		{
			PerMesh[Piece.Mesh].Add(Piece.Local * Placed);
		}
		if (Value.Kind == dtg::sim::PropKind::StreetLamp)
		{
			// The lamp lights its patch of street after dark: a warm, unshadowed point light at the head of
			// the arm, which reaches over the street along the lamp's X. It comes up with the dark (Tick),
			// since against this game's six-unit sun a lit lamp would pool white on a sunny street.
			UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
			Lamp->SetupAttachment(GetRootComponent());
			Lamp->SetMobility(EComponentMobility::Movable);
			Lamp->SetIntensityUnits(ELightUnits::Lumens);
			Lamp->SetIntensity(detail_worldbuilder::LampLumens * LampLevel);
			Lamp->SetLightColor(FLinearColor(1.0f, 0.82f, 0.58f));
			Lamp->SetAttenuationRadius(1400.0f);
			Lamp->SetCastShadows(false);
			Lamp->RegisterComponent();
			Lamp->SetWorldLocation(Placed.TransformPosition(FVector(55.0, 0.0, 370.0)));
			LampLights.Add(Lamp);
			LampLightParts.Add(Part);
		}
	}
	// The kerb itself: Megascans curb stones laid end to end along every block edge in the chunk, each its
	// own length, stopping short of the corners. The kerb quad the streets are built with stays as the
	// step; the stones stand two centimetres proud of it so the two never fight, and sunk so their top is
	// the sidewalk's. Nothing is laid without the kit, which comes in through the editor's Fab window.
	if (DTGPropModels::Mesh(DTGPropModels::CurbMesh(0)))
	{
		const double Proud = 0.02;
		const double HalfWide = 0.13;
		const double Tall = 0.30;
		const double Margin = 0.4;
		int32 Variant = 0;
		for (const dtg::sim::Rect& Block : Game->Sim().City.Blocks)
		{
			// No kerb stones round a crossroads community's blocks: it has no kerbs.
			const CityInfo* const Town = Game->Sim().City.CityAt(Block.CenterX(), Block.CenterY());
			if (Town != nullptr && Town->Style == CityStyle::Rural)
			{
				continue;
			}
			if (!Block.Overlaps(Area))
			{
				continue;
			}
			// Four edges from the south-west corner, each stone's X along the edge and its Y into the block.
			const double Starts[4][2] = {{Block.MinX, Block.MinY}, {Block.MaxX, Block.MinY}, {Block.MaxX, Block.MaxY}, {Block.MinX, Block.MaxY}};
			const double Dirs[4][2] = {{1.0, 0.0}, {0.0, 1.0}, {-1.0, 0.0}, {0.0, -1.0}};
			for (int32 Edge = 0; Edge < 4; ++Edge)
			{
				const double AlongX = Dirs[Edge][0];
				const double AlongY = Dirs[Edge][1];
				const double IntoX = -AlongY;
				const double IntoY = AlongX;
				const double Length = Edge % 2 == 0 ? Block.Width() : Block.Depth();
				double Along = Margin;
				while (true)
				{
					const int32 Mesh = DTGPropModels::CurbMesh(Variant++);
					const double PieceM = DTGPropModels::CurbLengthM(Mesh);
					if (Along + PieceM > Length - Margin)
					{
						break;
					}
					const double X = Starts[Edge][0] + AlongX * Along + IntoX * (HalfWide - Proud);
					const double Y = Starts[Edge][1] + AlongY * Along + IntoY * (HalfWide - Proud);
					if (Area.Contains(X, Y))
					{
						const double Z = Game->Sim().Map.HeightAt(X, Y) + Rules.KerbM - Tall;
						PerMesh[Mesh].Add(FTransform(FRotator(0.0, 90.0 * Edge, 0.0),
							FVector(X * DTGMetresToUnreal, Y * DTGMetresToUnreal, Z * DTGMetresToUnreal)));
					}
					Along += PieceM;
				}
			}
		}
	}
	// Grass clumps on the lawns, drawn with the grass models on the plant components. Their size and spin
	// come from the clump itself, so a reloaded chunk shows the same lawn.
	for (const dtg::sim::Prop& Clump : dtg::sim::GroundCoverIn(Game->Sim().Map, Game->Sim().City, Area))
	{
		const int32 Model = DTGPlantModels::GrassModel(Clump.Variant);
		const UStaticMesh* Source = DTGPlantModels::Mesh(Model);
		if (!Source)
		{
			continue;
		}
		const FBoxSphereBounds Bounds = Source->GetBounds();
		// The scans are small tufts a few tens of centimetres across, sitting wherever they were scanned:
		// scaled to about a metre, varied a little, and centred on the clump with their base on the lawn.
		const double Across = 2.0 * FMath::Max(1.0, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
		const double Scale = (0.8 + 0.4 * std::fmod(Clump.Yaw * 1.1141, 1.0)) * FMath::Clamp(95.0 / Across, 1.0, 4.0);
		const FTransform Centre(FQuat::Identity, FVector(-Bounds.Origin.X, -Bounds.Origin.Y, -(Bounds.Origin.Z - Bounds.BoxExtent.Z)));
		const FTransform Placed(FRotator(0.0, FMath::RadiansToDegrees(Clump.Yaw), 0.0),
			FVector(Clump.X * DTGMetresToUnreal, Clump.Y * DTGMetresToUnreal, Clump.Z * DTGMetresToUnreal), FVector(Scale, Scale, Scale));
		PerTree[Model].Add(Centre * Placed);
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
	for (int32 Model = 0; Model < PerTree.Num(); ++Model)
	{
		if (PerTree[Model].IsEmpty())
		{
			continue;
		}
		UInstancedStaticMeshComponent* Instances = PlantComponent(Part, Model);
		Instances->AddInstances(PerTree[Model], /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
		Instances->SetVisibility(true);
	}
}

void ADTGWorldBuilder::BuildChunkLots(int32 ChunkX, int32 ChunkY, int32 Part, UProceduralMeshComponent* Mesh, const std::vector<dtg::sim::LotFeatures>& Lots)
{
	(void)ChunkX;
	(void)ChunkY;
	if (Mesh == nullptr)
	{
		return;
	}
	// Sections 3 to 5 of the chunk's component: concrete, gravel, and the painted and built things.
	Mesh->ClearMeshSection(3);
	Mesh->ClearMeshSection(4);
	Mesh->ClearMeshSection(5);
	if (Lots.empty())
	{
		return;
	}
	const dtg::sim::StatePlan& Map = Game->Sim().Map;
	const double S = DTGMetresToUnreal;
	const FLinearColor ConcreteGrey(0.55f, 0.54f, 0.5f);
	const FLinearColor Gravel(0.5f, 0.47f, 0.4f);
	const FLinearColor Post(0.22f, 0.16f, 0.1f);
	const FLinearColor BoxPaint[] = {FLinearColor(0.08f, 0.08f, 0.08f), FLinearColor(0.7f, 0.7f, 0.68f), FLinearColor(0.3f, 0.06f, 0.05f)};
	const FLinearColor Headwall(0.5f, 0.49f, 0.46f);
	const FLinearColor Dark(0.03f, 0.03f, 0.03f);
	const FLinearColor ShedPaint[] = {FLinearColor(0.6f, 0.58f, 0.5f), FLinearColor(0.3f, 0.32f, 0.3f), FLinearColor(0.55f, 0.45f, 0.3f), FLinearColor(0.72f, 0.72f, 0.7f)};
	const FLinearColor Tin(0.4f, 0.4f, 0.42f);
	FDTGMesh Concrete;
	FDTGMesh Dirt;
	FDTGMesh Built;
	TArray<FTransform> Panels;
	const double PanelM = DTGPropModels::FenceLengthM();
	for (const dtg::sim::LotFeatures& Lot : Lots)
	{
		// The drive, the pad and the walk, laid flat at the lot's level: over the ditch the drive is a
		// little bridge, with the culvert's headwalls either side of it below.
		const double Level = Map.HeightAt(Lot.Pad.CenterX(), Lot.Pad.CenterY()) + 0.06;
		FDTGMesh& Paving = Lot.Paving == dtg::sim::LotPaving::Concrete ? Concrete : Dirt;
		const FLinearColor& PavingColour = Lot.Paving == dtg::sim::LotPaving::Concrete ? ConcreteGrey : Gravel;
		AddSlab(Paving, Lot.Driveway.MinX, Lot.Driveway.MinY, Lot.Driveway.MaxX, Lot.Driveway.MaxY, Level, PavingColour);
		AddSlab(Paving, Lot.Pad.MinX, Lot.Pad.MinY, Lot.Pad.MaxX, Lot.Pad.MaxY, Level, PavingColour);
		AddSlab(Concrete, Lot.Path.MinX, Lot.Path.MinY, Lot.Path.MaxX, Lot.Path.MaxY, Level + 0.01, ConcreteGrey);
		if (Lot.bCulvert)
		{
			// Two headwalls flanking the drive, standing in the ditch, with a dark mouth each.
			const bool bAlongX = FMath::Abs(FMath::Cos(Lot.CulvertYaw)) > 0.5;
			const double Ground = Map.HeightAt(Lot.CulvertX, Lot.CulvertY);
			const double HalfDrive = 0.5 * 3.2 + 0.25;
			for (const double Side : {-1.0, 1.0})
			{
				const double Cx = Lot.CulvertX + (bAlongX ? 0.0 : Side * HalfDrive);
				const double Cy = Lot.CulvertY + (bAlongX ? Side * HalfDrive : 0.0);
				const double Hx = bAlongX ? 0.8 : 0.15;
				const double Hy = bAlongX ? 0.15 : 0.8;
				// Up to just under the drive's surface, which bridges the ditch above them.
				Built.AddBox(FVector((Cx - Hx) * S, (Cy - Hy) * S, (Ground - 0.1) * S), FVector((Cx + Hx) * S, (Cy + Hy) * S, (Level - 0.1) * S), Headwall, false);
				const double Mx = bAlongX ? 0.3 : 0.16;
				const double My = bAlongX ? 0.16 : 0.3;
				Built.AddBox(FVector((Cx - Mx) * S, (Cy - My) * S, (Ground + 0.02) * S), FVector((Cx + Mx) * S, (Cy + My) * S, FMath::Min(Ground + 0.45, Level - 0.15) * S), Dark, false);
			}
		}
		if (Lot.bMailbox)
		{
			const double Ground = Map.HeightAt(Lot.MailboxX, Lot.MailboxY);
			const bool bLongX = FMath::Abs(FMath::Cos(Lot.MailboxYaw)) > 0.5;
			Built.AddBox(FVector((Lot.MailboxX - 0.05) * S, (Lot.MailboxY - 0.05) * S, Ground * S), FVector((Lot.MailboxX + 0.05) * S, (Lot.MailboxY + 0.05) * S, (Ground + 1.1) * S), Post);
			const double Lx = bLongX ? 0.26 : 0.1;
			const double Ly = bLongX ? 0.1 : 0.26;
			Built.AddBox(FVector((Lot.MailboxX - Lx) * S, (Lot.MailboxY - Ly) * S, (Ground + 1.1) * S), FVector((Lot.MailboxX + Lx) * S, (Lot.MailboxY + Ly) * S, (Ground + 1.32) * S),
				BoxPaint[static_cast<uint32>(Lot.BuildingId) % 3u], false);
		}
		if (Lot.bShed)
		{
			const double Ground = Map.HeightAt(Lot.Shed.CenterX(), Lot.Shed.CenterY());
			const FVector Min(Lot.Shed.MinX * S, Lot.Shed.MinY * S, (Ground + 0.05) * S);
			const FVector Max(Lot.Shed.MaxX * S, Lot.Shed.MaxY * S, (Ground + 2.3) * S);
			Built.AddBox(Min, Max, ShedPaint[static_cast<uint32>(Lot.BuildingId) % 4u]);
			Built.AddRoof(FVector(Min.X - 0.25 * S, Min.Y - 0.25 * S, Max.Z), FVector(Max.X + 0.25 * S, Max.Y + 0.25 * S, Max.Z), 0.9 * S, Tin, true);
		}
		for (const dtg::sim::LotLine& Line : Lot.Fence)
		{
			const double Dx = Line.Bx - Line.Ax;
			const double Dy = Line.By - Line.Ay;
			const double Length = FMath::Sqrt(Dx * Dx + Dy * Dy);
			if (Length < PanelM)
			{
				continue;
			}
			const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx));
			for (double Along = 0.0; Along + PanelM <= Length + 1e-6; Along += PanelM)
			{
				const double X = Line.Ax + Dx / Length * Along;
				const double Y = Line.Ay + Dy / Length * Along;
				Panels.Add(FTransform(FRotator(0.0, Yaw, 0.0), FVector(X * S, Y * S, Map.HeightAt(X, Y) * S)));
			}
		}
	}
	UE_LOG(LogDTG, Log, TEXT("Lots in chunk %d,%d: %d lots; concrete %d, gravel %d, built %d vertices; %d fence panels"), ChunkX, ChunkY,
		static_cast<int32>(Lots.size()), Concrete.Verts.Num(), Dirt.Verts.Num(), Built.Verts.Num(), Panels.Num());
	// The drives carry cars, so they cook collision: a few quads per chunk, cooked with the chunk.
	if (!Concrete.IsEmpty())
	{
		Mesh->CreateMeshSection_LinearColor(3, Concrete.Verts, Concrete.Tris, Concrete.Normals, Concrete.Uvs, Concrete.Colors, Concrete.Tangents, /*bCreateCollision*/ true);
		Mesh->SetMaterial(3, DTGSurface::Material(EDTGSurface::Concrete));
	}
	if (!Dirt.IsEmpty())
	{
		Mesh->CreateMeshSection_LinearColor(4, Dirt.Verts, Dirt.Tris, Dirt.Normals, Dirt.Uvs, Dirt.Colors, Dirt.Tangents, /*bCreateCollision*/ true);
		Mesh->SetMaterial(4, DTGSurface::Material(EDTGSurface::Sand));
	}
	if (!Built.IsEmpty())
	{
		Mesh->CreateMeshSection_LinearColor(5, Built.Verts, Built.Tris, Built.Normals, Built.Uvs, Built.Colors, Built.Tangents, /*bCreateCollision*/ false);
		Mesh->SetMaterial(5, DTGSurface::Material(EDTGSurface::Matte));
	}
	if (!Panels.IsEmpty() && DTGPropModels::Mesh(DTGPropModels::FenceMesh()))
	{
		UInstancedStaticMeshComponent* Instances = PropComponent(Part, DTGPropModels::FenceMesh());
		Instances->AddInstances(Panels, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
		Instances->SetVisibility(true);
	}
}

void ADTGWorldBuilder::BuildChunkWires(int32 ChunkX, int32 ChunkY, UProceduralMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->ClearMeshSection(2);
	if (!DTGPropModels::Ready())
	{
		return; // no poles, so no lines
	}
	const double OriginX = ChunkX * static_cast<double>(ChunkSizeM);
	const double OriginY = ChunkY * static_cast<double>(ChunkSizeM);
	const dtg::sim::Rect Area{OriginX, OriginY, OriginX + ChunkSizeM, OriginY + ChunkSizeM};
	const std::vector<dtg::sim::WireSpan> Spans = dtg::sim::WiresIn(Game->Sim().Map, Game->Sim().City, Area);
	if (Spans.empty())
	{
		return;
	}
	// Three lines a span, sagging between the poles, each a thin cross of two ribbons so it reads from any angle.
	const double S = DTGMetresToUnreal;
	const FLinearColor Dark(0.02f, 0.02f, 0.022f);
	const double HalfWidth = 1.8;
	const double Sag = 95.0;
	constexpr int32 Segments = 10;
	FDTGMesh Lines;
	for (const dtg::sim::WireSpan& Span : Spans)
	{
		const FVector A(Span.Ax * S, Span.Ay * S, Span.Az * S);
		const FVector B(Span.Bx * S, Span.By * S, Span.Bz * S);
		const FVector Side = FVector(-(B.Y - A.Y), B.X - A.X, 0.0).GetSafeNormal();
		const double Offsets[3] = {-85.0, 0.0, 85.0};
		const double Lifts[3] = {0.0, 18.0, 0.0};
		for (int32 Line = 0; Line < 3; ++Line)
		{
			FVector Previous = FVector::ZeroVector;
			for (int32 Step = 0; Step <= Segments; ++Step)
			{
				const double T = static_cast<double>(Step) / Segments;
				const FVector Point = A + (B - A) * T + Side * Offsets[Line] + FVector(0.0, 0.0, Lifts[Line] - Sag * 4.0 * T * (1.0 - T));
				if (Step > 0)
				{
					const FVector Up(0.0, 0.0, HalfWidth);
					Lines.AddQuad(Previous - Side * HalfWidth, Point - Side * HalfWidth, Point + Side * HalfWidth, Previous + Side * HalfWidth,
						FVector::UpVector, Dark);
					Lines.AddQuad(Previous - Side * HalfWidth, Point - Side * HalfWidth, Point + Side * HalfWidth, Previous + Side * HalfWidth,
						-FVector::UpVector, Dark);
					Lines.AddQuad(Previous - Up, Point - Up, Point + Up, Previous + Up, Side, Dark);
					Lines.AddQuad(Previous - Up, Point - Up, Point + Up, Previous + Up, -Side, Dark);
				}
				Previous = Point;
			}
		}
	}
	Mesh->CreateMeshSection_LinearColor(2, Lines.Verts, Lines.Tris, Lines.Normals, Lines.Uvs, Lines.Colors, Lines.Tangents, /*bCreateCollision*/ false);
	Mesh->SetMaterial(2, DTGSurface::Material(EDTGSurface::Rubber));
}

void ADTGWorldBuilder::BuildChunkPlants(int32 ChunkX, int32 ChunkY, int32 Part, UProceduralMeshComponent* Mesh, const std::vector<dtg::sim::Plant>& Wild)
{
	if (!bPlantVegetation || Mesh == nullptr)
	{
		return;
	}
	const double S = DTGMetresToUnreal;
	const double OriginX = ChunkX * static_cast<double>(ChunkSizeM);
	const double OriginY = ChunkY * static_cast<double>(ChunkSizeM);
	const dtg::sim::Rect Area{OriginX, OriginY, OriginX + ChunkSizeM, OriginY + ChunkSizeM};

	// What grew wild, and the shrubs the lots plant along their front walls (Sim/Lots).
	std::vector<dtg::sim::Plant> Grown = Wild;
	for (const dtg::sim::LotFeatures& Lot : dtg::sim::LotsIn(Game->Sim().Map, Game->Sim().City, Area))
	{
		for (const dtg::sim::Plant& Shrub : Lot.Shrubs)
		{
			if (Area.Contains(Shrub.X, Shrub.Y))
			{
				Grown.push_back(Shrub);
			}
		}
	}
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
		Section = Meshes.Apply(Buildings, Section, /*bCollision*/ false); // cooked once, in CookBuildingCollision
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
	StandMeshes.Apply(Buildings, Buildings->GetNumSections(), /*bCollision*/ false); // cooked once, in CookBuildingCollision
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
	// The street lamps follow the sky's night level. Every light is touched when it moves, so only when
	// it has moved a step: a dusk is a few dozen steps, not a few thousand frames of light updates.
	if (!Sky)
	{
		Sky = Cast<ADTGSky>(UGameplayStatics::GetActorOfClass(GetWorld(), ADTGSky::StaticClass()));
	}
	const float WantedLevel = Sky ? static_cast<float>(Sky->Night()) : 1.0f;
	if (FMath::Abs(WantedLevel - LampLevel) > 0.02f)
	{
		LampLevel = WantedLevel;
		for (UPointLightComponent* Lamp : LampLights)
		{
			if (Lamp)
			{
				Lamp->SetIntensity(detail_worldbuilder::LampLumens * LampLevel);
			}
		}
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
	const double OriginX = ChunkX * static_cast<double>(ChunkSizeM);
	const double OriginY = ChunkY * static_cast<double>(ChunkSizeM);
	const dtg::sim::Rect Area{OriginX, OriginY, OriginX + ChunkSizeM, OriginY + ChunkSizeM};
	// A crossroads community's ground carries a ditch three metres wide beside every street, which a
	// vertex every three metres cannot hold: there, a vertex every metre.
	float Spacing0 = FMath::Max(1.0f, VertexSpacingM);
	for (const dtg::sim::StateCity& Town : Map.Cities)
	{
		const dtg::sim::Rect Near{Town.Bounds.MinX - 8.0, Town.Bounds.MinY - 8.0, Town.Bounds.MaxX + 8.0, Town.Bounds.MaxY + 8.0};
		if (Town.Style == dtg::sim::CityStyle::Rural && Near.Overlaps(Area))
		{
			Spacing0 = 1.0f;
		}
	}
	const int32 Cells = FMath::Max(1, FMath::RoundToInt(ChunkSizeM / Spacing0));
	const double Spacing = static_cast<double>(ChunkSizeM) / Cells;
	// What grows here is decided once for the ground's colour, the plants and the lots alike.
	dtg::sim::PlantingRules PlantRules;
	const std::vector<dtg::sim::Plant> Grown = bPlantVegetation ? dtg::sim::PlantsIn(Map, Game->Sim().City, Area, PlantRules) : std::vector<dtg::sim::Plant>();
	const std::vector<dtg::sim::LotFeatures> Lots = dtg::sim::LotsIn(Map, Game->Sim().City, Area);
	// Pine straw: the ground under a pine or an oak is needles and leaf litter, not lawn. A coarse grid
	// of the trees so each vertex asks only its neighbours; the trees just over the chunk's edge count
	// too, or the litter would stop dead at the seam.
	const dtg::sim::Rect Wider{OriginX - 8.0, OriginY - 8.0, OriginX + ChunkSizeM + 8.0, OriginY + ChunkSizeM + 8.0};
	const std::vector<dtg::sim::Plant> Shading = bPlantVegetation ? dtg::sim::PlantsIn(Map, Game->Sim().City, Wider, PlantRules) : std::vector<dtg::sim::Plant>();
	const double Bucket = 8.0;
	const int32 Buckets = FMath::CeilToInt(ChunkSizeM / Bucket) + 4;
	TArray<TArray<int32>> Shade;
	Shade.SetNum(Buckets * Buckets);
	for (int32 Index = 0; Index < static_cast<int32>(Shading.size()); ++Index)
	{
		const dtg::sim::Plant& Tree = Shading[Index];
		if (Tree.Kind != dtg::sim::PlantKind::Pine && Tree.Kind != dtg::sim::PlantKind::Oak)
		{
			continue;
		}
		const int32 Bx = FMath::Clamp(FMath::FloorToInt((Tree.X - OriginX) / Bucket) + 2, 0, Buckets - 1);
		const int32 By = FMath::Clamp(FMath::FloorToInt((Tree.Y - OriginY) / Bucket) + 2, 0, Buckets - 1);
		Shade[By * Buckets + Bx].Add(Index);
	}
	auto Litter = [&](double X, double Y)
	{
		const int32 Bx = FMath::FloorToInt((X - OriginX) / Bucket) + 2;
		const int32 By = FMath::FloorToInt((Y - OriginY) / Bucket) + 2;
		double Nearest = 1e9;
		for (int32 J = FMath::Max(0, By - 1); J <= FMath::Min(Buckets - 1, By + 1); ++J)
		{
			for (int32 I = FMath::Max(0, Bx - 1); I <= FMath::Min(Buckets - 1, Bx + 1); ++I)
			{
				for (const int32 Index : Shade[J * Buckets + I])
				{
					const dtg::sim::Plant& Tree = Shading[Index];
					const double Reach = FMath::Max(2.5, 0.8 * Tree.RadiusM);
					const double D = FMath::Sqrt((Tree.X - X) * (Tree.X - X) + (Tree.Y - Y) * (Tree.Y - Y)) / Reach;
					Nearest = FMath::Min(Nearest, D);
				}
			}
		}
		return FMath::Clamp(1.2 - Nearest, 0.0, 1.0); // 1 at the trunk, fading out past the crown
	};

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
			if (!Info.bWater)
			{
				// Needles and leaf litter under the trees: the terrain material reads this colour as its dirt.
				const double Vx = OriginX + I * Spacing;
				const double Vy = OriginY + J * Spacing;
				const float Straw = static_cast<float>(Litter(Vx, Vy));
				Colour = FLinearColor(FMath::Lerp(0.17f, 0.25f, Straw), FMath::Lerp(0.3f, 0.22f, Straw), FMath::Lerp(0.12f, 0.16f, Straw));
				// A sand shoulder beside a crossroads community's street, the first stride off the asphalt and then grass down into the ditch; the
				// terrain material reads the beach colour as its sand.
				// Sand or grass, never a colour between (the material reads the midway colours as clay),
				// with the edge wandering a little with the ground's noise.
				const double OffStreet = Map.StreetEdgeDistance(Vx, Vy);
				if (OffStreet < 0.8 + 0.5 * Map.Noise(Vx, Vy, 3.0))
				{
					Colour = FLinearColor(0.62f, 0.56f, 0.42f);
				}
			}
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
	BuildChunkPlants(ChunkX, ChunkY, Part, Mesh, Grown);
	BuildChunkProps(ChunkX, ChunkY, Part);
	BuildChunkWires(ChunkX, ChunkY, Mesh);
	BuildChunkLots(ChunkX, ChunkY, Part, Mesh, Lots);
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
	Mesh->SetMobility(EComponentMobility::Static);
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
		TerrainParts[Chunks[Index].Part]->ClearMeshSection(2);
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
