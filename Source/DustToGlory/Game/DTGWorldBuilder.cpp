#include "Game/DTGWorldBuilder.h"

#include "DustToGlory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

#include "Game/DTGGameSubsystem.h"
#include "Game/DTGMeshBuilder.h"

using namespace dtg::sim;

ADTGWorldBuilder::ADTGWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	SetRootComponent(Terrain);
	Terrain->bUseAsyncCooking = true;
	Terrain->SetCollisionProfileName(TEXT("BlockAll"));
	Terrain->SetCastShadow(true);

	Surfaces = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surfaces"));
	Surfaces->SetupAttachment(Terrain);
	Surfaces->bUseAsyncCooking = true;
	Surfaces->SetCollisionProfileName(TEXT("BlockAll"));

	Buildings = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Buildings"));
	Buildings->SetupAttachment(Terrain);
	Buildings->bUseAsyncCooking = true;
	Buildings->SetCollisionProfileName(TEXT("BlockAll"));

	Water = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(Terrain);
	// Water is see-through and swum through, not stood on; vehicles test for it through the world builder.
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADTGWorldBuilder::BeginPlay()
{
	Super::BeginPlay();
	Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDTGGameSubsystem>() : nullptr;
	if (!Game || !Game->IsContentLoaded())
	{
		UE_LOG(LogDTG, Error, TEXT("World builder has no loaded content; the world will be empty."));
		return;
	}
	BuildStaticWorld();
	bBuilt = true;
}

UMaterialInterface* ADTGWorldBuilder::SurfaceMaterial()
{
	if (!Painted)
	{
		// Shared with everything else that draws generated geometry, and warns once if it is missing.
		Painted = DTGMeshBuilder::VertexColorMaterial();
	}
	return Painted;
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
	Terrain->ClearAllMeshSections();
	Surfaces->ClearAllMeshSections();
	Buildings->ClearAllMeshSections();
	Water->ClearAllMeshSections();
	Chunks.Reset();
	FreeSections.Reset();
	Pending.Reset();
	NextSection = 0;
	bBuilt = false;
	if (Game && Game->IsContentLoaded())
	{
		BuildStaticWorld();
		bBuilt = true;
	}
}

void ADTGWorldBuilder::BuildStaticWorld()
{
	BuildTownStreets();
	BuildHighways();
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

	FDTGMesh Mesh;
	for (const RoadPlan& Road : City.Roads)
	{
		// A street sits a hair above its town's pad so it reads as a surface, not z-fighting with the ground.
		const double Z = Map.HeightAt(Road.Area.CenterX(), Road.Area.CenterY()) + 0.04;
		AddSlab(Mesh, Road.Area.MinX, Road.Area.MinY, Road.Area.MaxX, Road.Area.MaxY, Z, Asphalt);
	}
	for (const Rect& Block : City.Blocks)
	{
		const double Z = Map.HeightAt(Block.CenterX(), Block.CenterY()) + 0.06;
		AddSlab(Mesh, Block.MinX, Block.MinY, Block.MaxX, Block.MaxY, Z, Sidewalk);
	}
	for (const Rect& Park : City.Parks)
	{
		const double Z = Map.HeightAt(Park.CenterX(), Park.CenterY()) + 0.08;
		AddSlab(Mesh, Park.MinX, Park.MinY, Park.MaxX, Park.MaxY, Z, Grass);
	}
	if (!Mesh.IsEmpty())
	{
		Surfaces->CreateMeshSection_LinearColor(0, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors, Mesh.Tangents, true);
		Surfaces->SetMaterial(0, SurfaceMaterial());
	}
}

