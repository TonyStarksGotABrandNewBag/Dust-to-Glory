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

void FDTGMesh::AddWheel(const FVector& Centre, double RadiusUu, double WidthUu, const FLinearColor& Tyre, const FLinearColor& Rim, int32 Sides)
{
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
		AddTri(HubIn, P0 - FVector(0, Half, 0), P1 - FVector(0, Half, 0), FVector(0, -1, 0), Rim);
		AddTri(HubOut, P1 + FVector(0, Half, 0), P0 + FVector(0, Half, 0), FVector(0, 1, 0), Rim);
	}
}

void FDTGMesh::AddRoof(const FVector& Min, const FVector& Max, double RidgeHeightUu, const FLinearColor& Colour)
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
		AddTri(FVector(Min.X, Min.Y, Min.Z), R0, FVector(Min.X, Max.Y, Min.Z), FVector(-1, 0, 0), Colour);
		AddTri(FVector(Max.X, Max.Y, Min.Z), R1, FVector(Max.X, Min.Y, Min.Z), FVector(1, 0, 0), Colour);
	}
	else
	{
		const FVector R0(MidX, Min.Y, Top);
		const FVector R1(MidX, Max.Y, Top);
		AddQuad(FVector(Min.X, Min.Y, Min.Z), R0, R1, FVector(Min.X, Max.Y, Min.Z), FVector(-1, 0, 1).GetSafeNormal(), Colour);
		AddQuad(FVector(Max.X, Max.Y, Min.Z), R1, R0, FVector(Max.X, Min.Y, Min.Z), FVector(1, 0, 1).GetSafeNormal(), Colour);
		AddTri(FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), R0, FVector(0, -1, 0), Colour);
		AddTri(FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), R1, FVector(0, 1, 0), Colour);
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
		// Downtown towers are glass and steel; everything else gets a roof you can see.
		Palette.Roof = Building.bDowntown ? FLinearColor(0.12f, 0.12f, 0.13f) : Palette.Wall * 0.55f;
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

	void BuildBuilding(const BuildingPlan& Building, double GroundZ, double S, FDTGMesh& Out)
	{
		const FPalette Palette = PaletteFor(Building);
		const double MinX = Building.Footprint.MinX * S;
		const double MinY = Building.Footprint.MinY * S;
		const double MaxX = Building.Footprint.MaxX * S;
		const double MaxY = Building.Footprint.MaxY * S;
		const double Base = GroundZ * S;
		const int32 Floors = FMath::Max(1, Building.Floors);
		const double Height = FMath::Max(3.0, Building.HeightM) * S;
		const double FloorHeight = Height / Floors;

		// Walls, floor by floor: a band of wall, then a band of window, so the facade has a storey count
		// you can read from the street.
		for (int32 Floor = 0; Floor < Floors; ++Floor)
		{
			const double Low = Base + Floor * FloorHeight;
			const double SillHeight = FloorHeight * 0.3;
			const double GlassHeight = FloorHeight * 0.45;
			Out.AddBox(FVector(MinX, MinY, Low), FVector(MaxX, MaxY, Low + SillHeight), Palette.Wall);
			// The window band is inset a little so it reads as glass set into the wall.
			const double Inset = FMath::Min(12.0, (MaxX - MinX) * 0.02);
			Out.AddBox(FVector(MinX + Inset, MinY + Inset, Low + SillHeight),
				FVector(MaxX - Inset, MaxY - Inset, Low + SillHeight + GlassHeight), Palette.Window);
			Out.AddBox(FVector(MinX, MinY, Low + SillHeight + GlassHeight), FVector(MaxX, MaxY, Low + FloorHeight), Palette.Wall);
		}

		// Roof: flat with a parapet on anything tall, pitched on a house or a church.
		const double Top = Base + Height;
		const bool bPitched = Building.Role == LotRole::Home || Building.Role == LotRole::Church || Floors <= 1;
		if (bPitched)
		{
			const double Ridge = FMath::Min(FMath::Min(MaxX - MinX, MaxY - MinY) * 0.35, 4.0 * S);
			Out.AddRoof(FVector(MinX - 0.25 * S, MinY - 0.25 * S, Top), FVector(MaxX + 0.25 * S, MaxY + 0.25 * S, Top), Ridge, Palette.Roof);
		}
		else
		{
			Out.AddBox(FVector(MinX, MinY, Top), FVector(MaxX, MaxY, Top + 0.1 * S), Palette.Roof);
			const double Wall = 0.25 * S;
			Out.AddBox(FVector(MinX, MinY, Top), FVector(MaxX, MinY + Wall, Top + 0.9 * S), Palette.Roof);
			Out.AddBox(FVector(MinX, MaxY - Wall, Top), FVector(MaxX, MaxY, Top + 0.9 * S), Palette.Roof);
			Out.AddBox(FVector(MinX, MinY, Top), FVector(MinX + Wall, MaxY, Top + 0.9 * S), Palette.Roof);
			Out.AddBox(FVector(MaxX - Wall, MinY, Top), FVector(MaxX, MaxY, Top + 0.9 * S), Palette.Roof);
		}

		// A door on the side the simulation put it, so the way in is where the game says it is.
		const double DoorWide = 1.2 * S;
		const double DoorHigh = 2.3 * S;
		const double Dx = Building.DoorX * S;
		const double Dy = Building.DoorY * S;
		const double Lip = 0.12 * S;
		switch (Building.DoorSide)
		{
		case 0: Out.AddBox(FVector(Dx - DoorWide, MinY - Lip, Base), FVector(Dx + DoorWide, MinY + Lip, Base + DoorHigh), Palette.Trim, false); break;
		case 1: Out.AddBox(FVector(MaxX - Lip, Dy - DoorWide, Base), FVector(MaxX + Lip, Dy + DoorWide, Base + DoorHigh), Palette.Trim, false); break;
		case 2: Out.AddBox(FVector(Dx - DoorWide, MaxY - Lip, Base), FVector(Dx + DoorWide, MaxY + Lip, Base + DoorHigh), Palette.Trim, false); break;
		default: Out.AddBox(FVector(MinX - Lip, Dy - DoorWide, Base), FVector(MinX + Lip, Dy + DoorWide, Base + DoorHigh), Palette.Trim, false); break;
		}

		// A steeple on a church, because a town skyline needs something that isn't a box.
		if (Building.Role == LotRole::Church)
		{
			const double Cx = 0.5 * (MinX + MaxX);
			const double Cy = 0.5 * (MinY + MaxY);
			const double Wide = 1.1 * S;
			Out.AddBox(FVector(Cx - Wide, Cy - Wide, Top), FVector(Cx + Wide, Cy + Wide, Top + 5.0 * S), Palette.Wall);
			Out.AddTaperedBox(FVector(Cx - Wide, Cy - Wide, Top + 5.0 * S), FVector(Cx + Wide, Cy + Wide, Top + 9.0 * S),
				0.48, 0.48, 0.48, Palette.Roof);
		}
	}

	// ======================================================================================== Cars

	void BuildCarBody(BodyStyle Body, double LengthM, double WidthM, double HeightM, const FLinearColor& Paint, FDTGMesh& Out)
	{
		constexpr double S = 100.0; // metres to Unreal units
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
			Out.AddTaperedBox(FVector(CabinMinX, -HalfW + CabinInset, LowerTop), FVector(CabinMaxX, HalfW - CabinInset, Floor + Height * 0.92),
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
		Out.AddBox(FVector(HalfL - 0.06 * S, -HalfW * 0.82, LightZ), FVector(HalfL + 0.02 * S, -HalfW * 0.82 + LightW, LightZ + LightH), Headlight);
		Out.AddBox(FVector(HalfL - 0.06 * S, HalfW * 0.82 - LightW, LightZ), FVector(HalfL + 0.02 * S, HalfW * 0.82, LightZ + LightH), Headlight);
		Out.AddBox(FVector(-HalfL - 0.02 * S, -HalfW * 0.82, LightZ), FVector(-HalfL + 0.06 * S, -HalfW * 0.82 + LightW, LightZ + LightH), Taillight);
		Out.AddBox(FVector(-HalfL - 0.02 * S, HalfW * 0.82 - LightW, LightZ), FVector(-HalfL + 0.06 * S, HalfW * 0.82, LightZ + LightH), Taillight);
	}

	void BuildWheel(double RadiusM, double WidthM, const FLinearColor& Rim, FDTGMesh& Out)
	{
		constexpr double S = 100.0;
		Out.AddWheel(FVector::ZeroVector, RadiusM * S, FMath::Max(0.12, WidthM) * S, FLinearColor(0.025f, 0.025f, 0.028f), Rim);
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
		static const TCHAR* const Path = TEXT("/Engine/EngineMaterials/VertexColorMaterial.VertexColorMaterial");
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
						 "means /Engine/EngineMaterials was not cooked; see DirectoriesToAlwaysCook in Config/DefaultGame.ini."),
					Path);
			}
			return nullptr;
		}
		Cached = Found;
		return Found;
	}
}
