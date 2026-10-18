#include "Game/DTGWorldBuilder.h"

#include "DustToGlory.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

#include "Game/DTGGameSubsystem.h"

using namespace dtg::sim;

namespace
{
	/** The cube every building shell and slab instance is scaled from (100 uu on a side, centred). */
	const TCHAR* const CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const BasicMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	FLinearColor FromRgb(uint32 Rgb)
	{
		return FLinearColor(FColor(static_cast<uint8>((Rgb >> 16) & 0xFF), static_cast<uint8>((Rgb >> 8) & 0xFF),
			static_cast<uint8>(Rgb & 0xFF)));
	}
}

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

UMaterialInterface* ADTGWorldBuilder::TintedMaterial(const FLinearColor& Color, const TCHAR* SlotName)
{
	const FString Key(SlotName);
	if (const TObjectPtr<UMaterialInterface>* Found = Materials.Find(Key))
	{
		return Found->Get();
	}
	UMaterialInterface* Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, BasicMaterialPath));
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, this);
	if (Instance)
	{
		// BasicShapeMaterial exposes a single colour parameter.
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		Materials.Add(Key, Instance);
	}
	return Instance;
}

// ============================================================================================ Static world

void ADTGWorldBuilder::AddSlab(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& Uvs,
	TArray<FLinearColor>& Colors, double MinX, double MinY, double MaxX, double MaxY, double Z, const FLinearColor& Color)
{
	const int32 Base = Verts.Num();
	const double S = DTGMetresToUnreal;
	Verts.Add(FVector(MinX * S, MinY * S, Z * S));
	Verts.Add(FVector(MaxX * S, MinY * S, Z * S));
	Verts.Add(FVector(MaxX * S, MaxY * S, Z * S));
	Verts.Add(FVector(MinX * S, MaxY * S, Z * S));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Normals.Add(FVector::UpVector);
		Colors.Add(Color);
	}
	const double TileU = FMath::Max(1.0, (MaxX - MinX) / 4.0);
	const double TileV = FMath::Max(1.0, (MaxY - MinY) / 4.0);
	Uvs.Add(FVector2D(0.0, 0.0));
	Uvs.Add(FVector2D(TileU, 0.0));
	Uvs.Add(FVector2D(TileU, TileV));
	Uvs.Add(FVector2D(0.0, TileV));
	// Wound so the face normal (UE's Cross(P2-P0, P1-P0)) points up, matching the normals above.
	Tris.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
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
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> Uvs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	const FLinearColor Asphalt(0.055f, 0.055f, 0.06f);
	const FLinearColor Sidewalk(0.42f, 0.41f, 0.39f);
	const FLinearColor Grass(0.16f, 0.27f, 0.11f);
	// One section per surface, because each one is drawn with its own tinted material.
	auto Flush = [&](int32 Section, const FLinearColor& Colour)
	{
		if (Verts.Num() > 0)
		{
			Surfaces->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, Uvs, Colors, Tangents, true);
			Surfaces->SetMaterial(Section, TintedMaterial(Colour, *FString::Printf(TEXT("Surface%d"), Section)));
		}
		Verts.Reset();
		Tris.Reset();
		Normals.Reset();
		Uvs.Reset();
		Colors.Reset();
	};
	for (const RoadPlan& Road : City.Roads)
	{
		// A street sits a hair above its town's pad so it reads as a surface, not z-fighting with the ground.
		const double Z = Map.HeightAt(Road.Area.CenterX(), Road.Area.CenterY()) + 0.04;
		AddSlab(Verts, Tris, Normals, Uvs, Colors, Road.Area.MinX, Road.Area.MinY, Road.Area.MaxX, Road.Area.MaxY, Z, Asphalt);
	}
	Flush(0, Asphalt);
	for (const Rect& Block : City.Blocks)
	{
		const double Z = Map.HeightAt(Block.CenterX(), Block.CenterY()) + 0.06;
		AddSlab(Verts, Tris, Normals, Uvs, Colors, Block.MinX, Block.MinY, Block.MaxX, Block.MaxY, Z, Sidewalk);
	}
	Flush(3, Sidewalk);
	for (const Rect& Park : City.Parks)
	{
		const double Z = Map.HeightAt(Park.CenterX(), Park.CenterY()) + 0.08;
		AddSlab(Verts, Tris, Normals, Uvs, Colors, Park.MinX, Park.MinY, Park.MaxX, Park.MaxY, Z, Grass);
	}
	Flush(4, Grass);
}

