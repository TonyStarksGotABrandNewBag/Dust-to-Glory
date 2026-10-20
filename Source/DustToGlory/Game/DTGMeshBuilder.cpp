#include "Game/DTGMeshBuilder.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

#include "DustToGlory.h"
#include "UObject/UObjectGlobals.h"

#include "Game/DTGBody.h"

using namespace dtg::sim;

// ============================================================================================ FDTGMesh

void FDTGMesh::Reset()
{
	Verts.Reset();
	Tris.Reset();
	Normals.Reset();
	Uvs.Reset();
	Colors.Reset();
	Tangents.Reset();
}

void FDTGMesh::Append(const FDTGMesh& Other)
{
	const int32 Base = Verts.Num();
	Verts.Append(Other.Verts);
	Normals.Append(Other.Normals);
	Uvs.Append(Other.Uvs);
	Colors.Append(Other.Colors);
	Tangents.Append(Other.Tangents);
	Tris.Reserve(Tris.Num() + Other.Tris.Num());
	for (const int32 Index : Other.Tris)
	{
		Tris.Add(Base + Index);
	}
}


void FDTGMesh::AddTri(const FVector& A, const FVector& B, const FVector& C, const FVector& Normal, const FLinearColor& Colour)
{
	// For fans and gables, where passing a quad with two corners in the same place leaves a zero-area
	// triangle in the buffer for every real one -- half the lake and half of every roof.
	const int32 Base = Verts.Num();
	Verts.Append({A, B, C});
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Normals.Add(Normal);
		Colors.Add(Colour);
	}
	Uvs.Append({FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0)});
	const bool bForward = FVector::DotProduct(FVector::CrossProduct(C - A, B - A), Normal) >= 0.0;
	if (bForward)
	{
		Tris.Append({Base, Base + 1, Base + 2});
	}
	else
	{
		Tris.Append({Base, Base + 2, Base + 1});
	}
}

void FDTGMesh::AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal,
	const FLinearColor& Colour, double UvScale)
{
	const int32 Base = Verts.Num();
	Verts.Append({A, B, C, D});
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Normals.Add(Normal);
		Colors.Add(Colour);
	}
	// UVs from the quad's own size, so a texture would tile evenly whatever the wall's dimensions.
	const double Wide = FVector::Dist(A, B) * UvScale;
	const double High = FVector::Dist(B, C) * UvScale;
	Uvs.Append({FVector2D(0.0, 0.0), FVector2D(Wide, 0.0), FVector2D(Wide, High), FVector2D(0.0, High)});
	// Unreal derives a face's normal as Cross(C - A, B - A) (see KismetProceduralMeshLibrary), so the
	// winding (A, B, C) is the right way round exactly when that agrees with the normal asked for.
	const bool bForward = FVector::DotProduct(FVector::CrossProduct(C - A, B - A), Normal) >= 0.0;
	if (bForward)
	{
		Tris.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
	}
	else
	{
		Tris.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
	}
}

void FDTGMesh::AddQuadAlpha(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal,
	const FLinearColor& Colour, float AlphaAB, float AlphaCD)
{
	const int32 Base = Verts.Num();
	AddQuad(A, B, C, D, Normal, Colour);
	Colors[Base].A = AlphaAB;
	Colors[Base + 1].A = AlphaAB;
	Colors[Base + 2].A = AlphaCD;
	Colors[Base + 3].A = AlphaCD;
}

void FDTGMesh::AddPanel(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal,
	const FLinearColor& Colour, double RepeatU, double RepeatV, double OffsetU, double OffsetV)
{
	const int32 Base = Verts.Num();
	AddQuad(A, B, C, D, Normal, Colour);
	Uvs[Base] = FVector2D(OffsetU, OffsetV);
	Uvs[Base + 1] = FVector2D(OffsetU + RepeatU, OffsetV);
	Uvs[Base + 2] = FVector2D(OffsetU + RepeatU, OffsetV + RepeatV);
	Uvs[Base + 3] = FVector2D(OffsetU, OffsetV + RepeatV);
}

void FDTGMesh::AddBox(const FVector& Min, const FVector& Max, const FLinearColor& Colour, bool bSkipBottom)
{
	const FVector A(Min.X, Min.Y, Min.Z);
	const FVector B(Max.X, Min.Y, Min.Z);
	const FVector C(Max.X, Max.Y, Min.Z);
	const FVector D(Min.X, Max.Y, Min.Z);
	const FVector E(Min.X, Min.Y, Max.Z);
	const FVector F(Max.X, Min.Y, Max.Z);
	const FVector G(Max.X, Max.Y, Max.Z);
	const FVector H(Min.X, Max.Y, Max.Z);
	AddQuad(E, F, G, H, FVector::UpVector, Colour);
	if (!bSkipBottom)
	{
		AddQuad(A, B, C, D, -FVector::UpVector, Colour);
	}
	AddQuad(A, B, F, E, FVector(0, -1, 0), Colour);
	AddQuad(C, D, H, G, FVector(0, 1, 0), Colour);
	AddQuad(B, C, G, F, FVector(1, 0, 0), Colour);
	AddQuad(D, A, E, H, FVector(-1, 0, 0), Colour);
}

void FDTGMesh::AddTaperedBox(const FVector& Min, const FVector& Max, double InsetFrontX, double InsetBackX, double InsetY,
	const FLinearColor& Colour, bool bSkipBottom)
{
	const double SizeX = Max.X - Min.X;
	const double SizeY = Max.Y - Min.Y;
	const double FrontIn = SizeX * InsetFrontX;
	const double BackIn = SizeX * InsetBackX;
	const double SideIn = SizeY * InsetY;

	const FVector A(Min.X, Min.Y, Min.Z);
	const FVector B(Max.X, Min.Y, Min.Z);
	const FVector C(Max.X, Max.Y, Min.Z);
	const FVector D(Min.X, Max.Y, Min.Z);
	const FVector E(Min.X + BackIn, Min.Y + SideIn, Max.Z);
	const FVector F(Max.X - FrontIn, Min.Y + SideIn, Max.Z);
	const FVector G(Max.X - FrontIn, Max.Y - SideIn, Max.Z);
	const FVector H(Min.X + BackIn, Max.Y - SideIn, Max.Z);

	AddQuad(E, F, G, H, FVector::UpVector, Colour);
	if (!bSkipBottom)
	{
		AddQuad(A, B, C, D, -FVector::UpVector, Colour);
	}
	// Sloping sides: the normal is worked out per face rather than assumed.
	auto Slope = [](const FVector& P0, const FVector& P1, const FVector& P2)
	{
		return FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
	};
	// Slope() already gives the outward normal; AddQuad winds the triangles to match it.
	AddQuad(A, B, F, E, Slope(A, B, F), Colour);
	AddQuad(C, D, H, G, Slope(C, D, H), Colour);
	AddQuad(B, C, G, F, Slope(B, C, G), Colour);
	AddQuad(D, A, E, H, Slope(D, A, E), Colour);
}

void FDTGMesh::AddWheel(const FVector& Centre, double RadiusUu, double WidthUu, const FLinearColor& Tyre, const FLinearColor& Rim, int32 Sides,
	FDTGMesh* RimOut)
{
	FDTGMesh& Faces = RimOut ? *RimOut : *this;
	const double Half = 0.5 * WidthUu;
	for (int32 Side = 0; Side < Sides; ++Side)
	{
		const double A0 = 2.0 * UE_PI * Side / Sides;
		const double A1 = 2.0 * UE_PI * (Side + 1) / Sides;
		const FVector P0(Centre.X + RadiusUu * FMath::Cos(A0), Centre.Y, Centre.Z + RadiusUu * FMath::Sin(A0));
		const FVector P1(Centre.X + RadiusUu * FMath::Cos(A1), Centre.Y, Centre.Z + RadiusUu * FMath::Sin(A1));
		const FVector Out0 = (P0 - Centre).GetSafeNormal();
		// Tread.
		AddQuad(P0 - FVector(0, Half, 0), P1 - FVector(0, Half, 0), P1 + FVector(0, Half, 0), P0 + FVector(0, Half, 0), Out0, Tyre);
		// The two faces, as a fan to the hub.
		const FVector HubIn = Centre - FVector(0, Half, 0);
		const FVector HubOut = Centre + FVector(0, Half, 0);
		Faces.AddTri(HubIn, P0 - FVector(0, Half, 0), P1 - FVector(0, Half, 0), FVector(0, -1, 0), Rim);
		Faces.AddTri(HubOut, P1 + FVector(0, Half, 0), P0 + FVector(0, Half, 0), FVector(0, 1, 0), Rim);
	}
}

bool FDTGSurfaceMeshes::IsEmpty() const
{
	for (const FDTGMesh& Part : Parts)
	{
		if (!Part.IsEmpty())
		{
			return false;
		}
	}
	return true;
}

void FDTGSurfaceMeshes::Reset()
{
	for (FDTGMesh& Part : Parts)
	{
		Part.Reset();
	}
}

int32 FDTGSurfaceMeshes::Apply(UProceduralMeshComponent* Target, int32 FirstSection, bool bCollision) const
{
	int32 Section = FirstSection;
	if (!Target)
	{
		return Section;
	}
	for (int32 Kind = 0; Kind < DTGSurfaceCount; ++Kind)
	{
		const FDTGMesh& Part = Parts[Kind];
		if (Part.IsEmpty())
		{
			continue;
		}
		Target->CreateMeshSection_LinearColor(Section, Part.Verts, Part.Tris, Part.Normals, Part.Uvs, Part.Colors, Part.Tangents, bCollision);
		Target->SetMaterial(Section, DTGSurface::Material(static_cast<EDTGSurface>(Kind)));
		++Section;
	}
	return Section;
}

