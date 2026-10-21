// Land cover: every point of the state is one kind of ground, decided once and the same whichever area
// asked, and the kinds lie where a Horry County map would put them -- a home's lot is a yard, a store's
// is cleared, the ditch is the road's, the country round a crossroads is woods, and beyond it the woods
// have fields between them.
#include "Sim/LandCover.h"

#include "TestFramework.h"
#include "TestSupport.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace detail_landcover_test
{
	Rect Around(double X, double Y, double Half)
	{
		return Rect{X - Half, Y - Half, X + Half, Y + Half};
	}

	const CityInfo* StartTown(const GameState& State)
	{
		for (const CityInfo& Town : State.City.Cities)
		{
			if (Town.bStart)
			{
				return &Town;
			}
		}
		return nullptr;
	}
}

DTG_TEST(LandCoverTellsYardsFromLotsFromRoads)
{
	using namespace detail_landcover_test;
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Longs = StartTown(State);
	EXPECT_MSG(Longs != nullptr, "no start town");
	if (!Longs)
	{
		return;
	}
	const double Cx = Longs->Bounds.CenterX();
	const double Cy = Longs->Bounds.CenterY();
	const LandCoverMap Cover(State.Map, State.City, Around(Cx, Cy, 400.0));

	// Every property: its pad is on its own lot, a yard for a home and cleared for anything else; its
	// mailbox stands on the roadside strip, which is the road's and not the lot's.
	int Yards = 0;
	int Commercial = 0;
	int WrongLot = 0;
	int WrongMailbox = 0;
	std::string Wrong;
	for (const LotFeatures& Lot : Cover.Lots())
	{
		const BuildingPlan* Building = State.City.FindBuilding(Lot.BuildingId);
		if (!Building || !Longs->Bounds.Contains(Lot.Lot.CenterX(), Lot.Lot.CenterY()))
		{
			continue;
		}
		const LandCover Got = Cover.At(Lot.Pad.CenterX(), Lot.Pad.CenterY());
		const LandCover Want = Building->Role == LotRole::Home ? LandCover::Yard : LandCover::Commercial;
		(Want == LandCover::Yard ? Yards : Commercial) += 1;
		if (Got != Want)
		{
			++WrongLot;
			Wrong += " lot " + std::to_string(Lot.BuildingId) + " pad is " + LandCoverName(Got) + " (" + std::to_string(static_cast<int>(State.Map.StreetEdgeDistance(Lot.Pad.CenterX(), Lot.Pad.CenterY()))) + " m off the street);";
		}
		if (Lot.bMailbox && Cover.At(Lot.MailboxX, Lot.MailboxY) != LandCover::Roadside)
		{
			++WrongMailbox;
			Wrong += " lot " + std::to_string(Lot.BuildingId) + " mailbox is " + LandCoverName(Cover.At(Lot.MailboxX, Lot.MailboxY)) + ";";
		}
	}
	EXPECT_MSG(Yards > 8, std::to_string(Yards) + " homes with lots in Longs");
	EXPECT_MSG(Commercial > 2, std::to_string(Commercial) + " stores or churches with lots in Longs");
	EXPECT_MSG(WrongLot == 0, std::to_string(WrongLot) + " lots are not the cover their building makes them:" + Wrong);
	EXPECT_MSG(WrongMailbox == 0, std::to_string(WrongMailbox) + " mailboxes stand on something other than the roadside:" + Wrong);

	// The ditch beside the outer street, just beyond the town's bounds and off the highway, is the road's too:
	// without that rule it would be the woods, and the shoulder would grow pines.
	EXPECT_MSG(Cover.At(Longs->Bounds.MaxX + 2.0, Longs->Bounds.MinY + 0.25 * Longs->Bounds.Depth()) == LandCover::Roadside,
		"2 m beyond Longs' outer street is " + std::string(LandCoverName(Cover.At(Longs->Bounds.MaxX + 2.0, Longs->Bounds.MinY + 0.25 * Longs->Bounds.Depth()))) + ", not the roadside");

	// The streets are streets.
	int Streets = 0;
	for (const RoadPlan& Road : State.City.Roads)
	{
		if (Longs->Bounds.Contains(Road.Area.CenterX(), Road.Area.CenterY()))
		{
			++Streets;
			EXPECT_MSG(Cover.At(Road.Area.CenterX(), Road.Area.CenterY()) == LandCover::Street, "the middle of " + Road.Name + " is not a street");
		}
	}
	EXPECT_MSG(Streets > 0, "no street in Longs");

	// A ring 60 m outside the town is woods wherever it is not a road leaving town.
	int Ring = 0;
	int Woods = 0;
	for (int Step = 0; Step < 24; ++Step)
	{
		const double Angle = Step * 0.2617993878;
		const double Half = 0.5 * std::max(Longs->Bounds.Width(), Longs->Bounds.Depth()) + 60.0;
		const double X = Cx + Half * std::cos(Angle);
		const double Y = Cy + Half * std::sin(Angle);
		const LandCover Got = Cover.At(X, Y);
		if (Got == LandCover::Road || Got == LandCover::Roadside || Got == LandCover::Water)
		{
			continue;
		}
		++Ring;
		Woods += Got == LandCover::Woodland ? 1 : 0;
	}
	EXPECT_MSG(Ring > 12 && Woods == Ring, std::to_string(Woods) + " of " + std::to_string(Ring) + " points 60 m outside Longs are woods");

	// A highway is a road, with a mown verge beside it, and open water is water.
	EXPECT_MSG(!State.Map.Highways.empty(), "no highways");
	if (!State.Map.Highways.empty())
	{
		const HighwayPlan& Road = State.Map.Highways.front();
		const std::array<double, 3> Mid = Road.At(0.5 * Road.LengthM());
		const LandCoverMap There(State.Map, State.City, Around(Mid[0], Mid[1], 50.0));
		EXPECT_MSG(There.At(Mid[0], Mid[1]) == LandCover::Road, "the middle of a highway is not a road");
	}
	int Water = 0;
	int Wet = 0;
	for (double X = Cx - 3000.0; X <= Cx + 3000.0 && Wet < 20; X += 150.0)
	{
		for (double Y = Cy - 3000.0; Y <= Cy + 3000.0 && Wet < 20; Y += 150.0)
		{
			if (State.Map.InsideState(X, Y) && State.Map.SurfaceAt(X, Y).bWater)
			{
				++Wet;
				const LandCoverMap There(State.Map, State.City, Around(X, Y, 50.0));
				Water += There.At(X, Y) == LandCover::Water ? 1 : 0;
			}
		}
	}
	EXPECT_MSG(Wet > 0, "no open water within three kilometres of Longs");
	EXPECT_MSG(Water == Wet, std::to_string(Wet - Water) + " of " + std::to_string(Wet) + " wet points are not water");
}

