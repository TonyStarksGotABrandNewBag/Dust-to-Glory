#include "Sim/Vegetation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dtg::sim
{
	namespace detail_vegetation
	{
		constexpr double Pi = 3.14159265358979;

		/**
		 * A hash of a cell's integer coordinates. Everything random about a plant comes from this, so
		 * it depends only on where the plant stands -- not on which chunk asked, nor on the order the
		 * cells were visited. That is what keeps a forest still while the player drives through it.
		 */
		std::uint64_t CellHash(std::int64_t Cx, std::int64_t Cy, std::uint64_t Salt)
		{
			std::uint64_t Key = static_cast<std::uint64_t>(Cx + 0x4000000) * 0x9E3779B97F4A7C15ULL;
			Key ^= static_cast<std::uint64_t>(Cy + 0x4000000) * 0xC2B2AE3D27D4EB4FULL;
			Key ^= Salt * 0x165667B19E3779F9ULL;
			Key ^= Key >> 33;
			Key *= 0xFF51AFD7ED558CCDULL;
			Key ^= Key >> 29;
			Key *= 0xC4CEB9FE1A85EC53ULL;
			Key ^= Key >> 32;
			return Key;
		}

		/** A hashed value in [0, 1). Stream picks an independent one for the same cell. */
		double CellValue(std::int64_t Cx, std::int64_t Cy, std::uint32_t Stream)
		{
			const std::uint64_t Hashed = CellHash(Cx, Cy, 0x517CC1B727220A95ULL + Stream * 0x2545F4914F6CDD1DULL);
			return static_cast<double>(Hashed >> 11) / static_cast<double>(1ULL << 53);
		}
	}

	const char* PlantKindName(PlantKind Kind)
	{
		switch (Kind)
		{
		case PlantKind::Pine: return "pine";
		case PlantKind::Oak: return "oak";
		case PlantKind::Palm: return "palm";
		case PlantKind::Palmetto: return "palmetto";
		case PlantKind::Cypress: return "cypress";
		case PlantKind::Scrub: return "scrub";
		case PlantKind::Count: break;
		}
		return "plant";
	}

	/**
	 * Whether a spot inside a town is a yard that grows: in a crossroads community (Rural style) the
	 * blocks are wooded lots with a house in a clearing, so anything off the street, in from the pavement
	 * and a stride from the buildings grows. Every other style keeps its lots clear.
	 */
	bool YardAt(const CityPlan& World, const CityInfo& Town, double X, double Y)
	{
		if (Town.Style != CityStyle::Rural)
		{
			return false;
		}
		constexpr double Pavement = 5.0;   // the shoulder and the ditch (Sim/Lots), and a little
		constexpr double Clearing = 3.0;   // round each building
		bool bInABlock = false;
		for (const Rect& Block : World.Blocks)
		{
			if (X > Block.MinX + Pavement && X < Block.MaxX - Pavement && Y > Block.MinY + Pavement && Y < Block.MaxY - Pavement)
			{
				bInABlock = true;
				break;
			}
		}
		if (!bInABlock)
		{
			return false;
		}
		for (const BuildingPlan& Building : World.Buildings)
		{
			if (Building.Id < Town.FirstBuildingId || Building.Id > Town.LastBuildingId)
			{
				continue;
			}
			const Rect& Lot = Building.Footprint;
			if (X > Lot.MinX - Clearing && X < Lot.MaxX + Clearing && Y > Lot.MinY - Clearing && Y < Lot.MaxY + Clearing)
			{
				return false;
			}
		}
		return true;
	}

	/** Whether a spot lies within Reach of a crossroads community's edge, outside it. */
	bool NearCrossroads(const CityPlan& World, double X, double Y, double Reach)
	{
		for (const CityInfo& Town : World.Cities)
		{
			if (Town.Style != CityStyle::Rural)
			{
				continue;
			}
			const Rect& B = Town.Bounds;
			if (X > B.MinX - Reach && X < B.MaxX + Reach && Y > B.MinY - Reach && Y < B.MaxY + Reach)
			{
				return true;
			}
		}
		return false;
	}

	bool CanPlantAt(const StatePlan& Map, const CityPlan& World, double X, double Y,
		const PlantingRules& Rules, SurfaceInfo* OutSurface)
	{
		if (!Map.InsideState(X, Y))
		{
			return false;
		}
		const SurfaceInfo Info = Map.SurfaceAt(X, Y);
		if (OutSurface)
		{
			*OutSurface = Info;
		}
		// Nothing grows in open water, and nothing grows on a road.
		if (Info.bWater || Info.RoadNearness > 0.0)
		{
			return false;
		}
		// Clear of the carriageway by a verge's width. RoadNearness has already faded to nothing by
		// the edge of the fade, so this second check is what actually holds the shoulder clear.
		if (Rules.RoadClearM > 0.0)
		{
			const double Probe = Rules.RoadClearM;
			const double Offsets[4][2] = {{Probe, 0.0}, {-Probe, 0.0}, {0.0, Probe}, {0.0, -Probe}};
			for (const auto& Offset : Offsets)
			{
				if (Map.SurfaceAt(X + Offset[0], Y + Offset[1]).RoadNearness > 0.5)
				{
					return false;
				}
			}
		}
		if (Rules.bAvoidTowns)
		{
			// Inside a town, only the parks are planted: a street or a lot is not.
			const CityInfo* Town = World.CityAt(X, Y);
			if (Town != nullptr)
			{
				bool bPark = false;
				for (const Rect& Park : World.Parks)
				{
					if (Park.Contains(X, Y))
					{
						bPark = true;
						break;
					}
				}
				if (!bPark && !YardAt(World, *Town, X, Y))
				{
					return false;
				}
			}
		}
		return true;
	}

	namespace detail_vegetation
	{
		/**
		 * Shrubs and ferns under the trees: wax myrtle, yaupon and fern in the pine woods, on a grid finer than
		 * the canopy's and only where the canopy is dense, so the woods have a floor and the fields do not.
		 */
		void Understory(const StatePlan& Map, const CityPlan& World, const Rect& Area, const PlantingRules& Rules, std::vector<Plant>& Out)
		{
			if (Rules.UnderstorySpacingM <= 0.0)
			{
				return;
			}
			const double Spacing = std::max(1.0, Rules.UnderstorySpacingM);
			const std::int64_t FromX = static_cast<std::int64_t>(std::floor(Area.MinX / Spacing));
			const std::int64_t ToX = static_cast<std::int64_t>(std::floor(Area.MaxX / Spacing));
			const std::int64_t FromY = static_cast<std::int64_t>(std::floor(Area.MinY / Spacing));
			const std::int64_t ToY = static_cast<std::int64_t>(std::floor(Area.MaxY / Spacing));
			for (std::int64_t Cy = FromY; Cy <= ToY; ++Cy)
			{
				for (std::int64_t Cx = FromX; Cx <= ToX; ++Cx)
				{
					if (Out.size() >= Rules.Limit)
					{
						return;
					}
					const double X = (static_cast<double>(Cx) + 0.1 + 0.8 * CellValue(Cx, Cy, 7)) * Spacing;
					const double Y = (static_cast<double>(Cy) + 0.1 + 0.8 * CellValue(Cx, Cy, 8)) * Spacing;
					if (X < Area.MinX || X > Area.MaxX || Y < Area.MinY || Y > Area.MaxY)
					{
						continue;
					}
					const double Stand = 0.5 + 0.5 * Map.Noise(X, Y, 520.0);
					double Canopy = Stand * Stand;
					if (const CityInfo* Town = World.CityAt(X, Y); Town != nullptr && Town->Style == CityStyle::Rural)
					{
						Canopy = std::max(Canopy, 0.5);
					}
					else if (NearCrossroads(World, X, Y, Rules.CrossroadsWoodsM))
					{
						Canopy = std::max(Canopy, 0.45);
					}
					// Thin under a thin canopy, and the odd shrub in a yard, which is how yards are.
					if (CellValue(Cx, Cy, 9) > Canopy * 0.55)
					{
						continue;
					}
					SurfaceInfo Info;
					if (!CanPlantAt(Map, World, X, Y, Rules, &Info))
					{
						continue;
					}
					Plant Shrub;
					Shrub.X = X;
					Shrub.Y = Y;
					Shrub.GroundM = Info.GroundM;
					Shrub.Kind = PlantKind::Scrub;
					const double Size = CellValue(Cx, Cy, 10);
					Shrub.HeightM = 0.5 + 1.6 * Size;
					Shrub.RadiusM = 0.4 + 0.6 * Size;
					Shrub.Spin = 6.2831853 * CellValue(Cx, Cy, 11);
					Out.push_back(Shrub);
				}
			}
		}
	}

	std::vector<Plant> PlantsIn(const StatePlan& Map, const CityPlan& World, const Rect& Area,
		const PlantingRules& Rules)
	{
		using namespace detail_vegetation;
		std::vector<Plant> Out;
		const double Spacing = std::max(1.0, Rules.SpacingM);
		// Candidate cells are a grid in world space, not in the requested rectangle, so two adjoining
		// requests agree exactly on the cells they share rather than each laying down its own grid.
		const std::int64_t FromX = static_cast<std::int64_t>(std::floor(Area.MinX / Spacing));
		const std::int64_t ToX = static_cast<std::int64_t>(std::floor(Area.MaxX / Spacing));
		const std::int64_t FromY = static_cast<std::int64_t>(std::floor(Area.MinY / Spacing));
		const std::int64_t ToY = static_cast<std::int64_t>(std::floor(Area.MaxY / Spacing));
		if (ToX < FromX || ToY < FromY)
		{
			return Out;
		}
		const std::size_t Cells = static_cast<std::size_t>(ToX - FromX + 1) *
			static_cast<std::size_t>(ToY - FromY + 1);
		Out.reserve(std::min(Rules.Limit, Cells / 3 + 1));

		for (std::int64_t Cy = FromY; Cy <= ToY; ++Cy)
		{
			for (std::int64_t Cx = FromX; Cx <= ToX; ++Cx)
			{
				if (Out.size() >= Rules.Limit)
				{
					return Out;
				}
				// Where in the cell, so the grid does not show through as rows of trees.
				const double X = (static_cast<double>(Cx) + 0.12 + 0.76 * CellValue(Cx, Cy, 1)) * Spacing;
				const double Y = (static_cast<double>(Cy) + 0.12 + 0.76 * CellValue(Cx, Cy, 2)) * Spacing;
				if (X < Area.MinX || X > Area.MaxX || Y < Area.MinY || Y > Area.MaxY)
				{
					continue;
				}

				// Woods come in stands with fields between them, which is most of what makes a
				// landscape read as cultivated country rather than as uniform scatter. Two
				// wavelengths: where the woods are, and thinner patches inside them.
				const double Stand = 0.5 + 0.5 * Map.Noise(X, Y, 520.0);
				const double Patch = 0.5 + 0.5 * Map.Noise(X, Y, 95.0);
				double Density = Stand * Stand * (0.45 + 0.55 * Patch);

				SurfaceInfo Info;
				if (!CanPlantAt(Map, World, X, Y, Rules, &Info))
				{
					continue;
				}
				// A yard in a crossroads community is a wooded lot whatever the fields outside are doing: the
				// stand noise that leaves farmland open would leave the lots bare, and a house in Longs stands
				// in pines. Half the cells, which at this spacing is woods with room to walk through.
				if (const CityInfo* Town = World.CityAt(X, Y); Town != nullptr && Town->Style == CityStyle::Rural)
				{
					Density = std::max(Density, 0.5);
				}
				// And the country round it is pine woods: a Horry County crossroads stands in the trees, not
				// in a field, and from its streets the woods are the horizon.
				else if (NearCrossroads(World, X, Y, Rules.CrossroadsWoodsM))
				{
					Density = std::max(Density, 0.45);
				}
				// Thin out as the ground steepens, and stop at the tree line.
				const double Slope = std::fabs(Map.SurfaceAt(X + 4.0, Y).GroundM - Info.GroundM) / 4.0 +
					std::fabs(Map.SurfaceAt(X, Y + 4.0).GroundM - Info.GroundM) / 4.0;
				const bool bSteep = Slope > Rules.MaxTreeSlope;
				if (Info.GroundM > Rules.TreeLineM)
				{
					continue;
				}
				// A margin below the tree line where trees give out gradually.
				const double Fade = 220.0;
				if (Info.GroundM > Rules.TreeLineM - Fade)
				{
					Density *= (Rules.TreeLineM - Info.GroundM) / Fade;
				}
				if (CellValue(Cx, Cy, 3) > Density)
				{
					continue;
				}

				// Species. Water and the coast decide most of it; elevation decides the rest.
				const double ToCoast = Map.CoastDistance(X, Y);
				// Standing water within a short walk: a river bottom or the edge of a lake or swamp.
				const double Reach = 14.0;
				const bool bWet = Map.SurfaceAt(X + Reach, Y).bWater || Map.SurfaceAt(X - Reach, Y).bWater ||
					Map.SurfaceAt(X, Y + Reach).bWater || Map.SurfaceAt(X, Y - Reach).bWater;
				const double Pick = CellValue(Cx, Cy, 4);
				PlantKind Kind = PlantKind::Pine;
				if (bSteep)
				{
					Kind = PlantKind::Scrub;
				}
				else if (bWet && Info.GroundM < 30.0)
				{
					// River bottoms and the edges of lakes and swamps.
					Kind = Pick < 0.72 ? PlantKind::Cypress : PlantKind::Palmetto;
				}
				else if (ToCoast < 900.0)
				{
					// The beach strip: palms and palmetto, with a few wind-bent pines behind them.
					Kind = Pick < 0.42 ? PlantKind::Palm : (Pick < 0.78 ? PlantKind::Palmetto : PlantKind::Pine);
				}
				else if (Info.GroundM > 620.0)
				{
					// Up in the mountains: hardwood and scrub, no pine plantations and no palms.
					Kind = Pick < 0.58 ? PlantKind::Oak : PlantKind::Scrub;
				}
				else
				{
					// Inland: pine country, with oaks scattered through it and brush at the edges.
					Kind = Pick < 0.66 ? PlantKind::Pine : (Pick < 0.88 ? PlantKind::Oak : PlantKind::Scrub);
				}

				Plant Grown;
				Grown.X = X;
				Grown.Y = Y;
				Grown.GroundM = Info.GroundM;
				Grown.Kind = Kind;
				Grown.Spin = CellValue(Cx, Cy, 5) * 2.0 * Pi;
				Grown.Tint = CellValue(Cx, Cy, 6);
				const double Size = CellValue(Cx, Cy, 7);
				switch (Kind)
				{
				case PlantKind::Pine:
					Grown.HeightM = Lerp(13.0, 27.0, Size);
					Grown.RadiusM = Lerp(1.8, 3.2, Size);
					break;
				case PlantKind::Oak:
					Grown.HeightM = Lerp(8.0, 16.0, Size);
					Grown.RadiusM = Lerp(5.0, 9.5, Size);
					break;
				case PlantKind::Palm:
					Grown.HeightM = Lerp(6.0, 13.0, Size);
					Grown.RadiusM = Lerp(2.2, 3.4, Size);
					// Coastal palms lean away from the prevailing wind off the water.
					Grown.Lean = Lerp(0.05, 0.22, CellValue(Cx, Cy, 8));
					break;
				case PlantKind::Palmetto:
					Grown.HeightM = Lerp(1.2, 2.6, Size);
					Grown.RadiusM = Lerp(1.3, 2.3, Size);
					break;
				case PlantKind::Cypress:
					Grown.HeightM = Lerp(11.0, 22.0, Size);
					Grown.RadiusM = Lerp(2.6, 4.6, Size);
					break;
				case PlantKind::Scrub:
				case PlantKind::Count:
					Grown.HeightM = Lerp(0.7, 2.2, Size);
					Grown.RadiusM = Lerp(0.8, 2.0, Size);
					break;
				}
				Grown.LeanAim = CellValue(Cx, Cy, 9) * 2.0 * Pi;
				Out.push_back(Grown);
			}
		}
		Understory(Map, World, Area, Rules, Out);
		return Out;
	}

	namespace detail_vegetation
	{
		/** A point rotated about the plant's own axis, then leaned, then placed in the world. */
		void Place(const Plant& Value, double Lx, double Ly, double Lz, double Out[3])
		{
			// Spin about Z.
			const double Cs = std::cos(Value.Spin);
			const double Sn = std::sin(Value.Spin);
			double X = Lx * Cs - Ly * Sn;
			double Y = Lx * Sn + Ly * Cs;
			double Z = Lz;
			// Lean: tip the whole plant away from vertical, about the horizontal axis across LeanAim.
			if (Value.Lean > 1e-6)
			{
				const double Ax = std::cos(Value.LeanAim);
				const double Ay = std::sin(Value.LeanAim);
				const double Tilt = std::sin(Value.Lean) * Z;
				X += Ax * Tilt;
				Y += Ay * Tilt;
				Z *= std::cos(Value.Lean);
			}
			Out[0] = Value.X + X;
			Out[1] = Value.Y + Y;
			Out[2] = Value.GroundM + Z;
		}

		void AddTri(const Plant& Value, std::vector<PlantTri>& Out, const double A[3], const double B[3],
			const double C[3], const double Rgb[3])
		{
			PlantTri Tri;
			Place(Value, A[0], A[1], A[2], Tri.P[0]);
			Place(Value, B[0], B[1], B[2], Tri.P[1]);
			Place(Value, C[0], C[1], C[2], Tri.P[2]);
			Tri.Rgb[0] = Rgb[0];
			Tri.Rgb[1] = Rgb[1];
			Tri.Rgb[2] = Rgb[2];
			Out.push_back(Tri);
		}

		/** A tapered n-sided column from BottomZ to TopZ. Trunks and flared cypress bases. */
		void AddColumn(const Plant& Value, std::vector<PlantTri>& Out, int Sides, double BottomR,
			double TopR, double BottomZ, double TopZ, const double Rgb[3], double BendX = 0.0,
			double BendY = 0.0)
		{
			const int Count = std::max(3, Sides);
			for (int Side = 0; Side < Count; ++Side)
			{
				const double A0 = 2.0 * Pi * Side / Count;
				const double A1 = 2.0 * Pi * (Side + 1) / Count;
				const double Low0[3] = {std::cos(A0) * BottomR, std::sin(A0) * BottomR, BottomZ};
				const double Low1[3] = {std::cos(A1) * BottomR, std::sin(A1) * BottomR, BottomZ};
				const double High0[3] = {std::cos(A0) * TopR + BendX, std::sin(A0) * TopR + BendY, TopZ};
				const double High1[3] = {std::cos(A1) * TopR + BendX, std::sin(A1) * TopR + BendY, TopZ};
				AddTri(Value, Out, Low0, Low1, High1, Rgb);
				AddTri(Value, Out, Low0, High1, High0, Rgb);
			}
		}

		/** An n-sided cone: the skirt of a pine, a scrub bush, a cypress crown. */
		void AddCone(const Plant& Value, std::vector<PlantTri>& Out, int Sides, double Radius,
			double BaseZ, double TipZ, const double Rgb[3])
		{
			const int Count = std::max(3, Sides);
			const double Tip[3] = {0.0, 0.0, TipZ};
			for (int Side = 0; Side < Count; ++Side)
			{
				const double A0 = 2.0 * Pi * Side / Count;
				const double A1 = 2.0 * Pi * (Side + 1) / Count;
				const double Rim0[3] = {std::cos(A0) * Radius, std::sin(A0) * Radius, BaseZ};
				const double Rim1[3] = {std::cos(A1) * Radius, std::sin(A1) * Radius, BaseZ};
				AddTri(Value, Out, Rim0, Rim1, Tip, Rgb);
				// Closed underneath, so a crown seen from below is not a hole.
				const double Middle[3] = {0.0, 0.0, BaseZ};
				AddTri(Value, Out, Rim1, Rim0, Middle, Rgb);
			}
		}

		/** A frond or branch: a tapered blade from the crown out and down. */
		void AddFrond(const Plant& Value, std::vector<PlantTri>& Out, double Angle, double Reach,
			double FromZ, double Droop, double Width, const double Rgb[3])
		{
			const double Cx = std::cos(Angle);
			const double Cy = std::sin(Angle);
			// Across the frond, so it has width rather than being a line.
			const double Px = -Cy * Width;
			const double Py = Cx * Width;
			const double Root0[3] = {-Px * 0.3, -Py * 0.3, FromZ};
			const double Root1[3] = {Px * 0.3, Py * 0.3, FromZ};
			const double Mid0[3] = {Cx * Reach * 0.55 - Px, Cy * Reach * 0.55 - Py, FromZ + Droop * 0.25};
			const double Mid1[3] = {Cx * Reach * 0.55 + Px, Cy * Reach * 0.55 + Py, FromZ + Droop * 0.25};
			const double Tip[3] = {Cx * Reach, Cy * Reach, FromZ + Droop};
			AddTri(Value, Out, Root0, Root1, Mid1, Rgb);
			AddTri(Value, Out, Root0, Mid1, Mid0, Rgb);
			AddTri(Value, Out, Mid0, Mid1, Tip, Rgb);
			// And the same blade facing the other way, so it is visible from both sides.
			AddTri(Value, Out, Root1, Root0, Mid0, Rgb);
			AddTri(Value, Out, Mid1, Root1, Mid0, Rgb);
			AddTri(Value, Out, Mid1, Mid0, Tip, Rgb);
		}
	}

	void BuildPlant(const Plant& Value, std::vector<PlantTri>& Out)
	{
		using namespace detail_vegetation;
		// Foliage shade varies within a species: a stand of pines is not one flat green.
		const double Shade = 0.82 + 0.36 * Value.Tint;
		const double Bark[3] = {0.19 * (0.85 + 0.3 * Value.Tint), 0.14, 0.10};
		const double PaleBark[3] = {0.34, 0.30, 0.25};

		switch (Value.Kind)
		{
		case PlantKind::Pine:
		{
			// Longleaf: a bare trunk most of the way up, then three narrowing skirts.
			const double Green[3] = {0.055 * Shade, 0.20 * Shade, 0.075 * Shade};
			const double BranchFrom = Value.HeightM * 0.42;
			AddColumn(Value, Out, 5, Value.RadiusM * 0.17, Value.RadiusM * 0.09, 0.0, Value.HeightM * 0.95, Bark);
			for (int Layer = 0; Layer < 3; ++Layer)
			{
				const double T = Layer / 2.0;
				const double Base = Lerp(BranchFrom, Value.HeightM * 0.76, T);
				const double Radius = Lerp(Value.RadiusM, Value.RadiusM * 0.42, T);
				const double Tip = Base + (Value.HeightM - Base) * 0.72;
				AddCone(Value, Out, 6, Radius, Base, Tip, Green);
			}
			break;
		}
		case PlantKind::Oak:
		{
			// Live oak: short thick trunk, broad low crown.
			const double Green[3] = {0.085 * Shade, 0.195 * Shade, 0.065 * Shade};
			const double TrunkTop = Value.HeightM * 0.34;
			AddColumn(Value, Out, 6, Value.RadiusM * 0.17, Value.RadiusM * 0.12, 0.0, TrunkTop, Bark);
			// Three limbs out of the crotch, which is what gives a live oak its sprawl.
			for (int Limb = 0; Limb < 3; ++Limb)
			{
				const double Angle = 2.0 * Pi * Limb / 3.0 + Value.Spin;
				AddColumn(Value, Out, 4, Value.RadiusM * 0.10, Value.RadiusM * 0.06, TrunkTop * 0.7,
					TrunkTop + Value.HeightM * 0.16, Bark, std::cos(Angle) * Value.RadiusM * 0.5,
					std::sin(Angle) * Value.RadiusM * 0.5);
			}
			AddCone(Value, Out, 8, Value.RadiusM, TrunkTop + Value.HeightM * 0.05,
				TrunkTop + Value.HeightM * 0.30, Green);
			AddCone(Value, Out, 8, Value.RadiusM * 0.72, TrunkTop + Value.HeightM * 0.26, Value.HeightM, Green);
			break;
		}
		case PlantKind::Palm:
		{
			// Cabbage palm: a bare curved trunk and a burst of fronds at the top, nothing below.
			const double Green[3] = {0.10 * Shade, 0.21 * Shade, 0.085 * Shade};
			const int Joints = 3;
			double FromZ = 0.0;
			double BendX = 0.0;
			double BendY = 0.0;
			for (int Joint = 0; Joint < Joints; ++Joint)
			{
				const double ToZ = Value.HeightM * (Joint + 1) / Joints;
				const double Curve = 0.16 * Value.HeightM * (Joint + 1) / Joints / Joints;
				const double NextX = BendX + std::cos(Value.LeanAim) * Curve;
				const double NextY = BendY + std::sin(Value.LeanAim) * Curve;
				AddColumn(Value, Out, 5, Value.RadiusM * (0.15 - 0.02 * Joint),
					Value.RadiusM * (0.13 - 0.02 * Joint), FromZ, ToZ, PaleBark, NextX - BendX, NextY - BendY);
				FromZ = ToZ;
				BendX = NextX;
				BendY = NextY;
			}
			for (int Frond = 0; Frond < 9; ++Frond)
			{
				const double Angle = 2.0 * Pi * Frond / 9.0;
				// Alternating: some fronds stand up, some hang right over.
				const double Droop = (Frond % 2 == 0) ? -Value.HeightM * 0.16 : Value.HeightM * 0.06;
				AddFrond(Value, Out, Angle, Value.RadiusM * 1.35, Value.HeightM, Droop,
					Value.RadiusM * 0.16, Green);
			}
			break;
		}
		case PlantKind::Palmetto:
		{
			// A fan of blades straight out of the ground.
			const double Green[3] = {0.075 * Shade, 0.165 * Shade, 0.07 * Shade};
			for (int Frond = 0; Frond < 7; ++Frond)
			{
				const double Angle = 2.0 * Pi * Frond / 7.0;
				AddFrond(Value, Out, Angle, Value.RadiusM, Value.HeightM * 0.22,
					Value.HeightM * 0.55, Value.RadiusM * 0.26, Green);
			}
			break;
		}
		case PlantKind::Cypress:
		{
			// Bald cypress: a flared base standing in the water, then a narrow ragged crown.
			const double Green[3] = {0.11 * Shade, 0.17 * Shade, 0.085 * Shade};
			const double Grey[3] = {0.26, 0.23, 0.21};
			AddColumn(Value, Out, 6, Value.RadiusM * 0.42, Value.RadiusM * 0.13, 0.0,
				Value.HeightM * 0.22, Grey);
			AddColumn(Value, Out, 5, Value.RadiusM * 0.13, Value.RadiusM * 0.08, Value.HeightM * 0.22,
				Value.HeightM * 0.82, Grey);
			for (int Layer = 0; Layer < 2; ++Layer)
			{
				const double Base = Lerp(Value.HeightM * 0.52, Value.HeightM * 0.74, Layer);
				const double Radius = Lerp(Value.RadiusM, Value.RadiusM * 0.55, Layer);
				AddCone(Value, Out, 6, Radius, Base, Value.HeightM * (Layer == 0 ? 0.88 : 1.0), Green);
			}
			break;
		}
		case PlantKind::Scrub:
		case PlantKind::Count:
		{
			// A low bush: two overlapping domes, no trunk worth drawing.
			const double Green[3] = {0.105 * Shade, 0.155 * Shade, 0.075 * Shade};
			AddCone(Value, Out, 6, Value.RadiusM, 0.0, Value.HeightM, Green);
			AddCone(Value, Out, 5, Value.RadiusM * 0.68, Value.HeightM * 0.18, Value.HeightM * 0.82, Green);
			break;
		}
		}
	}
}