int32 FDTGSurfaceMeshes::SectionOf(EDTGSurface Kind, int32 FirstSection) const
{
	const int32 Wanted = static_cast<int32>(Kind);
	if (Wanted < 0 || Wanted >= DTGSurfaceCount || Parts[Wanted].IsEmpty())
	{
		return INDEX_NONE;
	}
	int32 Section = FirstSection;
	for (int32 Index = 0; Index < Wanted; ++Index)
	{
		Section += Parts[Index].IsEmpty() ? 0 : 1;
	}
	return Section;
}

void FDTGMesh::AddRoof(const FVector& Min, const FVector& Max, double RidgeHeightUu, const FLinearColor& Colour, bool bGableEnds)
{
	// A ridge running the long way, so a house reads as a house from any angle.
	const bool bAlongX = (Max.X - Min.X) >= (Max.Y - Min.Y);
	const double MidY = 0.5 * (Min.Y + Max.Y);
	const double MidX = 0.5 * (Min.X + Max.X);
	const double Top = Min.Z + RidgeHeightUu;
	if (bAlongX)
	{
		const FVector R0(Min.X, MidY, Top);
		const FVector R1(Max.X, MidY, Top);
		AddQuad(FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), R1, R0,
			FVector::CrossProduct(FVector(Max.X, Min.Y, Min.Z) - FVector(Min.X, Min.Y, Min.Z), R0 - FVector(Min.X, Min.Y, Min.Z)).GetSafeNormal(), Colour);
		AddQuad(FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), R0, R1,
			FVector::CrossProduct(FVector(Min.X, Max.Y, Min.Z) - FVector(Max.X, Max.Y, Min.Z), R1 - FVector(Max.X, Max.Y, Min.Z)).GetSafeNormal(), Colour);
		// Gable ends.
		if (bGableEnds)
		{
			AddTri(FVector(Min.X, Min.Y, Min.Z), R0, FVector(Min.X, Max.Y, Min.Z), FVector(-1, 0, 0), Colour);
			AddTri(FVector(Max.X, Max.Y, Min.Z), R1, FVector(Max.X, Min.Y, Min.Z), FVector(1, 0, 0), Colour);
		}
	}
	else
	{
		const FVector R0(MidX, Min.Y, Top);
		const FVector R1(MidX, Max.Y, Top);
		AddQuad(FVector(Min.X, Min.Y, Min.Z), R0, R1, FVector(Min.X, Max.Y, Min.Z), FVector(-1, 0, 1).GetSafeNormal(), Colour);
		AddQuad(FVector(Max.X, Max.Y, Min.Z), R1, R0, FVector(Max.X, Min.Y, Min.Z), FVector(1, 0, 1).GetSafeNormal(), Colour);
		if (bGableEnds)
		{
			AddTri(FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), R0, FVector(0, -1, 0), Colour);
			AddTri(FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), R1, FVector(0, 1, 0), Colour);
		}
	}
}

// ============================================================================================ Buildings

namespace DTGMeshBuilder
{
	namespace
	{
		FLinearColor FromRgb(uint32 Rgb)
		{
			return FLinearColor(FColor(static_cast<uint8>((Rgb >> 16) & 0xFF), static_cast<uint8>((Rgb >> 8) & 0xFF),
				static_cast<uint8>(Rgb & 0xFF)));
		}

		/** Lit windows after dark would need the clock; these read as glass in daylight. */
		const FLinearColor Glass(0.08f, 0.13f, 0.17f);
		const FLinearColor DarkTrim(0.09f, 0.09f, 0.1f);
	}

	FPalette PaletteFor(const BuildingPlan& Building)
	{
		FPalette Palette;
		Palette.Wall = FromRgb(Building.ColorRgb);
		Palette.Window = Glass;
		Palette.Trim = DarkTrim;
		// Downtown towers are glass and steel; everything else gets a roof you can see. Scaled channel by
		// channel: FLinearColor * float scales alpha too.
		Palette.Roof = Building.bDowntown ? FLinearColor(0.12f, 0.12f, 0.13f)
										  : FLinearColor(Palette.Wall.R * 0.55f, Palette.Wall.G * 0.55f, Palette.Wall.B * 0.55f);
		switch (Building.Role)
		{
		case LotRole::Home:
			Palette.Roof = FLinearColor(0.11f, 0.09f, 0.08f);
			break;
		case LotRole::Church:
			Palette.Roof = FLinearColor(0.21f, 0.17f, 0.14f);
			break;
		case LotRole::Dealership:
		case LotRole::ClassicDealer:
		case LotRole::UsedCarLot:
			// Showrooms are mostly glass.
			Palette.Window = FLinearColor(0.1f, 0.16f, 0.21f);
			break;
		case LotRole::Jail:
		case LotRole::PoliceStation:
			Palette.Window = FLinearColor(0.05f, 0.06f, 0.07f);
			Palette.Roof = FLinearColor(0.15f, 0.15f, 0.16f);
			break;
		default:
			break;
		}
		return Palette;
	}

	EDTGSurface WallSurfaceFor(const BuildingPlan& Building)
	{
		// The same building always gets the same finish, and a street of them does not all match.
		const uint32 Pick = (static_cast<uint32>(Building.Id) * 2654435761u >> 16) % 100u;
		if (Building.bDowntown)
		{
			return EDTGSurface::Concrete;
		}
		switch (Building.Role)
		{
		case LotRole::Home:
			// Carolina and Florida housing: mostly siding, then brick, then stucco.
			return Pick < 55u ? EDTGSurface::WoodSiding : (Pick < 82u ? EDTGSurface::Brick : EDTGSurface::Stucco);
		case LotRole::Church:
			return EDTGSurface::Brick;
		case LotRole::Jail:
		case LotRole::PoliceStation:
			return EDTGSurface::Concrete;
		default:
			return Pick < 50u ? EDTGSurface::Brick : EDTGSurface::Stucco;
		}
	}

	namespace
	{
		/**
		 * One face of a building in its own coordinates: U runs along the face, Z up from the ground, D out
		 * from the wall. Footprints are axis-aligned, so every box built on a face is axis-aligned too, and a
		 * window layout written once works on all four sides.
		 */
		/** An opening in a face: a window, a door, a shopfront. Facade coordinates. */
		struct FHole
		{
			double U0 = 0.0;
			double U1 = 0.0;
			double Z0 = 0.0;
			double Z1 = 0.0;
		};

		struct FFacade
		{
			FVector Origin = FVector::ZeroVector; // one end of the face, on the wall plane, at ground level
			FVector Along = FVector::ZeroVector;  // unit, along the face: always Cross(Up, Out), so UVs read the same way on every face
			FVector Out = FVector::ZeroVector;    // unit, outward
			double Width = 0.0;                   // Unreal units
			bool bDoor = false;                   // the door is on this face
			double DoorU = 0.0;                   // and this far along it
			TArray<FHole> Holes;                  // what the wall is built around
		};

		FVector OnFacade(const FFacade& Face, double U, double Z, double D)
		{
			return Face.Origin + Face.Along * U + Face.Out * D + FVector(0.0, 0.0, Z);
		}

