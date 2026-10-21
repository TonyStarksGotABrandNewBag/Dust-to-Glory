#include "Sim/RoadMarkings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace dtg::sim
{
	namespace detail_markings
	{
		// Highway paint, linear. Road paint is never pure white or pure yellow: it is chalky, and it
		// is what the headlights pick up at night.
		constexpr double WhiteR = 0.54;
		constexpr double WhiteG = 0.53;
		constexpr double WhiteB = 0.48;
		constexpr double YellowR = 0.52;
		constexpr double YellowG = 0.40;
		constexpr double YellowB = 0.08;

		void SetWhite(RoadMark& Mark)
		{
			Mark.Rgb[0] = WhiteR;
			Mark.Rgb[1] = WhiteG;
			Mark.Rgb[2] = WhiteB;
		}

		void SetYellow(RoadMark& Mark)
		{
			Mark.Rgb[0] = YellowR;
			Mark.Rgb[1] = YellowG;
			Mark.Rgb[2] = YellowB;
		}

		/**
		 * A stripe running along the road, Offset metres to the right of the centreline, between two
		 * distances along it. Follows the deck, so paint stays on a bridge rather than sailing off it.
		 */
		/**
		 * Where a stripe is allowed to be one straight quad, and where it has to break. A road is a
		 * polyline, so in plan it is exactly straight between its own vertices; the only other thing
		 * that bends a stripe is the deck climbing or dropping under it. Those two lists are known
		 * up front, which is far cheaper than probing along the road looking for curvature -- that
		 * cost more time than the quads it saved.
		 */
		std::vector<double> BreaksFor(const HighwayPlan& Road)
		{
			std::vector<double> Breaks;
			const double Length = Road.LengthM();
			Breaks.push_back(0.0);
			double Walked = 0.0;
			for (std::size_t Part = 0; Part + 1 < Road.Points.size(); ++Part)
			{
				Walked += std::hypot(Road.Points[Part + 1][0] - Road.Points[Part][0],
					Road.Points[Part + 1][1] - Road.Points[Part][1]);
				if (Walked > 0.0 && Walked < Length)
				{
					Breaks.push_back(Walked);
				}
			}
			// And wherever the road's own profile changes slope by enough to show.
			const double Step = std::max(0.5, Road.DeckStepM);
			for (std::size_t Index = 1; Index + 1 < Road.DeckM.size(); ++Index)
			{
				const double Before = (Road.DeckM[Index] - Road.DeckM[Index - 1]) / Step;
				const double After = (Road.DeckM[Index + 1] - Road.DeckM[Index]) / Step;
				if (std::fabs(After - Before) > 0.004)
				{
					Breaks.push_back(static_cast<double>(Index) * Step);
				}
			}
			Breaks.push_back(Length);
			std::sort(Breaks.begin(), Breaks.end());
			Breaks.erase(std::unique(Breaks.begin(), Breaks.end()), Breaks.end());
			return Breaks;
		}

		/**
		 * A stripe running along the road, Offset metres to the right of the centreline, between two
		 * distances along it. Follows the deck, so paint stays on a bridge rather than sailing off it.
		 */
		void AddAlong(std::vector<RoadMark>& Out, const HighwayPlan& Road, const std::vector<double>& Breaks,
			double From, double To, double Offset, double Width, MarkKind Kind, bool bYellow, double Lift)
		{
			const double Length = Road.LengthM();
			From = std::max(0.0, From);
			To = std::min(Length, To);
			if (To - From < 0.01)
			{
				return;
			}
			auto Edge = [&Road, Length, Offset, Lift](double At, double& Ex, double& Ey, double& Ez)
			{
				const std::array<double, 3> P0 = Road.At(std::max(0.0, At - 0.5));
				const std::array<double, 3> P1 = Road.At(std::min(Length, At + 0.5));
				const double Dx = P1[0] - P0[0];
				const double Dy = P1[1] - P0[1];
				const double Len = std::max(1e-9, std::sqrt(Dx * Dx + Dy * Dy));
				const std::array<double, 3> P = Road.At(At);
				Ex = P[0] + (-Dy / Len) * Offset;
				Ey = P[1] + (Dx / Len) * Offset;
				Ez = P[2] + Lift;
			};

			// Walk the breaks that fall inside [From, To], emitting one quad between each pair.
			double PieceFrom = From;
			double Ax = 0.0;
			double Ay = 0.0;
			double Az = 0.0;
			Edge(PieceFrom, Ax, Ay, Az);
			std::size_t Next = 0;
			while (Next < Breaks.size() && Breaks[Next] <= From + 1e-6)
			{
				++Next;
			}
			// However straight the road, a stripe is cut every so often. One quad running the length
			// of an interstate would be correct and would also never be culled by the renderer, and
			// a triangle that large costs depth precision for everything drawn against it.
			constexpr double MaxPieceM = 120.0;
			while (PieceFrom < To - 1e-6)
			{
				double PieceTo = (Next < Breaks.size() && Breaks[Next] < To) ? Breaks[Next] : To;
				if (PieceTo - PieceFrom > MaxPieceM)
				{
					PieceTo = PieceFrom + MaxPieceM;
				}
				else
				{
					++Next;
				}
				if (PieceTo - PieceFrom < 1e-6)
				{
					continue;
				}
				double Bx = 0.0;
				double By = 0.0;
				double Bz = 0.0;
				Edge(PieceTo, Bx, By, Bz);
				RoadMark Mark;
				Mark.Ax = Ax;
				Mark.Ay = Ay;
				Mark.Az = Az;
				Mark.Bx = Bx;
				Mark.By = By;
				Mark.Bz = Bz;
				Mark.WidthM = Width;
				Mark.Kind = Kind;
				if (bYellow)
				{
					SetYellow(Mark);
				}
				else
				{
					SetWhite(Mark);
				}
				Out.push_back(Mark);
				PieceFrom = PieceTo;
				Ax = Bx;
				Ay = By;
				Az = Bz;
			}
		}

		/** The same, broken into dashes. */
		void AddBroken(std::vector<RoadMark>& Out, const HighwayPlan& Road, const std::vector<double>& Breaks,
			double Offset, double Width, MarkKind Kind, bool bYellow, const MarkingRules& Rules)
		{
			const double Length = Road.LengthM();
			const double Period = std::max(0.5, Rules.DashM + Rules.GapM);
			for (double At = 0.0; At < Length; At += Period)
			{
				AddAlong(Out, Road, Breaks, At, std::min(Length, At + Rules.DashM), Offset, Width, Kind,
					bYellow, Rules.LiftM);
				if (Out.size() >= Rules.Limit)
				{
					return;
				}
			}
		}
	}

	const char* MarkKindName(MarkKind Kind)
	{
		switch (Kind)
		{
		case MarkKind::CentreDouble: return "double yellow";
		case MarkKind::CentreBroken: return "broken yellow";
		case MarkKind::LaneBroken: return "lane line";
		case MarkKind::EdgeWhite: return "white edge";
		case MarkKind::EdgeYellow: return "yellow edge";
		case MarkKind::StopBar: return "stop bar";
		case MarkKind::Crosswalk: return "crosswalk";
		case MarkKind::Count: break;
		}
		return "marking";
	}

	std::vector<RoadMark> MarkHighway(const HighwayPlan& Road, const MarkingRules& Rules)
	{
		using namespace detail_markings;
		std::vector<RoadMark> Out;
		if (Road.Points.size() < 2 || Road.LengthM() < 1.0 || Road.WidthM < 2.0)
		{
			return Out;
		}
		const double Half = 0.5 * Road.WidthM;
		const double Edge = Half - Rules.EdgeInsetM;
		const double Line = Rules.LineWidthM;
		const std::vector<double> Breaks = BreaksFor(Road);

		if (Road.bInterstate)
		{
			// Divided: two carriageways either side of a median, each with its own paint. The median
			// is the middle fifth of the right of way, which is where the barrier or grass would be.
			const double MedianHalf = Half * 0.12;
			for (const double Side : {-1.0, 1.0})
			{
				const double Inner = Side * MedianHalf;
				const double Outer = Side * Edge;
				// Yellow on the left (median) side, white on the right.
				AddAlong(Out, Road, Breaks, 0.0, Road.LengthM(), Inner, Line, MarkKind::EdgeYellow, true, Rules.LiftM);
				AddAlong(Out, Road, Breaks, 0.0, Road.LengthM(), Outer, Line, MarkKind::EdgeWhite, false, Rules.LiftM);
				// Lane lines between: how many depends on how wide this carriageway is.
				const double Width = std::fabs(Outer - Inner);
				const int Lanes = std::max(1, static_cast<int>(std::floor(Width / Rules.LaneWidthM + 0.35)));
				for (int Lane = 1; Lane < Lanes; ++Lane)
				{
					const double At = Inner + (Outer - Inner) * Lane / Lanes;
					AddBroken(Out, Road, Breaks, At, Line, MarkKind::LaneBroken, false, Rules);
				}
			}
			return Out;
		}

		// Undivided. A double yellow down the middle, white edges, and lane lines if it is wide
		// enough for more than one lane each way.
		AddAlong(Out, Road, Breaks, 0.0, Road.LengthM(), -0.5 * Rules.LineWidthM - 0.05, Line,
			MarkKind::CentreDouble, true, Rules.LiftM);
		AddAlong(Out, Road, Breaks, 0.0, Road.LengthM(), 0.5 * Rules.LineWidthM + 0.05, Line,
			MarkKind::CentreDouble, true, Rules.LiftM);
		AddAlong(Out, Road, Breaks, 0.0, Road.LengthM(), -Edge, Line, MarkKind::EdgeWhite, false, Rules.LiftM);
		AddAlong(Out, Road, Breaks, 0.0, Road.LengthM(), Edge, Line, MarkKind::EdgeWhite, false, Rules.LiftM);
		const int PerSide = static_cast<int>(std::floor(Edge / Rules.LaneWidthM + 0.25));
		for (int Lane = 1; Lane < PerSide; ++Lane)
		{
			const double At = Edge * Lane / PerSide;
			AddBroken(Out, Road, Breaks, At, Line, MarkKind::LaneBroken, false, Rules);
			AddBroken(Out, Road, Breaks, -At, Line, MarkKind::LaneBroken, false, Rules);
		}
		return Out;
	}

	std::vector<RoadMark> MarkStreets(const CityPlan& World, double (*HeightAt)(double, double, const void*),
		const void* User, const MarkingRules& Rules)
	{
		using namespace detail_markings;
		std::vector<RoadMark> Out;
		if (HeightAt == nullptr)
		{
			return Out;
		}
		for (const RoadPlan& Road : World.Roads)
		{
			if (Out.size() >= Rules.Limit)
			{
				return Out;
			}
			const double Width = Road.bHorizontal ? Road.Area.Depth() : Road.Area.Width();
			const double Length = Road.bHorizontal ? Road.Area.Width() : Road.Area.Depth();
			if (Width < 6.0 || Length < 12.0)
			{
				continue;
			}
			const double Mid = Road.bHorizontal ? Road.Area.CenterY() : Road.Area.CenterX();
			const double From = Road.bHorizontal ? Road.Area.MinX : Road.Area.MinY;
			const double To = Road.bHorizontal ? Road.Area.MaxX : Road.Area.MaxY;
			// A crossroads community's street is a county road: a solid double yellow centre line and a
			// white line along each edge, the way SC-905 is painted through Longs.
			const CityInfo* const Town = World.CityAt(Road.Area.CenterX(), Road.Area.CenterY());
			if (Town != nullptr && Town->Style == CityStyle::Rural)
			{
				auto Solid = [&](double Across, MarkKind Kind, bool bYellow)
				{
					RoadMark Mark;
					Mark.Ax = Road.bHorizontal ? From + 1.0 : Across;
					Mark.Ay = Road.bHorizontal ? Across : From + 1.0;
					Mark.Bx = Road.bHorizontal ? To - 1.0 : Across;
					Mark.By = Road.bHorizontal ? Across : To - 1.0;
					Mark.Az = HeightAt(Mark.Ax, Mark.Ay, User) + Rules.LiftM;
					Mark.Bz = HeightAt(Mark.Bx, Mark.By, User) + Rules.LiftM;
					Mark.WidthM = Rules.LineWidthM;
					Mark.Kind = Kind;
					if (bYellow)
					{
						SetYellow(Mark);
					}
					else
					{
						SetWhite(Mark);
					}
					Out.push_back(Mark);
				};
				Solid(Mid - Rules.LineWidthM, MarkKind::CentreDouble, true);
				Solid(Mid + Rules.LineWidthM, MarkKind::CentreDouble, true);
				Solid(Mid - 0.5 * Width + Rules.EdgeInsetM, MarkKind::EdgeWhite, false);
				Solid(Mid + 0.5 * Width - Rules.EdgeInsetM, MarkKind::EdgeWhite, false);
				continue;
			}
			// A broken yellow centre line: town streets are two-way and passing is academic at
			// 25 mph, so the convention is a single broken line rather than a double.
			const double Period = Rules.DashM + Rules.GapM;
			for (double At = From + 2.0; At + Rules.DashM < To; At += Period)
			{
				RoadMark Mark;
				const double Bx = At + Rules.DashM;
				Mark.Ax = Road.bHorizontal ? At : Mid;
				Mark.Ay = Road.bHorizontal ? Mid : At;
				Mark.Bx = Road.bHorizontal ? Bx : Mid;
				Mark.By = Road.bHorizontal ? Mid : Bx;
				Mark.Az = HeightAt(Mark.Ax, Mark.Ay, User) + Rules.LiftM;
				Mark.Bz = HeightAt(Mark.Bx, Mark.By, User) + Rules.LiftM;
				Mark.WidthM = Rules.LineWidthM;
				Mark.Kind = MarkKind::CentreBroken;
				SetYellow(Mark);
				Out.push_back(Mark);
				if (Out.size() >= Rules.Limit)
				{
					return Out;
				}
			}
		}

		// Crosswalks: a ladder across each arm of a junction, set back from the middle.
		for (const Intersection& Node : World.Intersections)
		{
			if (Out.size() + 40 >= Rules.Limit)
			{
				return Out;
			}
			for (const int Link : Node.Links)
			{
				const Intersection* Other = World.FindIntersection(Link);
				if (Other == nullptr || Other->Id < Node.Id)
				{
					continue; // each pair once
				}
				const double Dx = Other->X - Node.X;
				const double Dy = Other->Y - Node.Y;
				const double Len = std::sqrt(Dx * Dx + Dy * Dy);
				if (Len < 24.0)
				{
					continue;
				}
				const double Ux = Dx / Len;
				const double Uy = Dy / Len;
				const double Nx = -Uy;
				const double Ny = Ux;
				const double Setback = 11.0;
				const double HalfWalk = 7.0;
				const double Cx = Node.X + Ux * Setback;
				const double Cy = Node.Y + Uy * Setback;
				// The rungs run along the road, spread across it.
				for (int Rung = 0; Rung < 6; ++Rung)
				{
					const double Across = -HalfWalk + 2.0 * HalfWalk * Rung / 5.0;
					RoadMark Mark;
					Mark.Ax = Cx + Nx * Across - Ux * 1.1;
					Mark.Ay = Cy + Ny * Across - Uy * 1.1;
					Mark.Bx = Cx + Nx * Across + Ux * 1.1;
					Mark.By = Cy + Ny * Across + Uy * 1.1;
					Mark.Az = HeightAt(Mark.Ax, Mark.Ay, User) + Rules.LiftM;
					Mark.Bz = HeightAt(Mark.Bx, Mark.By, User) + Rules.LiftM;
					Mark.WidthM = 0.45;
					Mark.Kind = MarkKind::Crosswalk;
					SetWhite(Mark);
					Out.push_back(Mark);
				}
				// And a stop bar just behind the crossing.
				RoadMark Bar;
				const double Bar0 = Setback + 2.6;
				Bar.Ax = Node.X + Ux * Bar0 + Nx * 0.3;
				Bar.Ay = Node.Y + Uy * Bar0 + Ny * 0.3;
				Bar.Bx = Node.X + Ux * Bar0 + Nx * HalfWalk;
				Bar.By = Node.Y + Uy * Bar0 + Ny * HalfWalk;
				Bar.Az = HeightAt(Bar.Ax, Bar.Ay, User) + Rules.LiftM;
				Bar.Bz = HeightAt(Bar.Bx, Bar.By, User) + Rules.LiftM;
				Bar.WidthM = 0.5;
				Bar.Kind = MarkKind::StopBar;
				SetWhite(Bar);
				Out.push_back(Bar);
			}
		}
		return Out;
	}
}
