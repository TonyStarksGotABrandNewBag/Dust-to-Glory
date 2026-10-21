#include "Sim/Lots.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dtg::sim
{
	namespace detail_lots
	{
		constexpr double Pi = 3.14159265358979;
		/** The strip between the block's edge and its lots: a sidewalk in a town, lawn and ditch here. */
		constexpr double BlockMarginM = 4.0;
		constexpr double AlleyM = 4.0;

		/** Stable per building and purpose, as the renderer's VarietyOf is. */
		std::uint32_t Hash(int Id, std::uint32_t Salt)
		{
			std::uint32_t H = static_cast<std::uint32_t>(Id) * 2654435761u ^ Salt * 0x9E3779B9u;
			H ^= H >> 15;
			H *= 0x85EBCA6Bu;
			H ^= H >> 13;
			return H;
		}

		/**
		 * The lot's own axes: Along runs with the road the door faces, Out runs from the road's edge into
		 * the lot. Every feature is laid out in these and turned into world rectangles at the end, so the
		 * four door sides share one piece of geometry.
		 */
		struct Frame
		{
			double Ox = 0.0;
			double Oy = 0.0;
			double Ax = 1.0;
			double Ay = 0.0;
			double Nx = 0.0;
			double Ny = 1.0;

			void ToFrame(double X, double Y, double& Along, double& Out) const
			{
				Along = (X - Ox) * Ax + (Y - Oy) * Ay;
				Out = (X - Ox) * Nx + (Y - Oy) * Ny;
			}
			void ToWorld(double Along, double Out, double& X, double& Y) const
			{
				X = Ox + Ax * Along + Nx * Out;
				Y = Oy + Ay * Along + Ny * Out;
			}
			Rect RectOf(double A0, double A1, double N0, double N1) const
			{
				double X0, Y0, X1, Y1;
				ToWorld(A0, N0, X0, Y0);
				ToWorld(A1, N1, X1, Y1);
				return Rect{std::min(X0, X1), std::min(Y0, Y1), std::max(X0, X1), std::max(Y0, Y1)};
			}
			/** The yaw, radians, of the Out direction: what faces the road faces -Out. */
			double OutYaw() const { return std::atan2(Ny, Nx); }
			double AlongYaw() const { return std::atan2(Ay, Ax); }
		};

		Frame FrameFor(const Rect& Block, const Rect& Lot, int DoorSide)
		{
			Frame F;
			switch (DoorSide)
			{
			case 1: F = {Block.MaxX, Lot.MinY, 0.0, 1.0, -1.0, 0.0}; break; // road to the east
			case 2: F = {Lot.MinX, Block.MaxY, 1.0, 0.0, 0.0, -1.0}; break; // north
			case 3: F = {Block.MinX, Lot.MinY, 0.0, 1.0, 1.0, 0.0}; break;  // west
			default: F = {Lot.MinX, Block.MinY, 1.0, 0.0, 0.0, 1.0}; break; // south
			}
			return F;
		}

		/** The lot a building stands on: the quadrant of its block, as the layout cut it. */
		bool LotOf(const CityPlan& World, const BuildingPlan& Building, Rect& Block, Rect& Lot)
		{
			const double Cx = Building.Footprint.CenterX();
			const double Cy = Building.Footprint.CenterY();
			for (const Rect& Candidate : World.Blocks)
			{
				if (!Candidate.Contains(Cx, Cy))
				{
					continue;
				}
				Block = Candidate;
				const double LotSize = 0.5 * (Block.Width() - 2.0 * BlockMarginM - AlleyM);
				if (LotSize < 10.0)
				{
					return false;
				}
				const int A = Cx > Block.CenterX() ? 1 : 0;
				const int B = Cy > Block.CenterY() ? 1 : 0;
				const double X0 = Block.MinX + BlockMarginM + A * (LotSize + AlleyM);
				const double Y0 = Block.MinY + BlockMarginM + B * (LotSize + AlleyM);
				Lot = Rect{X0, Y0, X0 + LotSize, Y0 + LotSize};
				return true;
			}
			return false;
		}

		LotFeatures FeaturesOf(const StatePlan& Map, const CityPlan& World, const BuildingPlan& Building, const Rect& Block, const Rect& Lot,
			const LotRules& Rules)
		{
			LotFeatures Out;
			Out.BuildingId = Building.Id;
			Out.Lot = Lot;
			const Frame F = FrameFor(Block, Lot, Building.DoorSide);
			// The lot and the house in the lot's own axes.
			double LotA0, LotA1, LotN0, LotN1;
			{
				double A[4], N[4];
				F.ToFrame(Lot.MinX, Lot.MinY, A[0], N[0]);
				F.ToFrame(Lot.MaxX, Lot.MinY, A[1], N[1]);
				F.ToFrame(Lot.MaxX, Lot.MaxY, A[2], N[2]);
				F.ToFrame(Lot.MinX, Lot.MaxY, A[3], N[3]);
				LotA0 = std::min({A[0], A[1], A[2], A[3]});
				LotA1 = std::max({A[0], A[1], A[2], A[3]});
				LotN0 = std::min({N[0], N[1], N[2], N[3]});
				LotN1 = std::max({N[0], N[1], N[2], N[3]});
			}
			double HouseA0, HouseA1, HouseN0, HouseN1;
			{
				const Rect& H = Building.Footprint;
				double A[4], N[4];
				F.ToFrame(H.MinX, H.MinY, A[0], N[0]);
				F.ToFrame(H.MaxX, H.MinY, A[1], N[1]);
				F.ToFrame(H.MaxX, H.MaxY, A[2], N[2]);
				F.ToFrame(H.MinX, H.MaxY, A[3], N[3]);
				HouseA0 = std::min({A[0], A[1], A[2], A[3]});
				HouseA1 = std::max({A[0], A[1], A[2], A[3]});
				HouseN0 = std::min({N[0], N[1], N[2], N[3]});
				HouseN1 = std::max({N[0], N[1], N[2], N[3]});
			}
			double DoorA, DoorN;
			F.ToFrame(Building.DoorX, Building.DoorY, DoorA, DoorN);

			// The pad goes on the side of the house with the room, level with its front.
			const double RoomLeft = HouseA0 - LotA0;
			const double RoomRight = LotA1 - HouseA1;
			const bool bRight = RoomRight >= RoomLeft;
			const double Room = bRight ? RoomRight : RoomLeft;
			const double PadWide = std::max(Rules.DriveWidthM, std::min(Rules.PadM, Room - 1.0));
			const double PadA0 = bRight ? HouseA1 + 0.5 : HouseA0 - 0.5 - PadWide;
			const double PadA1 = PadA0 + PadWide;
			const double PadN0 = HouseN0;
			const double PadN1 = std::min(LotN1, HouseN0 + Rules.PadM);
			Out.Pad = F.RectOf(PadA0, PadA1, PadN0, PadN1);
			// The drive, from the road's edge (the block's edge, Out 0) up to the pad.
			const double DriveA = 0.5 * (PadA0 + PadA1);
			Out.Driveway = F.RectOf(DriveA - 0.5 * Rules.DriveWidthM, DriveA + 0.5 * Rules.DriveWidthM, 0.0, PadN0);
			Out.Paving = Hash(Building.Id, 21) % static_cast<std::uint32_t>(Rules.ConcreteOneIn) < 2u ? LotPaving::Concrete : LotPaving::Gravel;
			// The walk: a strip in front of the wall from the door to the pad's near edge.
			const double WalkN0 = HouseN0 - 0.4 - Rules.PathWidthM;
			const double WalkN1 = HouseN0 - 0.4;
			Out.Path = F.RectOf(std::min(DoorA - 0.6, bRight ? PadA0 : PadA1), std::max(DoorA + 0.6, bRight ? PadA0 : PadA1), WalkN0, WalkN1);
			// The mailbox, at the road's edge on the far side of the drive from the house, facing the road.
			Out.bMailbox = true;
			const double BoxA = DriveA + (bRight ? 1.0 : -1.0) * (0.5 * Rules.DriveWidthM + 0.5);
			F.ToWorld(BoxA, 0.6, Out.MailboxX, Out.MailboxY);
			Out.MailboxYaw = F.OutYaw() + Pi;
			// The culvert under the drive where it crosses the ditch; the pipe runs with the road.
			Out.bCulvert = true;
			F.ToWorld(DriveA, 0.5 * (DitchFromM + DitchToM), Out.CulvertX, Out.CulvertY);
			Out.CulvertYaw = F.AlongYaw();
			// Shrubs along the front wall, a stride apart, clear of the door and the walk's end.
			for (double A = HouseA0 + 1.0; A <= HouseA1 - 1.0 + 1e-9; A += Rules.ShrubSpacingM)
			{
				if (std::fabs(A - DoorA) < 1.6)
				{
					continue;
				}
				Plant Shrub;
				F.ToWorld(A, HouseN0 - Rules.ShrubOutM, Shrub.X, Shrub.Y);
				Shrub.GroundM = Map.HeightAt(Shrub.X, Shrub.Y);
				Shrub.Kind = PlantKind::Scrub;
				const std::uint32_t Seed = Hash(Building.Id, static_cast<std::uint32_t>(A * 10.0));
				Shrub.HeightM = 0.8 + 0.6 * static_cast<double>(Seed % 1000u) / 999.0;
				Shrub.RadiusM = 0.6 + 0.3 * Shrub.HeightM;
				Shrub.Spin = 2.0 * Pi * static_cast<double>((Seed >> 10) % 1000u) / 999.0;
				Out.Shrubs.push_back(Shrub);
			}
			// A fence on some lots: both sides, from behind the pad to the back, and the back.
			if (Hash(Building.Id, 22) % static_cast<std::uint32_t>(Rules.FenceOneIn) == 0u)
			{
				const double FromN = std::max(HouseN0, PadN1) + 0.5;
				LotLine Left, Right, Back;
				F.ToWorld(LotA0, FromN, Left.Ax, Left.Ay);
				F.ToWorld(LotA0, LotN1, Left.Bx, Left.By);
				F.ToWorld(LotA1, FromN, Right.Ax, Right.Ay);
				F.ToWorld(LotA1, LotN1, Right.Bx, Right.By);
				F.ToWorld(LotA0, LotN1, Back.Ax, Back.Ay);
				F.ToWorld(LotA1, LotN1, Back.Bx, Back.By);
				Out.Fence = {Left, Right, Back};
			}
			// A shed in the back corner, on some lots: on the side that faces into the block (a corner lot's
			// other side is on a road, and a shed on a road is a shed in the way), away from the drive if it can.
			if (Hash(Building.Id, 23) % static_cast<std::uint32_t>(Rules.ShedOneIn) == 0u && LotN1 - HouseN1 > 8.0)
			{
				double LeftX, LeftY, RightX, RightY;
				F.ToWorld(LotA0, LotN1, LeftX, LeftY);
				F.ToWorld(LotA1, LotN1, RightX, RightY);
				const double LeftToEdge = std::min({LeftX - Block.MinX, Block.MaxX - LeftX, LeftY - Block.MinY, Block.MaxY - LeftY});
				const double RightToEdge = std::min({RightX - Block.MinX, Block.MaxX - RightX, RightY - Block.MinY, Block.MaxY - RightY});
				const bool bShedLeft = std::fabs(LeftToEdge - RightToEdge) > 1.0 ? LeftToEdge > RightToEdge : bRight;
				const double ShedA0 = bShedLeft ? LotA0 + 1.5 : LotA1 - 4.5;
				Out.Shed = F.RectOf(ShedA0, ShedA0 + 3.0, LotN1 - 5.1, LotN1 - 1.5);
				Out.bShed = true;
			}
			(void)World;
			(void)DoorN;
			(void)HouseN1;
			return Out;
		}
	}

	double DitchDepthM(double OffEdgeM)
	{
		if (OffEdgeM <= DitchFromM || OffEdgeM >= DitchToM)
		{
			return 0.0;
		}
		return 0.45 * std::sin(detail_lots::Pi * (OffEdgeM - DitchFromM) / (DitchToM - DitchFromM));
	}

	std::vector<LotFeatures> LotsIn(const StatePlan& Map, const CityPlan& World, const Rect& Area, const LotRules& Rules)
	{
		using namespace detail_lots;
		std::vector<LotFeatures> Out;
		for (const CityInfo& Town : World.Cities)
		{
			if (Town.Style != CityStyle::Rural || !Town.Bounds.Overlaps(Area))
			{
				continue;
			}
			for (const BuildingPlan& Building : World.Buildings)
			{
				if (Out.size() >= Rules.Limit)
				{
					return Out;
				}
				if (Building.Id < Town.FirstBuildingId || Building.Id > Town.LastBuildingId)
				{
					continue;
				}
				Rect Block, Lot;
				if (!LotOf(World, Building, Block, Lot) || !Lot.Overlaps(Area))
				{
					continue;
				}
				Out.push_back(FeaturesOf(Map, World, Building, Block, Lot, Rules));
			}
		}
		return Out;
	}
}
