#include "Sim/StreetFurniture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dtg::sim
{
	namespace detail_furniture
	{
		constexpr double HalfPi = 1.5707963267948966;
		constexpr double Pi = 3.141592653589793;

		/** Half-open, so a prop exactly on a chunk boundary belongs to one chunk, not two. */
		bool InArea(const Rect& Area, double X, double Y)
		{
			return X >= Area.MinX && X < Area.MaxX && Y >= Area.MinY && Y < Area.MaxY;
		}

		/** Stable per spot, 0..3. */
		int VariantOf(double X, double Y)
		{
			const auto Cx = static_cast<std::int64_t>(std::llround(X * 10.0));
			const auto Cy = static_cast<std::int64_t>(std::llround(Y * 10.0));
			std::uint64_t H = static_cast<std::uint64_t>(Cx) * 0x9E3779B97F4A7C15ull ^ static_cast<std::uint64_t>(Cy) * 0xC2B2AE3D27D4EB4Full;
			H ^= H >> 31;
			return static_cast<int>(H & 3u);
		}

		/** Distance to the nearest town street other than Skip: zero inside one. */
		double ToOtherStreet(const CityPlan& World, const RoadPlan* Skip, double X, double Y)
		{
			double Nearest = 1e18;
			for (const RoadPlan& Road : World.Roads)
			{
				if (&Road == Skip)
				{
					continue;
				}
				const double Dx = std::max({Road.Area.MinX - X, 0.0, X - Road.Area.MaxX});
				const double Dy = std::max({Road.Area.MinY - Y, 0.0, Y - Road.Area.MaxY});
				Nearest = std::min(Nearest, std::hypot(Dx, Dy));
			}
			return Nearest;
		}

		bool OnTownStreet(const CityPlan& World, double X, double Y)
		{
			for (const RoadPlan& Road : World.Roads)
			{
				if (Road.Area.Contains(X, Y))
				{
					return true;
				}
			}
			return false;
		}

		const Rect* BlockAt(const CityPlan& World, double X, double Y)
		{
			for (const Rect& Block : World.Blocks)
			{
				if (Block.Contains(X, Y))
				{
					return &Block;
				}
			}
			return nullptr;
		}

		bool IsPark(const CityPlan& World, const Rect& Block)
		{
			for (const Rect& Park : World.Parks)
			{
				if (Park.Overlaps(Block) && std::fabs(Park.CenterX() - Block.CenterX()) < 1.0 && std::fabs(Park.CenterY() - Block.CenterY()) < 1.0)
				{
					return true;
				}
			}
			return false;
		}

		/** The graded deck height Distance metres along the road, without walking the polyline again. */
		double DeckAt(const HighwayPlan& Road, double Distance, double Fallback)
		{
			if (Road.DeckM.empty() || Road.DeckStepM <= 0.0)
			{
				return Fallback;
			}
			const double Slot = std::clamp(Distance / Road.DeckStepM, 0.0, static_cast<double>(Road.DeckM.size() - 1));
			const std::size_t Low = static_cast<std::size_t>(Slot);
			const std::size_t High = std::min(Low + 1, Road.DeckM.size() - 1);
			return Road.DeckM[Low] + (Road.DeckM[High] - Road.DeckM[Low]) * (Slot - static_cast<double>(Low));
		}

		void Highways(const StatePlan& Map, const CityPlan& World, const Rect& Area, const FurnitureRules& Rules, std::vector<Prop>& Out)
		{
			for (const HighwayPlan& Road : World.Highways)
			{
				const double Half = 0.5 * Road.WidthM;
				const double Reach = Half + Rules.PoleSetbackM + 2.0;
				double Walked = 0.0;
				for (std::size_t Index = 0; Index + 1 < Road.Points.size() && Out.size() < Rules.Limit; ++Index)
				{
					const double Ax = Road.Points[Index][0];
					const double Ay = Road.Points[Index][1];
					const double Bx = Road.Points[Index + 1][0];
					const double By = Road.Points[Index + 1][1];
					const double Length = std::hypot(Bx - Ax, By - Ay);
					const double Start = Walked;
					Walked += Length;
					if (Length < 1e-9)
					{
						continue;
					}
					// Skip segments nowhere near the chunk: this loop sees every highway in the state.
					if (std::max(Ax, Bx) + Reach < Area.MinX || std::min(Ax, Bx) - Reach > Area.MaxX ||
						std::max(Ay, By) + Reach < Area.MinY || std::min(Ay, By) - Reach > Area.MaxY)
					{
						continue;
					}
					const double Dx = (Bx - Ax) / Length;
					const double Dy = (By - Ay) / Length;
					const double Nx = -Dy; // to the right of the direction of travel
					const double Ny = Dx;
					const double Heading = std::atan2(Dy, Dx);

					// Poles on the right-hand verge, on a grid of whole spacings along the road.
					for (double Along = std::ceil(Start / Rules.PoleSpacingM) * Rules.PoleSpacingM; Along < Walked; Along += Rules.PoleSpacingM)
					{
						const double T = Along - Start;
						const double X = Ax + Dx * T + Nx * (Half + Rules.PoleSetbackM);
						const double Y = Ay + Dy * T + Ny * (Half + Rules.PoleSetbackM);
						if (!InArea(Area, X, Y) || !Map.InsideState(X, Y) || World.CityAt(X, Y) != nullptr)
						{
							continue; // towns are lit by street lamps instead
						}
						const SurfaceInfo Info = Map.SurfaceAt(X, Y);
						// RoadNearness fades across the whole verge, so a pole on its own verge already reads as
						// "near a road"; bRoad is the carriageway and shoulder, which is what a pole must not be on.
						if (Info.bWater || Info.bRoad)
						{
							continue; // in a river, or on another road
						}
						Out.push_back({PropKind::UtilityPole, X, Y, Info.GroundM, Heading, VariantOf(X, Y)});
					}

					// Barriers along both edges wherever the deck stands well above the ground beneath.
					for (double Along = std::ceil(Start / Rules.BarrierLengthM) * Rules.BarrierLengthM; Along < Walked; Along += Rules.BarrierLengthM)
					{
						const double T = Along - Start;
						const double Cx = Ax + Dx * T;
						const double Cy = Ay + Dy * T;
						const double Deck = DeckAt(Road, Along, Map.HeightAt(Cx, Cy));
						if (Deck - Map.NaturalHeight(Cx, Cy) < Rules.ElevatedM)
						{
							continue;
						}
						for (const double Side : {-1.0, 1.0})
						{
							const double X = Cx + Nx * Side * (Half - 0.3);
							const double Y = Cy + Ny * Side * (Half - 0.3);
							if (InArea(Area, X, Y))
							{
								Out.push_back({PropKind::Barrier, X, Y, Deck, Heading, VariantOf(X, Y)});
							}
						}
					}
				}
			}
		}

		void TownStreets(const StatePlan& Map, const CityPlan& World, const Rect& Area, const FurnitureRules& Rules, std::vector<Prop>& Out)
		{
			const double Margin = Rules.LampSetbackM + 1.0;
			for (const RoadPlan& Road : World.Roads)
			{
				if (Out.size() >= Rules.Limit)
				{
					return;
				}
				const Rect& Street = Road.Area;
				if (Street.MaxX + Margin < Area.MinX || Street.MinX - Margin > Area.MaxX || Street.MaxY + Margin < Area.MinY ||
					Street.MinY - Margin > Area.MaxY)
				{
					continue;
				}
				// Lamps on a world grid along the street, alternating sides, each facing across it.
				const double From = Road.bHorizontal ? Street.MinX : Street.MinY;
				const double To = Road.bHorizontal ? Street.MaxX : Street.MaxY;
				for (double Along = std::ceil(From / Rules.LampSpacingM) * Rules.LampSpacingM; Along < To; Along += Rules.LampSpacingM)
				{
					const bool bFar = static_cast<long long>(std::llround(Along / Rules.LampSpacingM)) % 2 != 0;
					const double Across = bFar ? (Road.bHorizontal ? Street.MaxY : Street.MaxX) + Rules.LampSetbackM
											   : (Road.bHorizontal ? Street.MinY : Street.MinX) - Rules.LampSetbackM;
					const double X = Road.bHorizontal ? Along : Across;
					const double Y = Road.bHorizontal ? Across : Along;
					// On a block's sidewalk: not out in a cross street at a junction.
					if (!InArea(Area, X, Y) || OnTownStreet(World, X, Y) || BlockAt(World, X, Y) == nullptr)
					{
						continue;
					}
					// Nor crowding a junction: a lamp at a block corner stood 0.35 m off the cross street.
					if (ToOtherStreet(World, &Road, X, Y) < Rules.LampSetbackM + 1.5)
					{
						continue;
					}
					const double Facing = Road.bHorizontal ? (bFar ? -HalfPi : HalfPi) : (bFar ? Pi : 0.0);
					Out.push_back({PropKind::StreetLamp, X, Y, Map.HeightAt(X, Y) + Rules.KerbM, Facing, VariantOf(X, Y)});
				}
			}
		}

		void Hydrants(const StatePlan& Map, const CityPlan& World, const Rect& Area, const FurnitureRules& Rules, std::vector<Prop>& Out)
		{
			for (const Rect& Block : World.Blocks)
			{
				if (Out.size() >= Rules.Limit)
				{
					return;
				}
				if (!Block.Overlaps(Area) || IsPark(World, Block))
				{
					continue;
				}
				// One corner, chosen by the block itself, just in from the kerb.
				const int Corner = VariantOf(Block.CenterX(), Block.CenterY());
				const double Inset = Rules.LampSetbackM;
				const double X = (Corner & 1) ? Block.MaxX - Inset : Block.MinX + Inset;
				const double Y = (Corner & 2) ? Block.MaxY - Inset : Block.MinY + Inset;
				if (InArea(Area, X, Y))
				{
					Out.push_back({PropKind::Hydrant, X, Y, Map.HeightAt(X, Y) + Rules.KerbM, 0.0, Corner});
				}
			}
		}
	}

	const char* PropKindName(PropKind Kind)
	{
		switch (Kind)
		{
		case PropKind::UtilityPole: return "UtilityPole";
		case PropKind::StreetLamp: return "StreetLamp";
		case PropKind::Hydrant: return "Hydrant";
		case PropKind::Barrier: return "Barrier";
		case PropKind::Count: break;
		}
		return "Unknown";
	}

	std::vector<Prop> FurnitureIn(const StatePlan& Map, const CityPlan& World, const Rect& Area, const FurnitureRules& Rules)
	{
		std::vector<Prop> Out;
		detail_furniture::Highways(Map, World, Area, Rules, Out);
		detail_furniture::TownStreets(Map, World, Area, Rules, Out);
		detail_furniture::Hydrants(Map, World, Area, Rules, Out);
		if (Out.size() > Rules.Limit)
		{
			Out.resize(Rules.Limit);
		}
		return Out;
	}
}
