// Road markings. Paint that wanders off the tarmac is worse than no paint, and paint that breaks
// the US convention is the kind of thing a player who drives these roads every day spots in the
// first ten seconds: yellow separates opposing traffic, white separates traffic going the same way,
// and a divided highway has no yellow down its middle.
#include "Sim/RoadMarkings.h"

#include "TestFramework.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

using namespace dtg::sim;

namespace
{
	bool IsYellow(const RoadMark& Mark) { return Mark.Rgb[2] < 0.2 && Mark.Rgb[0] > 0.3; }

	/** Distance from a point to the road's centreline, and how far along the road that lands. */
	double ToCentre(const HighwayPlan& Road, double X, double Y, double* OutAlong = nullptr)
	{
		double Best = 1e18;
		double Walked = 0.0;
		for (std::size_t Part = 0; Part + 1 < Road.Points.size(); ++Part)
		{
			const auto& A = Road.Points[Part];
			const auto& B = Road.Points[Part + 1];
			const double Dx = B[0] - A[0];
			const double Dy = B[1] - A[1];
			const double Len2 = Dx * Dx + Dy * Dy;
			const double Len = std::sqrt(Len2);
			double T = Len2 > 1e-9 ? ((X - A[0]) * Dx + (Y - A[1]) * Dy) / Len2 : 0.0;
			T = std::clamp(T, 0.0, 1.0);
			const double Distance = std::hypot(A[0] + T * Dx - X, A[1] + T * Dy - Y);
			if (Distance < Best)
			{
				Best = Distance;
				if (OutAlong)
				{
					*OutAlong = Walked + T * Len;
				}
			}
			Walked += Len;
		}
		return Best;
	}
}

// Every stripe is on the road it belongs to, and on its surface rather than hovering over it or
// buried under it.
DTG_TEST(MarkingsStayOnTheRoad)
{
	const char* States[] = {"state.json", "state-florida.json", "state-northcarolina.json"};
	for (const char* Which : States)
	{
		GameState State;
		ContentReport Report;
		State.LoadContent(dtg_test::LoadAllFiles(Which), Report);
		EXPECT_MSG(Report.Errors.empty(), std::string(Which) + " failed to load");

		std::size_t Total = 0;
		std::size_t OffRoad = 0;
		std::size_t Floating = 0;
		std::string First;
		for (const HighwayPlan& Road : State.Map.Highways)
		{
			const std::vector<RoadMark> Marks = MarkHighway(Road);
			if (Road.LengthM() > 50.0 && Road.WidthM >= 2.0)
			{
				EXPECT_MSG(!Marks.empty(), std::string(Which) + ": " + Road.Name + " has no paint at all");
			}
			const double Half = 0.5 * Road.WidthM;
			for (const RoadMark& Mark : Marks)
			{
				++Total;
				// Along the whole stripe, not just its ends. A stripe is a straight quad, so on a
				// bend its middle is what leaves the road first -- and the subdivision deliberately
				// emits long quads wherever it believes the road is straight, so this is exactly
				// the assumption that needs checking.
				double Worst = 0.0;
				for (int Step = 0; Step <= 8; ++Step)
				{
					const double T = Step / 8.0;
					const double Px = Mark.Ax + (Mark.Bx - Mark.Ax) * T;
					const double Py = Mark.Ay + (Mark.By - Mark.Ay) * T;
					Worst = std::max(Worst, ToCentre(Road, Px, Py) + 0.5 * Mark.WidthM);
				}
				if (Worst > Half)
				{
					if (OffRoad == 0)
					{
						First = std::string(Which) + ": a " + MarkKindName(Mark.Kind) + " on " + Road.Name +
							" reaches " + std::to_string(Worst) + " m from the centre of a " +
							std::to_string(Road.WidthM) + " m road";
					}
					++OffRoad;
				}
				// And on its own road's deck. Deliberately not RoadHeightAt, which returns the
				// LOWER deck where two roads cross -- that is what makes overpasses work -- so
				// comparing against it would flag every stripe on a bridge as floating. North
				// Carolina has 848 of those, all of them correct.
				double Along = 0.0;
				ToCentre(Road, Mark.Ax, Mark.Ay, &Along);
				// Over a short window, because projecting an edge line back onto the centreline does
				// not recover the distance the stripe was generated at: on a bend the nearest point
				// is a little further round, and on a graded bend that costs half a metre of height.
				// The question is whether the paint sits on the deck, not whether this test can
				// invert the offset exactly.
				double Nearest = 1e18;
				for (double Step = -30.0; Step <= 30.0; Step += 2.0)
				{
					const double At = std::clamp(Along + Step, 0.0, Road.LengthM());
					Nearest = std::min(Nearest, std::fabs(Mark.Az - Road.At(At)[2]));
				}
				if (Nearest > 0.45)
				{
					++Floating;
				}
			}
		}
		EXPECT_MSG(Total > 0, std::string(Which) + " painted nothing anywhere");
		EXPECT_MSG(OffRoad == 0, std::to_string(OffRoad) + " of " + std::to_string(Total) +
			" stripes are off the pavement -- " + First);
		EXPECT_MSG(Floating == 0, std::to_string(Floating) + " of " + std::to_string(Total) +
			" stripes float off the deck of the road they are painted on");
	}
}