		/** A box between two facade-space corners. */
		void FacadeBox(FDTGMesh& Mesh, const FFacade& Face, double U0, double U1, double Z0, double Z1, double D0, double D1,
			const FLinearColor& Colour, bool bSkipBottom = true)
		{
			const FVector A = OnFacade(Face, U0, Z0, D0);
			const FVector B = OnFacade(Face, U1, Z1, D1);
			Mesh.AddBox(FVector(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z)),
				FVector(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z)), Colour, bSkipBottom);
		}

		/** A flat panel facing out, D in front of the wall. */
		void FacadePanel(FDTGMesh& Mesh, const FFacade& Face, double U0, double U1, double Z0, double Z1, double D, const FLinearColor& Colour)
		{
			Mesh.AddQuad(OnFacade(Face, U0, Z0, D), OnFacade(Face, U1, Z0, D), OnFacade(Face, U1, Z1, D), OnFacade(Face, U0, Z1, D),
				Face.Out, Colour);
		}

		/**
		 * Window glass, with UVs of one unit per room so the material can put a room behind it. The UVs start
		 * at a cell of this window's own (its place on the facade), so the material's per-room lottery of lit
		 * and dark rooms falls differently for every window: with every window at cell (0,0), every window
		 * in town was lit, or every window dark, at the same moment.
		 */
		void FacadeGlass(FDTGMesh& Mesh, const FFacade& Face, double U0, double U1, double Z0, double Z1, double D, const FLinearColor& Colour,
			double RepeatU, double RepeatV)
		{
			const double CellU = FMath::FloorToDouble(U0 / 0.5) + 3.0 * FMath::FloorToDouble(Face.Width / 0.5);
			const double CellV = FMath::FloorToDouble(Z0 / 0.5);
			Mesh.AddPanel(OnFacade(Face, U0, Z0, D), OnFacade(Face, U1, Z0, D), OnFacade(Face, U1, Z1, D), OnFacade(Face, U0, Z1, D),
				Face.Out, Colour, RepeatU, RepeatV, CellU, CellV);
		}

		/**
		 * The reveal of an opening: the four inner faces of the wall where it has been cut through, from the wall
		 * plane back to the recess. This is what makes a window read as a window instead of a picture of one --
		 * the jambs catch the sun and the head throws a shadow onto the glass.
		 */
		void AddReveal(FDTGMesh& Walls, const FFacade& Face, const FHole& Hole, double Recess, const FLinearColor& Colour, bool bHead = true)
		{
			if (Recess <= 0.0)
			{
				return;
			}
			const double Back = -Recess;
			Walls.AddQuad(OnFacade(Face, Hole.U0, Hole.Z0, 0.0), OnFacade(Face, Hole.U0, Hole.Z0, Back), OnFacade(Face, Hole.U0, Hole.Z1, Back),
				OnFacade(Face, Hole.U0, Hole.Z1, 0.0), Face.Along, Colour);
			Walls.AddQuad(OnFacade(Face, Hole.U1, Hole.Z0, 0.0), OnFacade(Face, Hole.U1, Hole.Z0, Back), OnFacade(Face, Hole.U1, Hole.Z1, Back),
				OnFacade(Face, Hole.U1, Hole.Z1, 0.0), -Face.Along, Colour);
			if (bHead)
			{
				Walls.AddQuad(OnFacade(Face, Hole.U0, Hole.Z1, 0.0), OnFacade(Face, Hole.U1, Hole.Z1, 0.0), OnFacade(Face, Hole.U1, Hole.Z1, Back),
					OnFacade(Face, Hole.U0, Hole.Z1, Back), FVector(0.0, 0.0, -1.0), Colour);
			}
			Walls.AddQuad(OnFacade(Face, Hole.U0, Hole.Z0, 0.0), OnFacade(Face, Hole.U1, Hole.Z0, 0.0), OnFacade(Face, Hole.U1, Hole.Z0, Back),
				OnFacade(Face, Hole.U0, Hole.Z0, Back), FVector(0.0, 0.0, 1.0), Colour);
		}

		/**
		 * The wall of one face, built around its openings: a scanline through every opening edge, and between
		 * each pair of edges a run of wall from the left corner to the right, skipping the openings in it.
		 * Overlapping openings (a door in a shopfront) merge. Vertex alpha carries the height above the ground.
		 */
		void AddWallFace(FDTGMesh& Walls, const FFacade& Face, double Z0, double Z1, double S, const FLinearColor& Colour)
		{
			auto Alpha = [S](double Z) { return static_cast<float>(FMath::Clamp(Z / (2.5 * S), 0.0, 1.0)); };
			TArray<double> Cuts;
			Cuts.Add(Z0);
			Cuts.Add(Z1);
			for (const FHole& Hole : Face.Holes)
			{
				Cuts.Add(FMath::Clamp(Hole.Z0, Z0, Z1));
				Cuts.Add(FMath::Clamp(Hole.Z1, Z0, Z1));
			}
			Cuts.Sort();
			for (int32 Index = 0; Index + 1 < Cuts.Num(); ++Index)
			{
				const double Za = Cuts[Index];
				const double Zb = Cuts[Index + 1];
				if (Zb - Za < 0.01)
				{
					continue;
				}
				TArray<FHole> Spans;
				for (const FHole& Hole : Face.Holes)
				{
					if (Hole.Z0 <= Za + 0.01 && Hole.Z1 >= Zb - 0.01)
					{
						Spans.Add(Hole);
					}
				}
				Spans.Sort([](const FHole& A, const FHole& B) { return A.U0 < B.U0; });
				double U = 0.0;
				auto Run = [&](double Ua, double Ub)
				{
					if (Ub - Ua > 0.01)
					{
						Walls.AddQuadAlpha(OnFacade(Face, Ua, Za, 0.0), OnFacade(Face, Ub, Za, 0.0), OnFacade(Face, Ub, Zb, 0.0),
							OnFacade(Face, Ua, Zb, 0.0), Face.Out, Colour, Alpha(Za), Alpha(Zb));
					}
				};
				for (const FHole& Span : Spans)
				{
					Run(U, FMath::Max(U, FMath::Min(Span.U0, Face.Width)));
					U = FMath::Max(U, FMath::Min(Span.U1, Face.Width));
				}
				Run(U, Face.Width);
			}
		}

		/** What dresses a window besides its frame: the sill (and lintel) in stone or board, and shutters if any. */
		struct FWindowDressing
		{
			FDTGMesh* Sills = nullptr;            // null draws the sill in the frame's own mesh
			FLinearColor SillColour = FLinearColor::White;
			bool bLintel = false;                 // a flat lintel over the head, where masonry needs carrying
			const FLinearColor* Shutters = nullptr; // a pair either side, in this colour, when set
		};

		/**
		 * A framed window set back into the wall: the reveal, then the frame and glass at the back of it, a glazing
		 * bar, a sill that runs out past the wall to throw its own shadow and run the rain off, a lintel over a
		 * masonry opening, and shutters either side on the houses that have them.
		 */
		void AddWindow(FDTGSurfaceMeshes& Meshes, FFacade& Face, double U0, double U1, double Z0, double Z1, double S, double Recess,
			const FLinearColor& GlassColour, const FLinearColor& FrameColour, FDTGMesh& Walls, const FLinearColor& WallColour,
			const FWindowDressing& Dressing)
		{
			const double Bar = 0.07 * S;
			const double Proud = 0.05 * S;
			const double Back = -Recess;
			FDTGMesh& Frames = Meshes[EDTGSurface::Matte];
			const FHole Hole{U0 - Bar, U1 + Bar, Z0 - Bar, Z1 + Bar};
			Face.Holes.Add(Hole);
			AddReveal(Walls, Face, Hole, Recess, WallColour);
			FacadeGlass(Meshes[EDTGSurface::Window], Face, U0, U1, Z0, Z1, Back + 0.01 * S, GlassColour, 1.0, 1.0);
			FacadeBox(Frames, Face, U0 - Bar, U1 + Bar, Z1, Z1 + Bar, Back, Back + Proud, FrameColour);
			FacadeBox(Frames, Face, U0 - Bar, U0, Z0, Z1, Back, Back + Proud, FrameColour);
			FacadeBox(Frames, Face, U1, U1 + Bar, Z0, Z1, Back, Back + Proud, FrameColour);
			const double Mid = 0.5 * (Z0 + Z1);
			FacadeBox(Frames, Face, U0, U1, Mid - 0.02 * S, Mid + 0.02 * S, Back, Back + 0.03 * S, FrameColour);
			FDTGMesh& Sills = Dressing.Sills ? *Dressing.Sills : Frames;
			FacadeBox(Sills, Face, U0 - 1.6 * Bar, U1 + 1.6 * Bar, Z0 - Bar - 0.07 * S, Z0 - Bar, Back, 0.1 * S, Dressing.SillColour, false);
			if (Dressing.bLintel)
			{
				FacadeBox(Sills, Face, U0 - 1.8 * Bar, U1 + 1.8 * Bar, Z1 + Bar, Z1 + Bar + 0.15 * S, 0.0, 0.035 * S, Dressing.SillColour);
			}
			if (Dressing.Shutters)
			{
				const double Leaf = FMath::Min(0.36 * S, 0.45 * (U1 - U0));
				const double Gap = Bar + 0.03 * S;
				FacadeBox(Frames, Face, U0 - Gap - Leaf, U0 - Gap, Z0 - Bar, Z1 + Bar, 0.0, 0.04 * S, *Dressing.Shutters);
				FacadeBox(Frames, Face, U1 + Gap, U1 + Gap + Leaf, Z0 - Bar, Z1 + Bar, 0.0, 0.04 * S, *Dressing.Shutters);
			}
		}

		/** A row of windows along one storey of one face: evenly spaced, clear of the corners and of the door. */
		void AddWindowRow(FDTGSurfaceMeshes& Meshes, FFacade& Face, double Z0, double Z1, double WindowWide, double Pitch,
			double S, double Recess, const FLinearColor& GlassColour, const FLinearColor& FrameColour, bool bAvoidDoor,
			FDTGMesh& Walls, const FLinearColor& WallColour, const FWindowDressing& Dressing)
		{
			const double Margin = 0.9 * S;
			const double Usable = Face.Width - 2.0 * Margin;
			if (Usable < WindowWide || Z1 <= Z0)
			{
				return;
			}
			const int32 Count = FMath::Max(1, FMath::FloorToInt32((Usable + Pitch - WindowWide) / Pitch));
			const double Span = (Count - 1) * Pitch + WindowWide;
			const double Start = 0.5 * (Face.Width - Span);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const double U0 = Start + Index * Pitch;
				const double U1 = U0 + WindowWide;
				if (bAvoidDoor && Face.bDoor && U1 > Face.DoorU - 1.0 * S && U0 < Face.DoorU + 1.0 * S)
				{
					continue;
				}
				AddWindow(Meshes, Face, U0, U1, Z0, Z1, S, Recess, GlassColour, FrameColour, Walls, WallColour, Dressing);
			}
		}

		/**
		 * The door, set back into the wall like the windows: a leaf (or glass, for a shop) in a frame, where the
		 * simulation says the way in is. RevealTop limits the reveal where the door stands in a shopfront that
		 * has its own; Recess 0 draws it on the wall instead (the towers, whose walls are solid).
		 */
		void AddDoor(FDTGSurfaceMeshes& Meshes, FFacade& Face, double FloorZ, double S, double Wide, double Recess, bool bGlass,
			const FLinearColor& LeafColour, const FLinearColor& GlassColour, const FLinearColor& FrameColour, FDTGMesh& Walls,
			const FLinearColor& WallColour, double RevealTop = -1.0)
		{
			const double Bar = 0.09 * S;
			const double Back = -Recess;
			const double U0 = Face.DoorU - 0.5 * Wide;
			const double U1 = Face.DoorU + 0.5 * Wide;
			const double Head = FloorZ + 2.2 * S;
			const FHole Hole{U0 - Bar, U1 + Bar, FloorZ, Head + Bar};
			Face.Holes.Add(Hole);
			FHole Reveal = Hole;
			const bool bOwnHead = RevealTop < 0.0;
			if (!bOwnHead)
			{
				Reveal.Z1 = RevealTop;
			}
			AddReveal(Walls, Face, Reveal, Recess, WallColour, bOwnHead);
			if (bGlass)
			{
				FacadeGlass(Meshes[EDTGSurface::Window], Face, U0, U1, FloorZ, Head, Back + 0.01 * S, GlassColour, 1.0, 1.0);
			}
			else
			{
				FacadeBox(Meshes[EDTGSurface::Matte], Face, U0, U1, FloorZ, Head, Back, Back + 0.04 * S, LeafColour);
			}
			FDTGMesh& Frames = Meshes[EDTGSurface::Matte];
			FacadeBox(Frames, Face, U0 - Bar, U1 + Bar, Head, Head + Bar, Back, Back + 0.08 * S, FrameColour);
			FacadeBox(Frames, Face, U0 - Bar, U0, FloorZ, Head, Back, Back + 0.08 * S, FrameColour);
			FacadeBox(Frames, Face, U1, U1 + Bar, FloorZ, Head, Back, Back + 0.08 * S, FrameColour);
		}

		/** A sloped shed roof standing out from a face: a porch roof or a shop awning. Top and underside. */
		void AddShedRoof(FDTGMesh& Top, FDTGMesh& Under, const FFacade& Face, double U0, double U1, double HighZ, double LowZ,
			double Depth, const FLinearColor& TopColour, const FLinearColor& UnderColour)
		{
			const FVector A = OnFacade(Face, U0, HighZ, 0.0);
			const FVector B = OnFacade(Face, U1, HighZ, 0.0);
			const FVector C = OnFacade(Face, U1, LowZ, Depth);
			const FVector D = OnFacade(Face, U0, LowZ, Depth);
			const FVector Up = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
			const FVector Normal = Up.Z >= 0.0 ? Up : -Up;
			Top.AddQuad(A, B, C, D, Normal, TopColour);
			Under.AddQuad(A, B, C, D, -Normal, UnderColour);
		}

		/** Simple per-building variety from its id, so the same building always looks the same. */
		uint32 VarietyOf(const BuildingPlan& Building, uint32 Salt)
		{
			uint32 H = static_cast<uint32>(Building.Id) * 2654435761u ^ Salt * 0x9E3779B9u;
			H ^= H >> 15;
			H *= 0x85EBCA6Bu;
			H ^= H >> 13;
			return H;
		}

		/**
		 * The colour a wall is painted, by what it is made of. The simulation colours a building by its role,
		 * which would paint every house on a street the same; siding and stucco take a colour of their own
		 * from the palette a Southern street actually shows. Brick and concrete keep their own colour under
		 * the photograph, and the civic buildings, churches and towers keep the role's.
		 */
		FLinearColor WallColourFor(const BuildingPlan& Building, EDTGSurface Kind, const FLinearColor& RoleColour)
		{
			static const FLinearColor Siding[] = {
				FLinearColor(0.80f, 0.80f, 0.76f), FLinearColor(0.78f, 0.74f, 0.62f), FLinearColor(0.55f, 0.58f, 0.60f),
				FLinearColor(0.52f, 0.62f, 0.70f), FLinearColor(0.78f, 0.72f, 0.45f), FLinearColor(0.50f, 0.58f, 0.46f),
				FLinearColor(0.70f, 0.68f, 0.64f), FLinearColor(0.84f, 0.84f, 0.82f)};
			static const FLinearColor Stucco[] = {
				FLinearColor(0.82f, 0.78f, 0.68f), FLinearColor(0.80f, 0.62f, 0.48f), FLinearColor(0.74f, 0.66f, 0.52f),
				FLinearColor(0.85f, 0.84f, 0.80f), FLinearColor(0.80f, 0.74f, 0.50f), FLinearColor(0.72f, 0.56f, 0.46f),
				FLinearColor(0.62f, 0.66f, 0.60f)};
			if (Building.bDowntown || Building.Role == LotRole::Church || Building.Role == LotRole::Jail || Building.Role == LotRole::PoliceStation)
			{
				return RoleColour;
			}
			const uint32 Pick = VarietyOf(Building, 7);
			switch (Kind)
			{
			case EDTGSurface::WoodSiding: return Siding[Pick % 8u];
			case EDTGSurface::Stucco: return Stucco[Pick % 7u];
			default: return RoleColour;
			}
		}
	}

	void BuildBuilding(const BuildingPlan& Building, double GroundZ, double S, FDTGSurfaceMeshes& Meshes)
	{
		const EDTGSurface WallKind = WallSurfaceFor(Building);
		FPalette Palette = PaletteFor(Building);
		Palette.Wall = WallColourFor(Building, WallKind, Palette.Wall);
		FDTGMesh& Walls = Meshes[WallKind];
		FDTGMesh& Trim = Meshes[EDTGSurface::Matte];
		FDTGMesh& Concrete = Meshes[EDTGSurface::Concrete];
		const double MinX = Building.Footprint.MinX * S;
		const double MinY = Building.Footprint.MinY * S;
		const double MaxX = Building.Footprint.MaxX * S;
		const double MaxY = Building.Footprint.MaxY * S;
		const double Base = GroundZ * S;
		const int32 Floors = FMath::Max(1, Building.Floors);
		const double Height = FMath::Max(3.0, Building.HeightM) * S;
		const double Top = Base + Height;

		const bool bHome = Building.Role == LotRole::Home;
		const bool bChurch = Building.Role == LotRole::Church;
		const bool bCivic = Building.Role == LotRole::Jail || Building.Role == LotRole::PoliceStation;
		const bool bTower = Building.bDowntown && Floors >= 5;
		const bool bShop = !bHome && !bChurch && !bCivic && !bTower;

		const FLinearColor White(0.74f, 0.73f, 0.69f);
		const FLinearColor Bronze(0.06f, 0.06f, 0.065f);
		const FLinearColor Stone(0.52f, 0.5f, 0.47f);
		const FLinearColor FrameColour = bHome || bChurch ? White : (bCivic ? Stone : Bronze);

		// The four faces, and which one carries the door.
		FFacade Faces[4];
		Faces[0] = {FVector(MinX, MinY, Base), FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0), MaxX - MinX};
		Faces[1] = {FVector(MaxX, MinY, Base), FVector(0.0, 1.0, 0.0), FVector(1.0, 0.0, 0.0), MaxY - MinY};
		Faces[2] = {FVector(MaxX, MaxY, Base), FVector(-1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), MaxX - MinX};
		Faces[3] = {FVector(MinX, MaxY, Base), FVector(0.0, -1.0, 0.0), FVector(-1.0, 0.0, 0.0), MaxY - MinY};
		const int32 DoorSide = FMath::Clamp(Building.DoorSide, 0, 3);
		FFacade& Front = Faces[DoorSide];
		Front.bDoor = true;
		const double DoorAlong[] = {Building.DoorX * S - MinX, Building.DoorY * S - MinY, MaxX - Building.DoorX * S, MaxY - Building.DoorY * S};
		Front.DoorU = FMath::Clamp(DoorAlong[DoorSide], 1.0 * S, FMath::Max(1.0 * S, Front.Width - 1.0 * S));
		// How far the openings sit back into the wall: a stud wall's worth on a house, a brick's length on the rest.
		const double Recess = (bHome ? 0.12 : 0.22) * S;

		// The base: a house stands on a raised foundation, everything else on a plinth a little proud of the wall.
		const double Footing = bHome ? 0.6 * S : 0.0;
		const double Proud = bHome ? 0.03 * S : 0.04 * S;
		Concrete.AddBox(FVector(MinX - Proud, MinY - Proud, Base - 0.3 * S), FVector(MaxX + Proud, MaxY + Proud, Base + (bHome ? Footing : 0.45 * S)),
			Stone);
		if (bTower)
		{
			// Solid: the curtain wall hangs on the outside of it.
			Walls.AddBox(FVector(MinX, MinY, Base + Footing), FVector(MaxX, MaxY, Top), Palette.Wall);
		}

		const double StoreyHeight = (Height - Footing) / Floors;
		if (bTower)
		{
			// A glass curtain wall above a double-height lobby: floor bands and metal mullions give the scale.
			const double Lobby = 4.5 * S;
			FDTGMesh& Metal = Meshes[EDTGSurface::Chrome];
			const FLinearColor Mullion(0.32f, 0.33f, 0.35f);
			for (const FFacade& Face : Faces)
			{
				FacadeGlass(Meshes[EDTGSurface::Window], Face, 0.0, Face.Width, Lobby, Height, 0.03 * S, Palette.Window,
					FMath::Max(1.0, static_cast<double>(FMath::RoundToInt(Face.Width / (4.0 * S)))), FMath::Max(1.0, static_cast<double>(Floors - 1)));
				for (int32 Floor = 1; Floor < Floors; ++Floor)
				{
					const double Line = Lobby + (Floor - 1) * (Height - Lobby) / FMath::Max(1, Floors - 1);
					FacadeBox(Concrete, Face, 0.0, Face.Width, Line - 0.45 * S, Line + 0.3 * S, 0.0, 0.08 * S, Stone);
				}
				for (double U = 0.75 * S; U < Face.Width - 0.5 * S; U += 1.5 * S)
				{
					FacadeBox(Metal, Face, U - 0.04 * S, U + 0.04 * S, Lobby, Height, 0.0, 0.12 * S, Mullion);
				}
				// The lobby: shopfront glass between piers.
				FacadeGlass(Meshes[EDTGSurface::Window], Face, 0.6 * S, Face.Width - 0.6 * S, 0.45 * S, Lobby - 0.6 * S, 0.02 * S, Palette.Window,
					FMath::Max(1.0, static_cast<double>(FMath::RoundToInt((Face.Width - 1.2 * S) / (3.0 * S)))), 1.0);
				for (double U = 0.6 * S; U <= Face.Width - 0.6 * S; U += 3.0 * S)
				{
					FacadeBox(Concrete, Face, U - 0.25 * S, U + 0.25 * S, 0.0, Lobby, 0.0, 0.15 * S, Stone);
				}
			}
			// A canopy over the way in.
			FacadeBox(Concrete, Front, Front.DoorU - 3.0 * S, Front.DoorU + 3.0 * S, Lobby - 0.5 * S, Lobby - 0.25 * S, 0.0, 2.5 * S, Stone, false);
			AddDoor(Meshes, Front, 0.0, S, 2.0 * S, 0.0, true, Palette.Trim, Palette.Window, Bronze, Walls, Palette.Wall);
		}
		else if (bShop)
		{
			// A shopfront on the street side: glass between thin mullions, an awning, and a sign band.
			const double ShopTop = FMath::Min(3.0 * S, StoreyHeight - 0.4 * S);
			const double ShopSill = 0.45 * S;
			const FHole ShopHole{0.5 * S, Front.Width - 0.5 * S, ShopSill, ShopTop};
			Front.Holes.Add(ShopHole);
			AddReveal(Walls, Front, ShopHole, Recess, Palette.Wall);
			FacadeGlass(Meshes[EDTGSurface::Window], Front, ShopHole.U0, ShopHole.U1, ShopSill, ShopTop, -Recess + 0.01 * S, Palette.Window,
				FMath::Max(1.0, static_cast<double>(FMath::RoundToInt((Front.Width - 1.0 * S) / (3.0 * S)))), 1.0);
			for (double U = 0.5 * S; U <= Front.Width - 0.5 * S + 1.0; U += 2.2 * S)
			{
				FacadeBox(Trim, Front, U - 0.05 * S, U + 0.05 * S, ShopSill, ShopTop, -Recess, -Recess + 0.08 * S, Bronze);
			}
			FacadeBox(Trim, Front, 0.5 * S, Front.Width - 0.5 * S, ShopTop - 0.6 * S, ShopTop - 0.54 * S, -Recess, -Recess + 0.06 * S, Bronze);
			static const FLinearColor Canvas[] = {
				FLinearColor(0.05f, 0.18f, 0.09f), FLinearColor(0.32f, 0.04f, 0.05f), FLinearColor(0.04f, 0.07f, 0.2f),
				FLinearColor(0.36f, 0.3f, 0.2f), FLinearColor(0.12f, 0.12f, 0.12f)};
			const FLinearColor Awning = Canvas[VarietyOf(Building, 1) % 5u];
			AddShedRoof(Meshes[EDTGSurface::Cloth], Meshes[EDTGSurface::Cloth], Front, 0.3 * S, Front.Width - 0.3 * S,
				ShopTop + 0.45 * S, ShopTop - 0.05 * S, 1.4 * S, Awning, Awning);
			FacadeBox(Meshes[EDTGSurface::Cloth], Front, 0.3 * S, Front.Width - 0.3 * S, ShopTop - 0.3 * S, ShopTop - 0.05 * S,
				1.35 * S, 1.4 * S, Awning);
			FacadeBox(Trim, Front, 0.3 * S, Front.Width - 0.3 * S, ShopTop + 0.55 * S, ShopTop + 1.2 * S, 0.0, 0.1 * S, Bronze);
			AddDoor(Meshes, Front, 0.0, S, 1.1 * S, Recess, true, Palette.Trim, Palette.Window, Bronze, Walls, Palette.Wall, ShopSill);
			// Ordinary windows round the other sides, and on every storey above, with stone sills and lintels.
			const FWindowDressing Masonry{&Concrete, Stone, true, nullptr};
			for (FFacade& Face : Faces)
			{
				for (int32 Floor = Face.bDoor ? 1 : 0; Floor < Floors; ++Floor)
				{
					const double Sill = Floor * StoreyHeight + 0.9 * S;
					AddWindowRow(Meshes, Face, Sill, Sill + FMath::Min(1.6 * S, StoreyHeight - 1.3 * S), 1.4 * S, 2.6 * S, S, Recess,
						Palette.Window, FrameColour, Floor == 0, Walls, Palette.Wall, Masonry);
				}
			}
		}
		else
		{
			// Houses, churches and the civic buildings: windows storey by storey, sized to what they are. A house
			// gets board sills, and about half of them shutters in a colour that is not the door's.
			static const FLinearColor ShutterPaint[] = {
				FLinearColor(0.04f, 0.12f, 0.06f), FLinearColor(0.02f, 0.02f, 0.02f), FLinearColor(0.03f, 0.05f, 0.14f),
				FLinearColor(0.28f, 0.04f, 0.04f), FLinearColor(0.12f, 0.12f, 0.13f)};
			const FLinearColor Shutter = ShutterPaint[VarietyOf(Building, 5) % 5u];
			const bool bShutters = bHome && VarietyOf(Building, 8) % 2u == 0u;
			const FWindowDressing Dressing{bHome ? &Trim : &Concrete, bHome ? White : Stone, !bHome, bShutters ? &Shutter : nullptr};
			for (FFacade& Face : Faces)
			{
				if (bChurch)
				{
					// Tall narrow windows down the nave, none on the gable with the door.
					if (!Face.bDoor)
					{
						AddWindowRow(Meshes, Face, 1.2 * S, Height - 1.8 * S, 1.0 * S, 3.4 * S, S, Recess, Palette.Window, FrameColour, false,
							Walls, Palette.Wall, Dressing);
					}
					continue;
				}
				for (int32 Floor = 0; Floor < Floors; ++Floor)
				{
					const double Sill = Footing + Floor * StoreyHeight + (bCivic ? 1.4 : 0.95) * S;
					const double Tall = bCivic ? 0.9 * S : FMath::Min(1.4 * S, StoreyHeight - 1.4 * S);
					AddWindowRow(Meshes, Face, Sill, Sill + Tall, bCivic ? 0.6 * S : 1.0 * S, bCivic ? 2.6 * S : 2.8 * S, S, Recess,
						Palette.Window, FrameColour, Floor == 0, Walls, Palette.Wall, Dressing);
				}
			}
			// A painted front door on a house: the one splash of colour a street of them has.
			static const FLinearColor DoorPaint[] = {
				FLinearColor(0.3f, 0.03f, 0.03f), FLinearColor(0.03f, 0.05f, 0.14f), FLinearColor(0.02f, 0.02f, 0.02f),
				FLinearColor(0.04f, 0.14f, 0.07f), FLinearColor(0.22f, 0.13f, 0.06f), FLinearColor(0.6f, 0.58f, 0.52f)};
			const FLinearColor Leaf = bHome ? DoorPaint[VarietyOf(Building, 2) % 6u] : Palette.Trim;
			AddDoor(Meshes, Front, Footing, S, bChurch ? 1.8 * S : 1.0 * S, Recess, false, Leaf, Palette.Window, FrameColour, Walls, Palette.Wall);
		}

		// The walls themselves, built around every opening, and the top of them under the roof.
		if (!bTower)
		{
			for (const FFacade& Face : Faces)
			{
				AddWallFace(Walls, Face, Footing, Height, S, Palette.Wall);
			}
			Walls.AddQuad(FVector(MinX, MinY, Top), FVector(MaxX, MinY, Top), FVector(MaxX, MaxY, Top), FVector(MinX, MaxY, Top),
				FVector::UpVector, Palette.Wall);
		}

		// Houses: a front porch on most, steps up to the door, and a chimney on some.
		if (bHome)
		{
			const double Depth = 2.2 * S;
			const bool bPorch = VarietyOf(Building, 3) % 3u != 0u;
			const double HalfWide = bPorch ? FMath::Min(0.3 * Front.Width, 3.2 * S) : 0.9 * S;
			const double U0 = FMath::Max(0.2 * S, Front.DoorU - HalfWide);
			const double U1 = FMath::Min(Front.Width - 0.2 * S, Front.DoorU + HalfWide);
			const double Deck = bPorch ? Depth : 1.2 * S;
			FacadeBox(Concrete, Front, U0, U1, -0.3 * S, Footing, 0.0, Deck, Stone);
			FacadeBox(Concrete, Front, Front.DoorU - 0.7 * S, Front.DoorU + 0.7 * S, -0.3 * S, Footing * 0.5, Deck, Deck + 0.35 * S, Stone);
			if (bPorch)
			{
				const double PostTop = Footing + 2.6 * S;
				for (const double U : {U0 + 0.1 * S, U1 - 0.3 * S})
				{
					FacadeBox(Trim, Front, U, U + 0.2 * S, Footing, PostTop, Depth - 0.3 * S, Depth - 0.1 * S, White);
				}
				AddShedRoof(Meshes[EDTGSurface::ShingleRoof], Trim, Front, U0 - 0.2 * S, U1 + 0.2 * S, PostTop + 0.45 * S, PostTop,
					Depth + 0.3 * S, Palette.Roof, White);
			}
			if (VarietyOf(Building, 4) % 4u == 0u)
			{
				const double Ridge = FMath::Min(FMath::Min(MaxX - MinX, MaxY - MinY) * 0.35, 4.0 * S);
				const double Cy = 0.5 * (MinY + MaxY);
				Meshes[EDTGSurface::Brick].AddBox(FVector(MaxX - 1.5 * S, Cy - 0.45 * S, Base), FVector(MaxX - 0.6 * S, Cy + 0.45 * S, Top + Ridge + 0.6 * S),
					FLinearColor(0.42f, 0.17f, 0.12f));
			}
		}

		// Roof: pitched on a house, a church or a one-storey shed, with eaves; flat behind a cornice and a
		// parapet on everything else, with plant rooms on the towers.
		const bool bPitched = bHome || bChurch || (Floors <= 1 && !bShop);
		if (bPitched)
		{
			const bool bShingled = bHome || bChurch;
			FDTGMesh& Roof = Meshes[bShingled ? EDTGSurface::ShingleRoof : EDTGSurface::MetalRoof];
			const double Ridge = FMath::Min(FMath::Min(MaxX - MinX, MaxY - MinY) * 0.35, 4.0 * S);
			const double Eave = 0.45 * S;
			// Dropped so the roof meets the wall top exactly at the wall line instead of floating above it,
			// and gable ends built in the wall, where a real house has them, rather than in shingle.
			const bool bAlongX = MaxX - MinX >= MaxY - MinY;
			const double HalfSpan = 0.5 * (bAlongX ? MaxY - MinY : MaxX - MinX);
			const double Drop = Ridge * Eave / (HalfSpan + Eave);
			const double Apex = Top - Drop + Ridge;
			Roof.AddRoof(FVector(MinX - Eave, MinY - Eave, Top - Drop), FVector(MaxX + Eave, MaxY + Eave, Top - Drop), Ridge, Palette.Roof,
				/*bGableEnds*/ false);
			if (bAlongX)
			{
				const double MidY = 0.5 * (MinY + MaxY);
				Walls.AddTri(FVector(MinX, MinY, Top), FVector(MinX, MidY, Apex), FVector(MinX, MaxY, Top), FVector(-1.0, 0.0, 0.0), Palette.Wall);
				Walls.AddTri(FVector(MaxX, MaxY, Top), FVector(MaxX, MidY, Apex), FVector(MaxX, MinY, Top), FVector(1.0, 0.0, 0.0), Palette.Wall);
			}
			else
			{
				const double MidX = 0.5 * (MinX + MaxX);
				Walls.AddTri(FVector(MinX, MinY, Top), FVector(MaxX, MinY, Top), FVector(MidX, MinY, Apex), FVector(0.0, -1.0, 0.0), Palette.Wall);
				Walls.AddTri(FVector(MaxX, MaxY, Top), FVector(MinX, MaxY, Top), FVector(MidX, MaxY, Apex), FVector(0.0, 1.0, 0.0), Palette.Wall);
			}
			// A fascia board along the eaves, so the roof has an edge instead of a knife. AddRoof runs the
			// ridge along the long axis, so the eaves are on the long sides.
			const double Board = 0.04 * S;
			if (bAlongX)
			{
				Trim.AddBox(FVector(MinX - Eave, MinY - Eave, Top - Drop - 0.2 * S), FVector(MaxX + Eave, MinY - Eave + Board, Top - Drop), White);
				Trim.AddBox(FVector(MinX - Eave, MaxY + Eave - Board, Top - Drop - 0.2 * S), FVector(MaxX + Eave, MaxY + Eave, Top - Drop), White);
			}
			else
			{
				Trim.AddBox(FVector(MinX - Eave, MinY - Eave, Top - Drop - 0.2 * S), FVector(MinX - Eave + Board, MaxY + Eave, Top - Drop), White);
				Trim.AddBox(FVector(MaxX + Eave - Board, MinY - Eave, Top - Drop - 0.2 * S), FVector(MaxX + Eave, MaxY + Eave, Top - Drop), White);
			}
		}
		else
		{
			// Built-up roofing behind the parapet: dark, like the tar and gravel it is, not a slab of the wall's colour.
			Meshes[EDTGSurface::Asphalt].AddBox(FVector(MinX, MinY, Top), FVector(MaxX, MaxY, Top + 0.1 * S), FLinearColor(0.2f, 0.19f, 0.18f));
			// The parapet is the wall carried up; the cornice under it is the line a facade finishes on.
			const double Wall = 0.25 * S;
			Walls.AddBox(FVector(MinX, MinY, Top), FVector(MaxX, MinY + Wall, Top + 0.9 * S), Palette.Wall);
			Walls.AddBox(FVector(MinX, MaxY - Wall, Top), FVector(MaxX, MaxY, Top + 0.9 * S), Palette.Wall);
			Walls.AddBox(FVector(MinX, MinY, Top), FVector(MinX + Wall, MaxY, Top + 0.9 * S), Palette.Wall);
			Walls.AddBox(FVector(MaxX - Wall, MinY, Top), FVector(MaxX, MaxY, Top + 0.9 * S), Palette.Wall);
			for (const FFacade& Face : Faces)
			{
				FacadeBox(Concrete, Face, -0.2 * S, Face.Width + 0.2 * S, Height - 0.35 * S, Height, 0.0, 0.2 * S, Stone, false);
				FacadeBox(Concrete, Face, -0.1 * S, Face.Width + 0.1 * S, Height - 0.5 * S, Height - 0.35 * S, 0.0, 0.1 * S, Stone, false);
			}
			// A cap on the parapet, a little proud of it, so the roofline is a line and not an edge.
			const double Cap = Top + 0.9 * S;
			const double Lip = 0.05 * S;
			Concrete.AddBox(FVector(MinX - Lip, MinY - Lip, Cap), FVector(MaxX + Lip, MinY + Wall + Lip, Cap + 0.08 * S), Stone);
			Concrete.AddBox(FVector(MinX - Lip, MaxY - Wall - Lip, Cap), FVector(MaxX + Lip, MaxY + Lip, Cap + 0.08 * S), Stone);
			Concrete.AddBox(FVector(MinX - Lip, MinY, Cap), FVector(MinX + Wall + Lip, MaxY, Cap + 0.08 * S), Stone);
			Concrete.AddBox(FVector(MaxX - Wall - Lip, MinY, Cap), FVector(MaxX + Lip, MaxY, Cap + 0.08 * S), Stone);
			if (bTower)
			{
				const double Cx = 0.5 * (MinX + MaxX);
				const double Cy = 0.5 * (MinY + MaxY);
				const double Hx = 0.2 * (MaxX - MinX);
				const double Hy = 0.2 * (MaxY - MinY);
				Concrete.AddBox(FVector(Cx - Hx, Cy - Hy, Top), FVector(Cx + Hx, Cy + Hy, Top + 3.5 * S), Stone);
			}
		}

		// What a plain box lacks: a line at every floor on the street buildings, a downpipe at two corners of
		// anything with a flat roof, and the plant that lives on top of it. Each is a few boxes; together they
		// are most of what tells a building from a drawing of one.
		if (bShop && Floors > 1)
		{
			for (const FFacade& Face : Faces)
			{
				for (int32 Floor = 1; Floor < Floors; ++Floor)
				{
					FacadeBox(Concrete, Face, 0.0, Face.Width, Floor * StoreyHeight - 0.07 * S, Floor * StoreyHeight + 0.07 * S, 0.0, 0.05 * S, Stone, false);
				}
			}
		}
		if (!bPitched)
		{
			const FLinearColor Pipe(0.3f, 0.3f, 0.31f);
			for (int32 Side = 0; Side < 4; Side += 2)
			{
				FacadeBox(Trim, Faces[Side], 0.3 * S, 0.4 * S, Footing, Height + 0.7 * S, 0.02 * S, 0.12 * S, Pipe);
			}
			const int32 Units = 1 + static_cast<int32>(VarietyOf(Building, 6) % 3u);
			for (int32 Unit = 0; Unit < Units; ++Unit)
			{
				const uint32 Where = VarietyOf(Building, 10u + static_cast<uint32>(Unit));
				const double Ux = MinX + 1.0 * S + (Where % 1000u) / 1000.0 * FMath::Max(0.0, MaxX - MinX - 3.2 * S);
				const double Uy = MinY + 1.0 * S + ((Where >> 10) % 1000u) / 1000.0 * FMath::Max(0.0, MaxY - MinY - 2.8 * S);
				Trim.AddBox(FVector(Ux, Uy, Top + 0.1 * S), FVector(Ux + 1.8 * S, Uy + 1.1 * S, Top + 1.2 * S), FLinearColor(0.56f, 0.57f, 0.58f));
				Trim.AddBox(FVector(Ux + 0.3 * S, Uy + 0.2 * S, Top + 1.2 * S), FVector(Ux + 1.5 * S, Uy + 0.9 * S, Top + 1.35 * S), FLinearColor(0.2f, 0.2f, 0.21f));
			}
		}

		// A steeple on a church, because a town skyline needs something that isn't a box.
		if (bChurch)
		{
			const double Cx = 0.5 * (MinX + MaxX);
			const double Cy = 0.5 * (MinY + MaxY);
			const double Wide = 1.1 * S;
			Walls.AddBox(FVector(Cx - Wide, Cy - Wide, Top), FVector(Cx + Wide, Cy + Wide, Top + 5.0 * S), Palette.Wall);
			Meshes[EDTGSurface::ShingleRoof].AddTaperedBox(FVector(Cx - Wide, Cy - Wide, Top + 5.0 * S), FVector(Cx + Wide, Cy + Wide, Top + 9.0 * S),
				0.48, 0.48, 0.48, Palette.Roof);
		}
	}

	// ======================================================================================== Cars

	void BuildCarBody(BodyStyle Body, double LengthM, double WidthM, double HeightM, const FLinearColor& Paint, FDTGSurfaceMeshes& Meshes)
	{
		constexpr double S = 100.0; // metres to Unreal units
		FDTGMesh& Out = Meshes[EDTGSurface::Paint];
		const double HalfL = 0.5 * LengthM * S;
		// The body is narrower than the car's overall width: the tyres sit outboard of it, where they can
		// be seen, exactly as the wheel positions the chassis works with put them.
		const double HalfW = 0.5 * WidthM * S * 0.86;
		const double Height = HeightM * S;
		// The body sits around the centre of mass, which is roughly a third of the way up.
		const double Floor = -Height * 0.32;

		// Each body style is the same two shapes in different proportions: how much of the length the cabin
		// takes, where it sits, and how low the whole thing is.
		double CabinStart = -0.25;  // fraction of length, from the middle, where the cabin begins
		double CabinEnd = 0.25;
		double LowerShare = 0.45;   // how much of the height is body rather than cabin
		double Taper = 0.12;
		bool bOpenTop = false;
		bool bBed = false;
		switch (Body)
		{
		case BodyStyle::Sedan: CabinStart = -0.30; CabinEnd = 0.18; LowerShare = 0.50; break;
		case BodyStyle::Coupe: CabinStart = -0.26; CabinEnd = 0.10; LowerShare = 0.52; Taper = 0.18; break;
		case BodyStyle::Hatchback: CabinStart = -0.38; CabinEnd = 0.14; LowerShare = 0.46; break;
		case BodyStyle::Wagon: CabinStart = -0.44; CabinEnd = 0.16; LowerShare = 0.46; break;
		case BodyStyle::Convertible: CabinStart = -0.24; CabinEnd = 0.06; LowerShare = 0.70; bOpenTop = true; Taper = 0.16; break;
		case BodyStyle::SUV: CabinStart = -0.42; CabinEnd = 0.22; LowerShare = 0.44; Taper = 0.08; break;
		case BodyStyle::Pickup: CabinStart = -0.10; CabinEnd = 0.26; LowerShare = 0.48; bBed = true; break;
		case BodyStyle::Van: CabinStart = -0.46; CabinEnd = 0.34; LowerShare = 0.34; Taper = 0.06; break;
		case BodyStyle::Muscle: CabinStart = -0.24; CabinEnd = 0.08; LowerShare = 0.56; Taper = 0.16; break;
		case BodyStyle::Sports: CabinStart = -0.20; CabinEnd = 0.04; LowerShare = 0.58; Taper = 0.22; break;
		case BodyStyle::Supercar: CabinStart = -0.16; CabinEnd = 0.00; LowerShare = 0.62; Taper = 0.26; break;
		case BodyStyle::Luxury: CabinStart = -0.32; CabinEnd = 0.20; LowerShare = 0.50; break;
		case BodyStyle::Truck: CabinStart = 0.10; CabinEnd = 0.40; LowerShare = 0.40; bBed = true; Taper = 0.05; break;
		case BodyStyle::Bus: CabinStart = -0.48; CabinEnd = 0.46; LowerShare = 0.22; Taper = 0.03; break;
		case BodyStyle::MonsterTruck: CabinStart = -0.10; CabinEnd = 0.26; LowerShare = 0.38; bBed = true; break;
		case BodyStyle::OpenWheel:
		case BodyStyle::Dragster: CabinStart = -0.10; CabinEnd = 0.05; LowerShare = 0.75; bOpenTop = true; Taper = 0.3; break;
		default: break;
		}

		const double LowerTop = Floor + Height * LowerShare;
		// The lower body, tapering in a little toward the nose.
		Out.AddTaperedBox(FVector(-HalfL, -HalfW, Floor), FVector(HalfL, HalfW, LowerTop), Taper * 0.5, Taper * 0.2, 0.04, Paint, false);

		if (bBed)
		{
			// A pickup's bed: sides and a tailgate, open in the middle.
			const double BedBack = -HalfL;
			const double BedFront = CabinStart * 2.0 * HalfL;
			const double Wall = 0.1 * S;
			const double BedTop = LowerTop + Height * 0.22;
			Out.AddBox(FVector(BedBack, -HalfW, LowerTop), FVector(BedFront, -HalfW + Wall, BedTop), Paint);
			Out.AddBox(FVector(BedBack, HalfW - Wall, LowerTop), FVector(BedFront, HalfW, BedTop), Paint);
			Out.AddBox(FVector(BedBack, -HalfW, LowerTop), FVector(BedBack + Wall, HalfW, BedTop), Paint);
		}

		if (!bOpenTop)
		{
			// The cabin, inset from the body and tapering toward the roof: the greenhouse.
			const double CabinMinX = CabinStart * 2.0 * HalfL;
			const double CabinMaxX = CabinEnd * 2.0 * HalfL;
			const double CabinInset = HalfW * 0.08;
			const FLinearColor Windows(0.1f, 0.13f, 0.16f);
			// Glass first, as a slightly smaller box, then the roof cap in body colour on top.
			Meshes[EDTGSurface::Glass].AddTaperedBox(FVector(CabinMinX, -HalfW + CabinInset, LowerTop), FVector(CabinMaxX, HalfW - CabinInset, Floor + Height * 0.92),
				0.18, 0.1, 0.1, Windows);
			Out.AddBox(FVector(CabinMinX + (CabinMaxX - CabinMinX) * 0.1, -HalfW + CabinInset * 2.0, Floor + Height * 0.92),
				FVector(CabinMaxX - (CabinMaxX - CabinMinX) * 0.18, HalfW - CabinInset * 2.0, Floor + Height), Paint);
		}

		// Lights, so the front is obviously the front.
		const double LightH = Height * 0.12;
		const double LightZ = Floor + Height * (LowerShare * 0.55);
		const FLinearColor Headlight(0.92f, 0.9f, 0.78f);
		const FLinearColor Taillight(0.5f, 0.04f, 0.04f);
		const double LightW = HalfW * 0.3;
		Meshes[EDTGSurface::Chrome].AddBox(FVector(HalfL - 0.06 * S, -HalfW * 0.82, LightZ), FVector(HalfL + 0.02 * S, -HalfW * 0.82 + LightW, LightZ + LightH), Headlight);
		Meshes[EDTGSurface::Chrome].AddBox(FVector(HalfL - 0.06 * S, HalfW * 0.82 - LightW, LightZ), FVector(HalfL + 0.02 * S, HalfW * 0.82, LightZ + LightH), Headlight);
		Meshes[EDTGSurface::Glass].AddBox(FVector(-HalfL - 0.02 * S, -HalfW * 0.82, LightZ), FVector(-HalfL + 0.06 * S, -HalfW * 0.82 + LightW, LightZ + LightH), Taillight);
		Meshes[EDTGSurface::Glass].AddBox(FVector(-HalfL - 0.02 * S, HalfW * 0.82 - LightW, LightZ), FVector(-HalfL + 0.06 * S, HalfW * 0.82, LightZ + LightH), Taillight);
	}

	void BuildWheel(double RadiusM, double WidthM, const FLinearColor& Rim, FDTGSurfaceMeshes& Meshes)
	{
		constexpr double S = 100.0;
		Meshes[EDTGSurface::Rubber].AddWheel(FVector::ZeroVector, RadiusM * S, FMath::Max(0.12, WidthM) * S, FLinearColor(0.025f, 0.025f, 0.028f), Rim,
			14, &Meshes[EDTGSurface::Chrome]);
	}

	// ======================================================================================== People

	void BuildPerson(const FLinearColor& Clothes, const FLinearColor& Skin, FDTGMesh& Out)
	{
		constexpr double S = 100.0;
		// A capsule's worth of person, built around the actor origin (the capsule's middle).
		const double Half = 0.9 * S;
		Out.AddBox(FVector(-0.14 * S, -0.16 * S, -Half), FVector(0.14 * S, -0.04 * S, -Half + 0.85 * S), FLinearColor(0.1f, 0.1f, 0.12f));
		Out.AddBox(FVector(-0.14 * S, 0.04 * S, -Half), FVector(0.14 * S, 0.16 * S, -Half + 0.85 * S), FLinearColor(0.1f, 0.1f, 0.12f));
		Out.AddBox(FVector(-0.16 * S, -0.22 * S, -Half + 0.85 * S), FVector(0.16 * S, 0.22 * S, -Half + 1.42 * S), Clothes);
		Out.AddBox(FVector(-0.1 * S, -0.3 * S, -Half + 0.9 * S), FVector(0.1 * S, -0.22 * S, -Half + 1.38 * S), Clothes);
		Out.AddBox(FVector(-0.1 * S, 0.22 * S, -Half + 0.9 * S), FVector(0.1 * S, 0.3 * S, -Half + 1.38 * S), Clothes);
		Out.AddBox(FVector(-0.11 * S, -0.11 * S, -Half + 1.42 * S), FVector(0.11 * S, 0.11 * S, -Half + 1.66 * S), Skin);
	}

	// ======================================================================================== Interiors

	void BuildInterior(const BuildingPlan& Building, double S, FDTGMesh& Out)
	{
		const FPalette Palette = PaletteFor(Building);
		const double Wide = FMath::Max(4.0, Building.Footprint.Width() - 0.6) * S;
		const double Deep = FMath::Max(4.0, Building.Footprint.Depth() - 0.6) * S;
		const double High = 3.2 * S;
		const double HalfW = 0.5 * Wide;
		const double HalfD = 0.5 * Deep;
		const double Wall = 0.15 * S;

		const FLinearColor FloorColour = Building.Role == LotRole::Home || Building.Role == LotRole::Apartment
			? FLinearColor(0.21f, 0.15f, 0.1f)
			: FLinearColor(0.3f, 0.3f, 0.31f);
		const FLinearColor WallColour = Palette.Wall * 1.25f;
		const FLinearColor Ceiling(0.5f, 0.5f, 0.52f);

		Out.AddBox(FVector(-HalfW, -HalfD, -Wall), FVector(HalfW, HalfD, 0.0), FloorColour, false);
		Out.AddBox(FVector(-HalfW, -HalfD, High), FVector(HalfW, HalfD, High + Wall), Ceiling, false);
		// Three solid walls and a fourth with a doorway in it, on the side the player comes in.
		Out.AddBox(FVector(-HalfW, HalfD - Wall, 0.0), FVector(HalfW, HalfD, High), WallColour, false);
		Out.AddBox(FVector(-HalfW, -HalfD, 0.0), FVector(-HalfW + Wall, HalfD, High), WallColour, false);
		Out.AddBox(FVector(HalfW - Wall, -HalfD, 0.0), FVector(HalfW, HalfD, High), WallColour, false);
		const double DoorHalf = 1.1 * S;
		Out.AddBox(FVector(-HalfW, -HalfD, 0.0), FVector(-DoorHalf, -HalfD + Wall, High), WallColour, false);
		Out.AddBox(FVector(DoorHalf, -HalfD, 0.0), FVector(HalfW, -HalfD + Wall, High), WallColour, false);
		Out.AddBox(FVector(-DoorHalf, -HalfD, 2.3 * S), FVector(DoorHalf, -HalfD + Wall, High), WallColour, false);

		// What's in the room depends on what the building is for.
		const FLinearColor Wood(0.26f, 0.17f, 0.1f);
		const FLinearColor Metal(0.4f, 0.41f, 0.43f);
		const FLinearColor Fabric(0.2f, 0.26f, 0.3f);
		switch (Building.Role)
		{
		case LotRole::Shop:
		case LotRole::ElectronicsStore:
		case LotRole::FurnitureStore:
		case LotRole::GardenCenter:
			// Aisles and a counter.
			for (int32 Aisle = 0; Aisle < 3; ++Aisle)
			{
				const double X = -HalfW * 0.5 + Aisle * HalfW * 0.5;
				Out.AddBox(FVector(X - 0.4 * S, -HalfD * 0.5, 0.0), FVector(X + 0.4 * S, HalfD * 0.6, 1.8 * S), Metal);
			}
			Out.AddBox(FVector(-HalfW * 0.8, HalfD * 0.7, 0.0), FVector(HalfW * 0.2, HalfD * 0.85, 1.1 * S), Wood);
			break;
		case LotRole::Diner:
			for (int32 Table = 0; Table < 4; ++Table)
			{
				const double X = -HalfW * 0.6 + Table * HalfW * 0.4;
				Out.AddBox(FVector(X - 0.6 * S, -0.6 * S, 0.7 * S), FVector(X + 0.6 * S, 0.6 * S, 0.78 * S), Wood);
				Out.AddBox(FVector(X - 0.6 * S, -1.2 * S, 0.0), FVector(X + 0.6 * S, -0.7 * S, 0.45 * S), Fabric);
				Out.AddBox(FVector(X - 0.6 * S, 0.7 * S, 0.0), FVector(X + 0.6 * S, 1.2 * S, 0.45 * S), Fabric);
			}
			break;
		case LotRole::Church:
			for (int32 Pew = 0; Pew < 6; ++Pew)
			{
				const double Y = -HalfD * 0.7 + Pew * HalfD * 0.22;
				Out.AddBox(FVector(-HalfW * 0.7, Y, 0.0), FVector(HalfW * 0.7, Y + 0.3 * S, 0.9 * S), Wood);
			}
			Out.AddBox(FVector(-1.0 * S, HalfD * 0.75, 0.0), FVector(1.0 * S, HalfD * 0.85, 1.2 * S), Wood);
			break;
		case LotRole::Dealership:
		case LotRole::ClassicDealer:
		case LotRole::UsedCarLot:
			// A showroom floor: the cars themselves are spawned, so this is the dais and the desk.
			Out.AddBox(FVector(-HalfW * 0.6, -HalfD * 0.4, 0.0), FVector(HalfW * 0.6, HalfD * 0.4, 0.12 * S), FLinearColor(0.55f, 0.55f, 0.57f));
			Out.AddBox(FVector(HalfW * 0.5, HalfD * 0.6, 0.0), FVector(HalfW * 0.9, HalfD * 0.85, 1.1 * S), Wood);
			break;
		case LotRole::Jail:
			// Bars, a bunk and a door that is not for you.
			for (int32 Bar = 0; Bar < 9; ++Bar)
			{
				const double X = -HalfW * 0.6 + Bar * HalfW * 0.15;
				Out.AddBox(FVector(X, -0.05 * S, 0.0), FVector(X + 0.08 * S, 0.05 * S, 2.6 * S), Metal);
			}
			Out.AddBox(FVector(-HalfW * 0.8, HalfD * 0.3, 0.45 * S), FVector(-HalfW * 0.2, HalfD * 0.8, 0.6 * S), Metal);
			break;
		case LotRole::Bank:
		case LotRole::Office:
		case LotRole::CareerCenter:
		case LotRole::RealEstate:
			Out.AddBox(FVector(-HalfW * 0.8, HalfD * 0.5, 0.0), FVector(HalfW * 0.8, HalfD * 0.7, 1.1 * S), Wood);
			for (int32 Desk = 0; Desk < 2; ++Desk)
			{
				const double X = -HalfW * 0.4 + Desk * HalfW * 0.8;
				Out.AddBox(FVector(X - 0.8 * S, -HalfD * 0.4, 0.68 * S), FVector(X + 0.8 * S, -HalfD * 0.1, 0.76 * S), Wood);
			}
			break;
		case LotRole::Gym:
			for (int32 Rack = 0; Rack < 3; ++Rack)
			{
				const double X = -HalfW * 0.6 + Rack * HalfW * 0.6;
				Out.AddBox(FVector(X - 0.5 * S, -0.3 * S, 0.0), FVector(X + 0.5 * S, 0.3 * S, 1.4 * S), Metal);
			}
			break;
		case LotRole::Library:
		case LotRole::Museum:
		case LotRole::School:
		case LotRole::University:
			for (int32 Shelf = 0; Shelf < 4; ++Shelf)
			{
				const double X = -HalfW * 0.7 + Shelf * HalfW * 0.45;
				Out.AddBox(FVector(X - 0.25 * S, -HalfD * 0.6, 0.0), FVector(X + 0.25 * S, HalfD * 0.6, 2.1 * S), Wood);
			}
			break;
		case LotRole::Hospital:
			for (int32 Bed = 0; Bed < 3; ++Bed)
			{
				const double X = -HalfW * 0.6 + Bed * HalfW * 0.6;
				Out.AddBox(FVector(X - 0.45 * S, HalfD * 0.2, 0.5 * S), FVector(X + 0.45 * S, HalfD * 0.8, 0.62 * S), FLinearColor(0.85f, 0.86f, 0.88f));
			}
			break;
		case LotRole::ModShop:
			Out.AddBox(FVector(-HalfW * 0.7, -HalfD * 0.2, 0.0), FVector(HalfW * 0.7, HalfD * 0.2, 0.3 * S), Metal);
			Out.AddBox(FVector(HalfW * 0.4, HalfD * 0.4, 0.0), FVector(HalfW * 0.9, HalfD * 0.9, 1.6 * S), Metal);
			break;
		default:
			// Homes and anything unlisted get the furniture the player actually owns, placed by the caller.
			break;
		}
	}

	void BuildFurniture(const FString& Category, const FVector& At, double YawDegrees, const FLinearColor& Colour, FDTGMesh& Out)
	{
		constexpr double S = 100.0;
		FDTGMesh Piece;
		// Each category is a recognisable silhouette rather than a specific product.
		if (Category.Contains(TEXT("Bed")))
		{
			Piece.AddBox(FVector(-1.0 * S, -0.95 * S, 0.0), FVector(1.0 * S, 0.95 * S, 0.3 * S), FLinearColor(0.2f, 0.14f, 0.09f), false);
			Piece.AddBox(FVector(-1.0 * S, -0.95 * S, 0.3 * S), FVector(1.0 * S, 0.95 * S, 0.55 * S), Colour);
			Piece.AddBox(FVector(-1.05 * S, -0.95 * S, 0.0), FVector(-0.95 * S, 0.95 * S, 1.0 * S), FLinearColor(0.2f, 0.14f, 0.09f));
		}
		else if (Category.Contains(TEXT("Living")) || Category.Contains(TEXT("Lounge")))
		{
			Piece.AddBox(FVector(-1.0 * S, -0.45 * S, 0.0), FVector(1.0 * S, 0.45 * S, 0.42 * S), Colour, false);
			Piece.AddBox(FVector(-1.0 * S, 0.3 * S, 0.42 * S), FVector(1.0 * S, 0.45 * S, 0.85 * S), Colour);
		}
		else if (Category.Contains(TEXT("Kitchen")) || Category.Contains(TEXT("Dining")))
		{
			Piece.AddBox(FVector(-0.75 * S, -0.5 * S, 0.72 * S), FVector(0.75 * S, 0.5 * S, 0.8 * S), Colour, false);
			for (int32 Leg = 0; Leg < 4; ++Leg)
			{
				const double X = (Leg % 2 == 0 ? -0.65 : 0.65) * S;
				const double Y = (Leg < 2 ? -0.4 : 0.4) * S;
				Piece.AddBox(FVector(X - 0.04 * S, Y - 0.04 * S, 0.0), FVector(X + 0.04 * S, Y + 0.04 * S, 0.72 * S), Colour * 0.7f);
			}
		}
		else if (Category.Contains(TEXT("Office")) || Category.Contains(TEXT("Study")))
		{
			Piece.AddBox(FVector(-0.7 * S, -0.35 * S, 0.68 * S), FVector(0.7 * S, 0.35 * S, 0.76 * S), Colour, false);
			Piece.AddBox(FVector(-0.7 * S, -0.35 * S, 0.0), FVector(-0.5 * S, 0.35 * S, 0.68 * S), Colour * 0.8f);
		}
		else
		{
			Piece.AddBox(FVector(-0.4 * S, -0.4 * S, 0.0), FVector(0.4 * S, 0.4 * S, 0.9 * S), Colour, false);
		}

		// Place it: rotate about Z, then move.
		const double Yaw = FMath::DegreesToRadians(YawDegrees);
		const double Cos = FMath::Cos(Yaw);
		const double Sin = FMath::Sin(Yaw);
		for (FVector& Vert : Piece.Verts)
		{
			const FVector Local = Vert;
			Vert = FVector(Local.X * Cos - Local.Y * Sin, Local.X * Sin + Local.Y * Cos, Local.Z) + At;
		}
		for (FVector& Normal : Piece.Normals)
		{
			const FVector Local = Normal;
			Normal = FVector(Local.X * Cos - Local.Y * Sin, Local.X * Sin + Local.Y * Cos, Local.Z);
		}
		Out.Append(Piece);
	}
}