void ADTGWorldBuilder::BuildHighways()
{
	const CityPlan& City = Game->Sim().City;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> Uvs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	const double S = DTGMetresToUnreal;
	const FLinearColor Asphalt(0.06f, 0.06f, 0.065f);

	for (const HighwayPlan& Road : City.Highways)
	{
		const double Length = Road.LengthM();
		if (Length < 1.0 || Road.Points.size() < 2)
		{
			continue;
		}
		// Walk the road at the same spacing its deck heights were sampled at, so bridges and ramps come
		// through exactly as the simulation graded them.
		const double Step = FMath::Max(2.0, Road.DeckStepM);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / Step));
		const double Half = 0.5 * Road.WidthM;
		int32 Previous = INDEX_NONE;
		for (int32 Index = 0; Index <= Steps; ++Index)
		{
			const double Along = FMath::Min(Length, Index * Step);
			const std::array<double, 3> Here = Road.At(Along);
			const std::array<double, 3> Ahead = Road.At(FMath::Min(Length, Along + 1.0));
			double DirX = Ahead[0] - Here[0];
			double DirY = Ahead[1] - Here[1];
			if (FMath::Abs(DirX) + FMath::Abs(DirY) < 1e-6 && Index > 0)
			{
				const std::array<double, 3> Behind = Road.At(FMath::Max(0.0, Along - 1.0));
				DirX = Here[0] - Behind[0];
				DirY = Here[1] - Behind[1];
			}
			const double Len = FMath::Max(1e-6, FMath::Sqrt(DirX * DirX + DirY * DirY));
			const double NormalX = -DirY / Len;
			const double NormalY = DirX / Len;
			const int32 Base = Verts.Num();
			Verts.Add(FVector((Here[0] - NormalX * Half) * S, (Here[1] - NormalY * Half) * S, (Here[2] + 0.05) * S));
			Verts.Add(FVector((Here[0] + NormalX * Half) * S, (Here[1] + NormalY * Half) * S, (Here[2] + 0.05) * S));
			Normals.Add(FVector::UpVector);
			Normals.Add(FVector::UpVector);
			Colors.Add(Asphalt);
			Colors.Add(Asphalt);
			Uvs.Add(FVector2D(0.0, Along / 8.0));
			Uvs.Add(FVector2D(1.0, Along / 8.0));
			if (Previous != INDEX_NONE)
			{
				Tris.Append({Previous, Previous + 1, Base + 1, Previous, Base + 1, Base});
			}
			Previous = Base;
		}
	}
	if (Verts.Num() > 0)
	{
		Surfaces->CreateMeshSection_LinearColor(1, Verts, Tris, Normals, Uvs, Colors, Tangents, true);
		Surfaces->SetMaterial(1, TintedMaterial(FLinearColor(0.08f, 0.08f, 0.085f), TEXT("Highways")));
	}
}

void ADTGWorldBuilder::BuildBuildings()
{
	UStaticMesh* Cube = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, CubePath));
	if (!Cube)
	{
		UE_LOG(LogDTG, Error, TEXT("Could not load %s; buildings will be missing."), CubePath);
		return;
	}
	const CityPlan& City = Game->Sim().City;
	const StatePlan& Map = Game->Sim().Map;
	const double S = DTGMetresToUnreal;
	for (const BuildingPlan& Building : City.Buildings)
	{
		// One instanced component per role: every shop shares a draw call, and the role decides the colour.
		const FName Key(LotRoleName(Building.Role));
		TObjectPtr<UInstancedStaticMeshComponent>* Found = BuildingMeshes.Find(Key);
		if (!Found)
		{
			UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, Key);
			Component->SetStaticMesh(Cube);
			Component->SetMaterial(0, TintedMaterial(FromRgb(Building.ColorRgb), *Key.ToString()));
			Component->SetCollisionProfileName(TEXT("BlockAll"));
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetupAttachment(Terrain);
			Component->RegisterComponent();
			Found = &BuildingMeshes.Add(Key, Component);
		}
		const double Height = FMath::Max(3.0, Building.HeightM);
		const double GroundZ = Map.HeightAt(Building.Footprint.CenterX(), Building.Footprint.CenterY());
		const FVector Centre((Building.Footprint.CenterX()) * S, (Building.Footprint.CenterY()) * S, (GroundZ + 0.5 * Height) * S);
		// The engine cube is 100 uu on a side, so scale is simply the size in metres.
		const FVector Scale(Building.Footprint.Width(), Building.Footprint.Depth(), Height);
		(*Found)->AddInstance(FTransform(FRotator::ZeroRotator, Centre, Scale), /*bWorldSpace*/ true);
	}
	int32 Total = 0;
	for (const TPair<FName, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : BuildingMeshes)
	{
		Total += Pair.Value->GetInstanceCount();
	}
	UE_LOG(LogDTG, Log, TEXT("Placed %d building shells in %d roles"), Total, BuildingMeshes.Num());
}