// Yellow and white mean what they mean.
DTG_TEST(MarkingsFollowTheConvention)
{
	const GameState& State = dtg_test::LoadedState();
	bool bSawInterstate = false;
	bool bSawTwoLane = false;
	for (const HighwayPlan& Road : State.Map.Highways)
	{
		const std::vector<RoadMark> Marks = MarkHighway(Road);
		if (Marks.empty())
		{
			continue;
		}
		if (Road.bInterstate)
		{
			bSawInterstate = true;
			// A divided highway has no yellow centre: the yellow is on each carriageway's left edge,
			// and the lane lines between are white and broken.
			std::size_t Centre = 0;
			std::size_t YellowLane = 0;
			for (const RoadMark& Mark : Marks)
			{
				if (Mark.Kind == MarkKind::CentreDouble || Mark.Kind == MarkKind::CentreBroken)
				{
					++Centre;
				}
				if (Mark.Kind == MarkKind::LaneBroken && IsYellow(Mark))
				{
					++YellowLane;
				}
			}
			EXPECT_MSG(Centre == 0, Road.Name + " is an interstate with a yellow line down the middle");
			EXPECT_MSG(YellowLane == 0, Road.Name + " has yellow lane lines between same-way traffic");
			// It must still have both edges marked.
			const bool bYellowEdge = std::any_of(Marks.begin(), Marks.end(),
				[](const RoadMark& M) { return M.Kind == MarkKind::EdgeYellow; });
			const bool bWhiteEdge = std::any_of(Marks.begin(), Marks.end(),
				[](const RoadMark& M) { return M.Kind == MarkKind::EdgeWhite; });
			EXPECT_MSG(bYellowEdge, Road.Name + " has no yellow median edge");
			EXPECT_MSG(bWhiteEdge, Road.Name + " has no white outside edge");
		}
		else
		{
			bSawTwoLane = true;
			const bool bCentre = std::any_of(Marks.begin(), Marks.end(),
				[](const RoadMark& M) { return M.Kind == MarkKind::CentreDouble && IsYellow(M); });
			EXPECT_MSG(bCentre, Road.Name + " is undivided but has no yellow centre line");
		}
		// Whatever the road, every centre marking is yellow and every lane and edge-white is not.
		for (const RoadMark& Mark : Marks)
		{
			if (Mark.Kind == MarkKind::CentreDouble || Mark.Kind == MarkKind::CentreBroken ||
				Mark.Kind == MarkKind::EdgeYellow)
			{
				EXPECT_MSG(IsYellow(Mark), std::string("a ") + MarkKindName(Mark.Kind) + " on " +
					Road.Name + " is not yellow");
			}
			if (Mark.Kind == MarkKind::LaneBroken || Mark.Kind == MarkKind::EdgeWhite ||
				Mark.Kind == MarkKind::StopBar || Mark.Kind == MarkKind::Crosswalk)
			{
				EXPECT_MSG(!IsYellow(Mark), std::string("a ") + MarkKindName(Mark.Kind) + " on " +
					Road.Name + " is yellow");
			}
			EXPECT_MSG(Mark.WidthM > 0.05 && Mark.WidthM < 1.0,
				std::string("a ") + MarkKindName(Mark.Kind) + " is " + std::to_string(Mark.WidthM) + " m wide");
		}
	}
	EXPECT_MSG(bSawInterstate, "the state has no interstate to check");
	EXPECT_MSG(bSawTwoLane, "the state has no undivided highway to check");
}