namespace DTGMeshBuilder
{
	UDTGBody* DressPerson(AActor* Person, int32 Seed)
	{
		if (!Person || !Person->GetRootComponent())
		{
			return nullptr;
		}
		UDTGBody* Body = NewObject<UDTGBody>(Person);
		Body->SetupAttachment(Person->GetRootComponent());
		Body->RegisterComponent();
		Body->Build(Seed);
		return Body;
	}

	UMaterialInterface* VertexColorMaterial()
	{
		static const TCHAR* const Path = TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial");
		// Found once. StaticLoadObject hits the asset registry, and this is asked for by every building,
		// every car and every person in the world.
		static TWeakObjectPtr<UMaterialInterface> Cached;
		static bool bWarned = false;
		if (Cached.IsValid())
		{
			return Cached.Get();
		}
		UMaterialInterface* Found = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, Path));
		if (!Found)
		{
			if (!bWarned)
			{
				bWarned = true;
				UE_LOG(LogDTG, Warning,
					TEXT("No vertex-colour material at %s, so the world will draw untextured. In a packaged build this "
						 "means /Engine/EngineDebugMaterials was not cooked; see DirectoriesToAlwaysCook in Config/DefaultGame.ini."),
					Path);
			}
			return nullptr;
		}
		Cached = Found;
		return Found;
	}
}