// Beyond the woods round a crossroads community the country is fields among woods, a fair share of
// each, and the answer for a point does not depend on which area was asked.
DTG_TEST(FieldsLieAmongTheWoods)
{
	using namespace detail_landcover_test;
	const GameState& State = dtg_test::LoadedState();
	const CityInfo* Longs = StartTown(State);
	EXPECT_MSG(Longs != nullptr, "no start town");
	if (!Longs)
	{
		return;
	}
	const double Cx = Longs->Bounds.CenterX();
	const double Cy = Longs->Bounds.CenterY();
	const LandCoverRules Rules;
	const LandCoverMap Cover(State.Map, State.City, Around(Cx, Cy, 1600.0), Rules);
	int Country = 0;
	int Fields = 0;
	int Disagree = 0;
	for (double X = Cx - 1500.0; X <= Cx + 1500.0; X += 60.0)
	{
		for (double Y = Cy - 1500.0; Y <= Cy + 1500.0; Y += 60.0)
		{
			if (!State.Map.InsideState(X, Y) || NearCrossroads(State.City, X, Y, Rules.CrossroadsWoodsM))
			{
				continue;
			}
			const LandCover Got = Cover.At(X, Y);
			if (Got != LandCover::Field && Got != LandCover::Woodland)
			{
				continue;
			}
			++Country;
			Fields += Got == LandCover::Field ? 1 : 0;
			// The same point from a map built over a different, smaller area.
			if (Country % 7 == 0)
			{
				const LandCoverMap Other(State.Map, State.City, Around(X + 30.0, Y - 30.0, 80.0), Rules);
				Disagree += Other.At(X, Y) == Got ? 0 : 1;
			}
		}
	}
	EXPECT_MSG(Country > 300, std::to_string(Country) + " points of open country round Longs");
	const double Share = Country > 0 ? static_cast<double>(Fields) / Country : 0.0;
	EXPECT_MSG(Share > 0.25 && Share < 0.75, std::to_string(Fields) + " of " + std::to_string(Country) + " points of country are field: " +
		std::to_string(static_cast<int>(Share * 100.0)) + "%, not cultivated country");
	EXPECT_MSG(Disagree == 0, std::to_string(Disagree) + " points change cover depending on the area asked");
}