// Broken lines are broken: the paint-to-gap ratio is the one drivers judge speed by.
DTG_TEST(MarkingsDashAtTheRightRate)
{
	const GameState& State = dtg_test::LoadedState();
	MarkingRules Rules;
	for (const HighwayPlan& Road : State.Map.Highways)
	{
		if (!Road.bInterstate || Road.LengthM() < 500.0)
		{
			continue;
		}
		double Paint = 0.0;
		double Span = 0.0;
		for (const RoadMark& Mark : MarkHighway(Road, Rules))
		{
			if (Mark.Kind != MarkKind::LaneBroken)
			{
				continue;
			}
			Paint += std::hypot(Mark.Bx - Mark.Ax, Mark.By - Mark.Ay);
		}
		Span = Road.LengthM();
		if (Paint <= 0.0)
		{
			continue;
		}
		// Each lane line covers DashM out of every DashM + GapM. There may be several lane lines,
		// so compare the ratio per line.
		const double Lines = std::round(Paint / (Span * Rules.DashM / (Rules.DashM + Rules.GapM)));
		EXPECT_MSG(Lines >= 1.0, Road.Name + " has lane paint but no whole number of lane lines");
		const double PerLine = Paint / Lines;
		const double Ratio = PerLine / Span;
		const double Want = Rules.DashM / (Rules.DashM + Rules.GapM);
		EXPECT_MSG(std::fabs(Ratio - Want) < 0.06, Road.Name + " paints " +
			std::to_string(Ratio * 100.0) + "% of each lane line, wanted about " +
			std::to_string(Want * 100.0) + "%");
		break;
	}
}

// Town streets get centre lines, and junctions get crossings.
DTG_TEST(MarkingsReachTheTownStreets)
{
	const GameState& State = dtg_test::LoadedState();
	struct Context { const StatePlan* Map; };
	Context Ctx{&State.Map};
	const auto Height = [](double X, double Y, const void* User) -> double
	{
		return static_cast<const Context*>(User)->Map->HeightAt(X, Y);
	};
	const std::vector<RoadMark> Marks = MarkStreets(State.City, Height, &Ctx);
	EXPECT_MSG(!Marks.empty(), "no town street has any paint on it");

	std::size_t Centres = 0;
	std::size_t Crossings = 0;
	std::size_t Bars = 0;
	for (const RoadMark& Mark : Marks)
	{
		switch (Mark.Kind)
		{
		case MarkKind::CentreBroken: ++Centres; break;
		case MarkKind::Crosswalk: ++Crossings; break;
		case MarkKind::StopBar: ++Bars; break;
		default: break;
		}
		EXPECT_MSG(std::isfinite(Mark.Az) && std::isfinite(Mark.Bz), "a street marking has no height");
	}
	EXPECT_MSG(Centres > 0, "no street has a centre line");
	EXPECT_MSG(Crossings > 0, "no junction has a crosswalk");
	EXPECT_MSG(Bars > 0, "no junction has a stop bar");
	// Crosswalks come in ladders, so there should be several rungs per stop bar.
	EXPECT_MSG(Crossings > Bars * 3, "crosswalks are not drawn as ladders (" +
		std::to_string(Crossings) + " rungs for " + std::to_string(Bars) + " stop bars)");
}
