// Lots: a property's features have to relate to its building and its road, or they are just more
// scatter. The driveway meets the road, the mailbox stands at the road by the drive, the shrubs hug
// the front wall, the fence and the shed stay on the lot and off the drive, and the ditch is a ditch.
#include "Sim/GameState.h"
#include "Sim/Lots.h"
#include "TestFramework.h"
#include "TestSupport.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace detail_lots_test
{
	bool Touches(const Rect& A, const Rect& B, double Slack)
	{
		return A.MinX <= B.MaxX + Slack && A.MaxX >= B.MinX - Slack && A.MinY <= B.MaxY + Slack && A.MaxY >= B.MinY - Slack;
	}

	bool Inside(const Rect& Inner, const Rect& Outer, double Slack)
	{
		return Inner.MinX >= Outer.MinX - Slack && Inner.MaxX <= Outer.MaxX + Slack && Inner.MinY >= Outer.MinY - Slack && Inner.MaxY <= Outer.MaxY + Slack;
	}

	const BuildingPlan* BuildingOf(const GameState& State, int Id)
	{
		for (const BuildingPlan& Building : State.City.Buildings)
		{
			if (Building.Id == Id)
			{
				return &Building;
			}
		}
		return nullptr;
	}

	/** How far a point lies from the house's front wall plane (positive outside), given the door side. */
	double OutFromFront(const BuildingPlan& Building, double X, double Y)
	{
		switch (Building.DoorSide)
		{
		case 1: return X - Building.Footprint.MaxX;
		case 2: return Y - Building.Footprint.MaxY;
		case 3: return Building.Footprint.MinX - X;
		default: return Building.Footprint.MinY - Y;
		}
	}
}

DTG_TEST(EveryCrossroadsLotHasADriveToTheRoad)
{
	using namespace detail_lots_test;
	const GameState& State = dtg_test::LoadedState();
	int Lots = 0;
	int DrivesOffTheRoad = 0;
	int DrivesThroughTheHouse = 0;
	int MailboxesAway = 0;
	int ShrubsAdrift = 0;
	int FencesAcrossTheDrive = 0;
	int ShedsOffTheLot = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (Town.Style != CityStyle::Rural)
		{
			continue;
		}
		for (const LotFeatures& Lot : LotsIn(State.Map, State.City, Town.Bounds))
		{
			const BuildingPlan* Building = BuildingOf(State, Lot.BuildingId);
			EXPECT_MSG(Building != nullptr, "a lot with no building, id " + std::to_string(Lot.BuildingId));
			if (!Building)
			{
				continue;
			}
			++Lots;
			// The drive runs from the block's edge (the road's edge) to the pad, beside the house, not through it.
			const Rect* Block = nullptr;
			for (const Rect& Candidate : State.City.Blocks)
			{
				if (Candidate.Contains(Building->Footprint.CenterX(), Building->Footprint.CenterY()))
				{
					Block = &Candidate;
				}
			}
			EXPECT_MSG(Block != nullptr, "a crossroads building off every block");
			if (!Block)
			{
				continue;
			}
			const bool bAtEdge = std::fabs(Lot.Driveway.MinY - Block->MinY) < 0.05 || std::fabs(Lot.Driveway.MaxY - Block->MaxY) < 0.05
				|| std::fabs(Lot.Driveway.MinX - Block->MinX) < 0.05 || std::fabs(Lot.Driveway.MaxX - Block->MaxX) < 0.05;
			if (!bAtEdge || !Touches(Lot.Driveway, Lot.Pad, 0.05))
			{
				++DrivesOffTheRoad;
			}
			if (Lot.Driveway.Overlaps(Building->Footprint) || Lot.Pad.Overlaps(Building->Footprint))
			{
				++DrivesThroughTheHouse;
			}
			// The mailbox stands at the road's edge, within reach of the drive.
			const double BoxToEdge = std::min({std::fabs(Lot.MailboxY - Block->MinY), std::fabs(Lot.MailboxY - Block->MaxY),
				std::fabs(Lot.MailboxX - Block->MinX), std::fabs(Lot.MailboxX - Block->MaxX)});
			const double BoxToDrive = std::max({Lot.Driveway.MinX - Lot.MailboxX, Lot.MailboxX - Lot.Driveway.MaxX,
				Lot.Driveway.MinY - Lot.MailboxY, Lot.MailboxY - Lot.Driveway.MaxY, 0.0});
			if (!Lot.bMailbox || BoxToEdge > 1.0 || BoxToDrive > 1.5)
			{
				++MailboxesAway;
			}
			// Shrubs hug the front wall and keep clear of the door.
			for (const Plant& Shrub : Lot.Shrubs)
			{
				const double Out = OutFromFront(*Building, Shrub.X, Shrub.Y);
				const double FromDoor = std::sqrt((Shrub.X - Building->DoorX) * (Shrub.X - Building->DoorX) + (Shrub.Y - Building->DoorY) * (Shrub.Y - Building->DoorY));
				if (Out < 0.6 || Out > 1.6 || FromDoor < 1.5 || Shrub.Kind != PlantKind::Scrub)
				{
					++ShrubsAdrift;
				}
			}
			// The fence stays on the lot and off the drive and the pad; so does the shed.
			for (const LotLine& Line : Lot.Fence)
			{
				const Rect Span{std::min(Line.Ax, Line.Bx) - 0.01, std::min(Line.Ay, Line.By) - 0.01, std::max(Line.Ax, Line.Bx) + 0.01, std::max(Line.Ay, Line.By) + 0.01};
				if (!Inside(Span, Lot.Lot, 0.05) || Span.Overlaps(Lot.Driveway) || Span.Overlaps(Lot.Pad))
				{
					++FencesAcrossTheDrive;
				}
			}
			if (Lot.bShed && (!Inside(Lot.Shed, Lot.Lot, 0.05) || Lot.Shed.Overlaps(Building->Footprint) || Lot.Shed.Overlaps(Lot.Pad) || Lot.Shed.Overlaps(Lot.Driveway)))
			{
				++ShedsOffTheLot;
			}
			// A shed stands in from every road: a corner lot's side yard is beside one.
			const double ShedToEdge = std::min({Lot.Shed.MinX - Block->MinX, Block->MaxX - Lot.Shed.MaxX, Lot.Shed.MinY - Block->MinY, Block->MaxY - Lot.Shed.MaxY});
			if (Lot.bShed && ShedToEdge < 8.0)
			{
				++ShedsOffTheLot;
			}
		}
	}
	EXPECT_MSG(Lots > 10, std::to_string(Lots) + " crossroads lots: too few to mean anything");
	EXPECT_MSG(DrivesOffTheRoad == 0, std::to_string(DrivesOffTheRoad) + " drives do not run from the road's edge to the pad");
	EXPECT_MSG(DrivesThroughTheHouse == 0, std::to_string(DrivesThroughTheHouse) + " drives or pads run through the house");
	EXPECT_MSG(MailboxesAway == 0, std::to_string(MailboxesAway) + " mailboxes stand away from the road or the drive");
	EXPECT_MSG(ShrubsAdrift == 0, std::to_string(ShrubsAdrift) + " shrubs stand off the front wall or in the door");
	EXPECT_MSG(FencesAcrossTheDrive == 0, std::to_string(FencesAcrossTheDrive) + " fence lines leave the lot or cross the drive");
	EXPECT_MSG(ShedsOffTheLot == 0, std::to_string(ShedsOffTheLot) + " sheds stand off the lot or in something");
}