void ADTGWorldBuilder::BuildHighways()
{
	const CityPlan& City = Game->Sim().City;
	const double S = DTGMetresToUnreal;
	const FLinearColor Asphalt(0.06f, 0.06f, 0.065f);
	const FLinearColor Line(0.65f, 0.63f, 0.5f);
	FDTGMesh Mesh;

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
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / Step));
		const double Half = 0.5 * Road.WidthM;
		FVector PrevLeft = FVector::ZeroVector;
		FVector PrevRight = FVector::ZeroVector;
		FVector PrevInnerLeft = FVector::ZeroVector;
		FVector PrevInnerRight = FVector::ZeroVector;
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
			// A centre line, so a highway reads as a road rather than a grey ribbon.
			const double LineHalf = 0.12;
			const FVector InnerLeft((Here[0] - Nx * LineHalf) * S, (Here[1] - Ny * LineHalf) * S, Z + 1.0);
			const FVector InnerRight((Here[0] + Nx * LineHalf) * S, (Here[1] + Ny * LineHalf) * S, Z + 1.0);
			if (bHavePrevious)
			{
				Mesh.AddQuad(PrevLeft, Left, Right, PrevRight, FVector::UpVector, Asphalt);
				if (Index % 2 == 0) // a dashed line
				{
					Mesh.AddQuad(PrevInnerLeft, InnerLeft, InnerRight, PrevInnerRight, FVector::UpVector, Line);
				}
			}
			PrevLeft = Left;
			PrevRight = Right;
			PrevInnerLeft = InnerLeft;
			PrevInnerRight = InnerRight;
			bHavePrevious = true;
		}
	}
	if (!Mesh.IsEmpty())
	{
		Surfaces->CreateMeshSection_LinearColor(1, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors, Mesh.Tangents, true);
		Surfaces->SetMaterial(1, SurfaceMaterial());
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
	auto BuildRange = [&](const Rect* Bounds)
	{
		FDTGMesh Mesh;
		for (const BuildingPlan& Building : City.Buildings)
		{
			if (Bounds && !Bounds->Contains(Building.Footprint.CenterX(), Building.Footprint.CenterY()))
			{
				continue;
			}
			const double GroundZ = Map.HeightAt(Building.Footprint.CenterX(), Building.Footprint.CenterY());
			DTGMeshBuilder::BuildBuilding(Building, GroundZ, DTGMetresToUnreal, Mesh);
			++Total;
		}
		if (!Mesh.IsEmpty())
		{
			Buildings->CreateMeshSection_LinearColor(Section, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors, Mesh.Tangents, true);
			Buildings->SetMaterial(Section, SurfaceMaterial());
			++Section;
		}
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
	const CityPlan& City = Game->Sim().City;
	const StatePlan& Map = Game->Sim().Map;
	const FLinearColor Apron(0.21f, 0.2f, 0.19f);
	const FLinearColor Track(0.05f, 0.05f, 0.055f);
	const FLinearColor Stand(0.3f, 0.3f, 0.32f);
	const double S = DTGMetresToUnreal;
	FDTGMesh Mesh;
	for (const VenuePlan& Venue : City.Venues)
	{
		const double Z = Map.HeightAt(Venue.Area.CenterX(), Venue.Area.CenterY());
		AddSlab(Mesh, Venue.Area.MinX, Venue.Area.MinY, Venue.Area.MaxX, Venue.Area.MaxY, Z + 0.02, Apron);
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
		// Grandstands down the long side, so an event has somewhere for a crowd to be.
		const double StandDepth = FMath::Min(14.0, Venue.Area.Depth() * 0.12);
		for (int32 Tier = 0; Tier < 4; ++Tier)
		{
			const double Inset = Tier * StandDepth * 0.25;
			Mesh.AddBox(FVector(Venue.Area.MinX * S, (Venue.Area.MinY - StandDepth + Inset) * S, (Z + Tier * 0.9) * S),
				FVector(Venue.Area.MaxX * S, (Venue.Area.MinY - StandDepth + Inset + StandDepth * 0.25) * S, (Z + (Tier + 1) * 0.9) * S),
				Stand);
		}
	}
	if (!Mesh.IsEmpty())
	{
		Surfaces->CreateMeshSection_LinearColor(2, Mesh.Verts, Mesh.Tris, Mesh.Normals, Mesh.Uvs, Mesh.Colors, Mesh.Tangents, true);
		Surfaces->SetMaterial(2, SurfaceMaterial());
	}
}

void ADTGWorldBuilder::BuildWater()
{
	const StatePlan& Map = Game->Sim().Map;
	const FLinearColor Sea(0.02f, 0.09f, 0.16f);
	const FLinearColor Fresh(0.05f, 0.14f, 0.15f);
	const double S = DTGMetresToUnreal;
	FDTGMesh Mesh;

	// The ocean is one sheet at sea level, reaching well past the coast.
	const Rect& Bounds = Map.Bounds;
	const double Margin = 4000.0;
	AddSlab(Mesh, Bounds.MinX - Margin, Bounds.MinY - Margin, Bounds.MaxX + Margin, Bounds.MaxY + Margin, 0.0, Sea);
	for (const StateLake& Lake : Map.Lakes)
	{
		// A disc rather than a square, so a lake has a shoreline.
		constexpr int32 Sides = 24;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const double A0 = 2.0 * PI * Side / Sides;
			const double A1 = 2.0 * PI * (Side + 1) / Sides;
			Mesh.AddQuad(FVector(Lake.X * S, Lake.Y * S, Lake.LevelM * S),
				FVector((Lake.X + Lake.RadiusM * FMath::Cos(A0)) * S, (Lake.Y + Lake.RadiusM * FMath::Sin(A0)) * S, Lake.LevelM * S),
				FVector((Lake.X + Lake.RadiusM * FMath::Cos(A1)) * S, (Lake.Y + Lake.RadiusM * FMath::Sin(A1)) * S, Lake.LevelM * S),
				FVector(Lake.X * S, Lake.Y * S, Lake.LevelM * S), FVector::UpVector, Fresh);
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
	Water->SetMaterial(0, SurfaceMaterial());
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
			Around = Pawn->GetActorLocation();
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
	const int32 CentreX = FMath::FloorToInt(Around.X / ChunkUu);
	const int32 CentreY = FMath::FloorToInt(Around.Y / ChunkUu);
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
			bool bHave = false;
			for (FDTGTerrainChunk& Chunk : Chunks)
			{
				if (Chunk.X == X && Chunk.Y == Y)
				{
					Chunk.LastSeen = Now;
					bHave = true;
					break;
				}
			}
			if (!bHave)
			{
				Pending.Add(FIntPoint(X, Y));
			}
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
	// Normals from central differences of the heights already sampled above: evaluating the terrain again
	// per vertex would cost four more full surface queries each.
	Normals.Reserve(Verts.Num());
	auto HeightOf = [&Verts, Cells, S](int32 I, int32 J)
	{
		const int32 Ci = FMath::Clamp(I, 0, Cells);
		const int32 Cj = FMath::Clamp(J, 0, Cells);
		return Verts[Cj * (Cells + 1) + Ci].Z / S;
	};
	for (int32 J = 0; J <= Cells; ++J)
	{
		for (int32 I = 0; I <= Cells; ++I)
		{
			const double Dx = HeightOf(I + 1, J) - HeightOf(I - 1, J);
			const double Dy = HeightOf(I, J + 1) - HeightOf(I, J - 1);
			// At the chunk edge the clamp halves the span, so scale the step to match.
			const double SpanX = (I == 0 || I == Cells) ? Spacing : 2.0 * Spacing;
			const double SpanY = (J == 0 || J == Cells) ? Spacing : 2.0 * Spacing;
			Normals.Add(FVector(-Dx * SpanY, -Dy * SpanX, SpanX * SpanY).GetSafeNormal());
		}
	}

	int32 Section = NextSection;
	if (FreeSections.Num() > 0)
	{
		Section = FreeSections.Pop();
	}
	else
	{
		++NextSection;
	}
	Terrain->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, Uvs, Colors, Tangents, /*bCreateCollision*/ true);
	Terrain->SetMaterial(Section, SurfaceMaterial());

	FDTGTerrainChunk Chunk;
	Chunk.X = ChunkX;
	Chunk.Y = ChunkY;
	Chunk.Section = Section;
	Chunk.LastSeen = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Chunks.Add(Chunk);
}

void ADTGWorldBuilder::ReleaseChunk(int32 Index)
{
	if (!Chunks.IsValidIndex(Index))
	{
		return;
	}
	Terrain->ClearMeshSection(Chunks[Index].Section);
	FreeSections.Add(Chunks[Index].Section);
	Chunks.RemoveAtSwap(Index);
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
