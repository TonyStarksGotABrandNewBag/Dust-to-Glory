#include "Sim/Pavement.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace dtg::sim
{
	namespace detail_pavement
	{
		using Point = std::array<double, 2>;

		/** The widest gap between two blocks that is a street to cross; anything wider is open country. */
		constexpr double MaxCrossingM = 30.0;
		/** No town is this many blocks across. A bound on the walk, not a limit any route reaches. */
		constexpr int MaxBlocks = 256;
		constexpr double Epsilon = 1e-6;

		/** Directions across a street: 0 east, 1 north, 2 west, 3 south. The ring corners on the side of
		 * a block facing each, south or west one first. */
		constexpr std::array<std::array<std::size_t, 2>, 4> Facing = {{{1, 2}, {3, 2}, {0, 3}, {0, 1}}};
		/** The same corner of the block across the street: mirrored east-west, and north-south. */
		constexpr std::array<std::size_t, 4> MirrorX = {1, 0, 3, 2};
		constexpr std::array<std::size_t, 4> MirrorY = {3, 2, 1, 0};

		double Distance(const Point& A, const Point& B)
		{
			return std::hypot(B[0] - A[0], B[1] - A[1]);
		}

		/** Distance from a point to a rectangle: zero inside it. */
		double ToRect(const Rect& Area, double X, double Y)
		{
			const double Dx = std::max({Area.MinX - X, 0.0, X - Area.MaxX});
			const double Dy = std::max({Area.MinY - Y, 0.0, Y - Area.MaxY});
			return std::hypot(Dx, Dy);
		}

		/** The block a point is on, or the nearest within a street's width of it; -1 when there is none. */
		int BlockAt(const std::vector<Rect>& Blocks, double X, double Y)
		{
			int Best = -1;
			double Nearest = MaxCrossingM;
			for (std::size_t Index = 0; Index < Blocks.size(); ++Index)
			{
				const double Gap = ToRect(Blocks[Index], X, Y);
				if (Gap < Nearest)
				{
					Nearest = Gap;
					Best = static_cast<int>(Index);
				}
			}
			return Best;
		}

		/** The next block over the street in a direction, or -1. */
		int Neighbour(const std::vector<Rect>& Blocks, const Rect& From, int Direction)
		{
			const bool bAcrossX = Direction == 0 || Direction == 2;
			int Best = -1;
			double Nearest = MaxCrossingM;
			for (std::size_t Index = 0; Index < Blocks.size(); ++Index)
			{
				const Rect& Other = Blocks[Index];
				// Facing each other across the street, rather than diagonally across a junction.
				const bool bInLine = bAcrossX ? Other.MinY < From.MaxY && Other.MaxY > From.MinY
											  : Other.MinX < From.MaxX && Other.MaxX > From.MinX;
				const double Gap = Direction == 0 ? Other.MinX - From.MaxX
					: Direction == 1             ? Other.MinY - From.MaxY
					: Direction == 2             ? From.MinX - Other.MaxX
												 : From.MinY - Other.MaxY;
				if (bInLine && Gap >= 0.0 && Gap < Nearest)
				{
					Nearest = Gap;
					Best = static_cast<int>(Index);
				}
			}
			return Best;
		}

		/** A ring with the distance round it, anticlockwise from corner 0, to each corner; At[4] is once round. */
		struct Lap
		{
			std::array<Point, 4> Corners;
			std::array<double, 5> At = {};
		};

		Lap LapOf(const std::array<Point, 4>& Corners)
		{
			Lap Out;
			Out.Corners = Corners;
			for (std::size_t Side = 0; Side < 4; ++Side)
			{
				Out.At[Side + 1] = Out.At[Side] + Distance(Corners[Side], Corners[(Side + 1) % 4]);
			}
			return Out;
		}

		/** How far round the ring the point on it nearest a spot lies. */
		double Project(const Lap& Ring, double X, double Y)
		{
			double Best = 0.0;
			double Nearest = 1e300;
			for (std::size_t Side = 0; Side < 4; ++Side)
			{
				const Point& A = Ring.Corners[Side];
				const Point& B = Ring.Corners[(Side + 1) % 4];
				const double Length = Ring.At[Side + 1] - Ring.At[Side];
				const double T = Length > Epsilon
					? std::clamp(((X - A[0]) * (B[0] - A[0]) + (Y - A[1]) * (B[1] - A[1])) / (Length * Length), 0.0, 1.0)
					: 0.0;
				const double Off = std::hypot(X - (A[0] + (B[0] - A[0]) * T), Y - (A[1] + (B[1] - A[1]) * T));
				if (Off < Nearest)
				{
					Nearest = Off;
					Best = Ring.At[Side] + T * Length;
				}
			}
			return Best;
		}

		Point PointAt(const Lap& Ring, double Along)
		{
			for (std::size_t Side = 0; Side < 4; ++Side)
			{
				const double Length = Ring.At[Side + 1] - Ring.At[Side];
				if (Along <= Ring.At[Side + 1] || Side == 3)
				{
					const double T = Length > Epsilon ? std::clamp((Along - Ring.At[Side]) / Length, 0.0, 1.0) : 0.0;
					const Point& A = Ring.Corners[Side];
					const Point& B = Ring.Corners[(Side + 1) % 4];
					return Point{Lerp(A[0], B[0], T), Lerp(A[1], B[1], T)};
				}
			}
			return Ring.Corners[0];
		}

		void Add(std::vector<Point>& Route, const Point& Next)
		{
			if (Route.empty() || Distance(Route.back(), Next) > Epsilon)
			{
				Route.push_back(Next);
			}
		}

		/** Round the ring the shorter way from one distance to another: every corner passed, then the end. */
		void WalkRound(const Lap& Ring, double From, double To, std::vector<Point>& Route)
		{
			const double Round = Ring.At[4];
			const double Ahead = std::fmod(To - From + 2.0 * Round, Round);
			const bool bAnticlockwise = Ahead <= Round - Ahead;
			const double Span = bAnticlockwise ? Ahead : Round - Ahead;
			std::vector<std::pair<double, std::size_t>> Passed;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const double Offset = bAnticlockwise ? std::fmod(Ring.At[Corner] - From + 2.0 * Round, Round)
													 : std::fmod(From - Ring.At[Corner] + 2.0 * Round, Round);
				if (Offset > Epsilon && Offset < Span - Epsilon)
				{
					Passed.emplace_back(Offset, Corner);
				}
			}
			std::sort(Passed.begin(), Passed.end());
			for (const std::pair<double, std::size_t>& Corner : Passed)
			{
				Add(Route, Ring.Corners[Corner.second]);
			}
			Add(Route, PointAt(Ring, To));
		}
	}

	std::array<std::array<double, 2>, 4> PavementRing(const Rect& Block, const FurnitureRules& Rules)
	{
		const double In = 0.5 * Rules.SidewalkM;
		return {{{Block.MinX + In, Block.MinY + In}, {Block.MaxX - In, Block.MinY + In}, {Block.MaxX - In, Block.MaxY - In},
			{Block.MinX + In, Block.MaxY - In}}};
	}

	std::vector<std::array<double, 2>> PavementRoute(const CityPlan& World, double FromX, double FromY, double ToX, double ToY,
		const FurnitureRules& Rules)
	{
		using namespace detail_pavement;
		const std::vector<Rect>& Blocks = World.Blocks;
		int Current = BlockAt(Blocks, FromX, FromY);
		const int Goal = BlockAt(Blocks, ToX, ToY);
		if (Current < 0 || Goal < 0)
		{
			return {};
		}
		const Point To{ToX, ToY};
		std::vector<Point> Route;
		Lap Ring = LapOf(PavementRing(Blocks[static_cast<std::size_t>(Current)], Rules));
		double Along = Project(Ring, FromX, FromY);
		Add(Route, PointAt(Ring, Along));

		int Last = -1;
		for (int Step = 0; Current != Goal; ++Step)
		{
			if (Step >= MaxBlocks)
			{
				return {};
			}
			const Rect& Here = Blocks[static_cast<std::size_t>(Current)];
			const Rect& There = Blocks[static_cast<std::size_t>(Goal)];
			// Which way the goal's block lies on each axis, if it does: -1 when already in line.
			const int AlongX = There.MinX >= Here.MaxX ? 0 : (There.MaxX <= Here.MinX ? 2 : -1);
			const int AlongY = There.MinY >= Here.MaxY ? 1 : (There.MaxY <= Here.MinY ? 3 : -1);
			// Keep going the way they were while it still leads there, so the route is an L rather than a
			// staircase; otherwise the axis with further to go first.
			std::array<int, 2> Tries = std::fabs(There.CenterX() - Here.CenterX()) >= std::fabs(There.CenterY() - Here.CenterY())
				? std::array<int, 2>{AlongX, AlongY}
				: std::array<int, 2>{AlongY, AlongX};
			if (Last >= 0 && Tries[1] == Last)
			{
				std::swap(Tries[0], Tries[1]);
			}
			int Direction = -1;
			int Next = -1;
			for (const int Try : Tries)
			{
				if (Try >= 0 && Next < 0)
				{
					Next = Neighbour(Blocks, Here, Try);
					Direction = Next >= 0 ? Try : -1;
				}
			}
			if (Next < 0)
			{
				return {}; // the blocks run out before the goal: it is in another town
			}
			// Round to whichever corner of the side facing that way is nearer the goal, and straight over.
			const std::array<std::size_t, 2>& Pair = Facing[static_cast<std::size_t>(Direction)];
			const std::size_t Exit = Distance(Ring.Corners[Pair[0]], To) <= Distance(Ring.Corners[Pair[1]], To) ? Pair[0] : Pair[1];
			WalkRound(Ring, Along, Ring.At[Exit], Route);
			const std::size_t Entry = Direction % 2 == 0 ? MirrorX[Exit] : MirrorY[Exit];
			Ring = LapOf(PavementRing(Blocks[static_cast<std::size_t>(Next)], Rules));
			Along = Ring.At[Entry];
			Add(Route, Ring.Corners[Entry]);
			Current = Next;
			Last = Direction;
		}
		// Round the goal's block to the pavement in front of it, and up the path.
		WalkRound(Ring, Along, Project(Ring, ToX, ToY), Route);
		Add(Route, To);
		return Route;
	}
}