// The same lot, whichever piece of ground is asked for.
DTG_TEST(LotsAreStableInWorldSpace)
{
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Longs = nullptr;
	for (const CityInfo& Town : State.City.Cities)
	{
		if (Town.bStart)
		{
			Longs = &Town;
		}
	}
	EXPECT_MSG(Longs != nullptr && Longs->Style == CityStyle::Rural, "the start town is not a crossroads community");
	if (!Longs)
	{
		return;
	}
	const std::vector<LotFeatures> Whole = LotsIn(State.Map, State.City, Longs->Bounds);
	const Rect West{Longs->Bounds.MinX, Longs->Bounds.MinY, Longs->Bounds.CenterX(), Longs->Bounds.MaxY};
	int Matched = 0;
	for (const LotFeatures& Half : LotsIn(State.Map, State.City, West))
	{
		for (const LotFeatures& Full : Whole)
		{
			if (Full.BuildingId == Half.BuildingId)
			{
				Matched += std::fabs(Full.Driveway.MinX - Half.Driveway.MinX) < 1e-9 && std::fabs(Full.MailboxX - Half.MailboxX) < 1e-9
					&& Full.Shrubs.size() == Half.Shrubs.size() && Full.Fence.size() == Half.Fence.size();
			}
		}
	}
	EXPECT_MSG(Matched > 0 && Matched <= static_cast<int>(Whole.size()), std::to_string(Matched) + " lots match between the half and the whole");
}

// The ditch beside a crossroads community's streets: flat shoulder, a swale, then the lawn again.
DTG_TEST(CrossroadsStreetsHaveDitches)
{
	EXPECT_MSG(DitchDepthM(0.5) == 0.0, "the shoulder is dug out");
	EXPECT_MSG(DitchDepthM(3.0) > 0.3 && DitchDepthM(3.0) < 0.5, "the ditch is " + std::to_string(DitchDepthM(3.0)) + " m deep at its middle");
	EXPECT_MSG(DitchDepthM(6.0) == 0.0, "the ditch runs into the lot");
	// And the ground itself follows it beside a street in Longs, but not beside a street in the hub.
	const GameState& State = dtg_test::LoadedState();
	int Checked = 0;
	int Flat = 0;
	int HubDug = 0;
	for (const CityInfo& Town : State.City.Cities)
	{
		for (const RoadPlan& Road : State.City.Roads)
		{
			if (!Town.Bounds.Contains(Road.Area.CenterX(), Road.Area.CenterY()) || !Road.bHorizontal)
			{
				continue;
			}
			// Beside the road's north edge a quarter of the way along (its middle is a crossroads), 3 m out and 0.5 m out.
			const double X = Road.Area.MinX + 0.25 * Road.Area.Width();
			const double Grade = State.Map.SurfaceAt(X, Road.Area.CenterY()).GroundM;
			const double InDitch = State.Map.SurfaceAt(X, Road.Area.MaxY + 3.0).GroundM;
			const double OnShoulder = State.Map.SurfaceAt(X, Road.Area.MaxY + 0.5).GroundM;
			if (Town.Style == CityStyle::Rural)
			{
				++Checked;
				if (Grade - InDitch < 0.3 || std::fabs(Grade - OnShoulder) > 0.05)
				{
					++Flat;
				}
			}
			else if (Town.Style == CityStyle::Hub && Grade - InDitch > 0.1)
			{
				++HubDug;
			}
			break;
		}
	}
	EXPECT_MSG(Checked > 0, "no crossroads street to check");
	EXPECT_MSG(Flat == 0, std::to_string(Flat) + " of " + std::to_string(Checked) + " crossroads streets have no ditch beside them");
	EXPECT_MSG(HubDug == 0, std::to_string(HubDug) + " hub streets have a ditch");
}