void ADTGWorldBuilder::BuildVenues()
{
	const CityPlan& City = Game->Sim().City;
	const StatePlan& Map = Game->Sim().Map;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> Uvs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	const FLinearColor Apron(0.21f, 0.2f, 0.19f);
	const FLinearColor Track(0.05f, 0.05f, 0.055f);
	for (const VenuePlan& Venue : City.Venues)
	{
		const double Z = Map.HeightAt(Venue.Area.CenterX(), Venue.Area.CenterY());
		AddSlab(Verts, Tris, Normals, Uvs, Colors, Venue.Area.MinX, Venue.Area.MinY, Venue.Area.MaxX, Venue.Area.MaxY, Z + 0.02, Apron);
		// The racing surface itself follows the venue's centreline.
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
			const int32 Base = Verts.Num();
			const double S = DTGMetresToUnreal;
			Verts.Add(FVector((A[0] - Nx) * S, (A[1] - Ny) * S, (Z + 0.06) * S));
			Verts.Add(FVector((A[0] + Nx) * S, (A[1] + Ny) * S, (Z + 0.06) * S));
			Verts.Add(FVector((B[0] + Nx) * S, (B[1] + Ny) * S, (Z + 0.06) * S));
			Verts.Add(FVector((B[0] - Nx) * S, (B[1] - Ny) * S, (Z + 0.06) * S));
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				Normals.Add(FVector::UpVector);
				Colors.Add(Track);
			}
			Uvs.Append({FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, Len / 8.0), FVector2D(0, Len / 8.0)});
			Tris.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		}
	}
	if (Verts.Num() > 0)
	{
		Surfaces->CreateMeshSection_LinearColor(2, Verts, Tris, Normals, Uvs, Colors, Tangents, true);
		Surfaces->SetMaterial(2, TintedMaterial(FLinearColor(0.18f, 0.18f, 0.18f), TEXT("Venues")));
	}
}

void ADTGWorldBuilder::BuildWater()
{
	const StatePlan& Map = Game->Sim().Map;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> Uvs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	const FLinearColor Sea(0.02f, 0.09f, 0.16f);
	const FLinearColor Fresh(0.05f, 0.14f, 0.15f);

	// The ocean is one sheet at sea level, reaching well past the coast.
	const Rect& Bounds = Map.Bounds;
	const double Margin = 4000.0;
	AddSlab(Verts, Tris, Normals, Uvs, Colors, Bounds.MinX - Margin, Bounds.MinY - Margin, Bounds.MaxX + Margin, Bounds.MaxY + Margin, 0.0, Sea);
	for (const StateLake& Lake : Map.Lakes)
	{
		AddSlab(Verts, Tris, Normals, Uvs, Colors, Lake.X - Lake.RadiusM, Lake.Y - Lake.RadiusM, Lake.X + Lake.RadiusM,
			Lake.Y + Lake.RadiusM, Lake.LevelM, Fresh);
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
			const double S = DTGMetresToUnreal;
			const int32 Base = Verts.Num();
			const double ZA = FMath::Max(0.2, Map.NaturalHeight(A[0], A[1]) - 0.8);
			const double ZB = FMath::Max(0.2, Map.NaturalHeight(B[0], B[1]) - 0.8);
			Verts.Add(FVector((A[0] - Nx) * S, (A[1] - Ny) * S, ZA * S));
			Verts.Add(FVector((A[0] + Nx) * S, (A[1] + Ny) * S, ZA * S));
			Verts.Add(FVector((B[0] + Nx) * S, (B[1] + Ny) * S, ZB * S));
			Verts.Add(FVector((B[0] - Nx) * S, (B[1] - Ny) * S, ZB * S));
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				Normals.Add(FVector::UpVector);
				Colors.Add(Fresh);
			}
			Uvs.Append({FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, Len / 10.0), FVector2D(0, Len / 10.0)});
			Tris.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		}
	}
	Water->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, Uvs, Colors, Tangents, false);
	Water->SetMaterial(0, TintedMaterial(Sea, TEXT("Water")));
	(void)Fresh;
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
	Terrain->SetMaterial(Section, TintedMaterial(FLinearColor::White, TEXT("Ground")));

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
