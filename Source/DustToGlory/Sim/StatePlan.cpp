#include "Sim/StatePlan.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	namespace detail_stateplan
	{
		constexpr double Pi = 3.14159265358979;
		constexpr double CityMargin = 60.0;      // clear ground kept between neighbouring cities
		constexpr double RoadWidth = 18.0;
		constexpr double BlockSize = 110.0;
		constexpr double CityPad = 20.0;        // level ground around a city's outer road
		constexpr double CityFade = 120.0;      // ...blending back into the land over this distance
		constexpr double VenuePad = 15.0;
		constexpr double VenueFade = 80.0;
		constexpr double RoadShoulder = 2.0;
		constexpr double RoadFade = 28.0;

		double Smooth(double T)
		{
			const double X = Clamp(T, 0.0, 1.0);
			return X * X * (3.0 - 2.0 * X);
		}

		bool ParseVenueKind(const std::string& Text, EventKind& Out)
		{
			const std::string Key = ToLowerAscii(Text);
			if (Key == "dragrace") Out = EventKind::DragRace;
			else if (Key == "demolitionderby") Out = EventKind::DemolitionDerby;
			else if (Key == "monsterjam") Out = EventKind::MonsterJam;
			else if (Key == "formula") Out = EventKind::Formula;
			else return false;
			return true;
		}

		bool PointInPolygon(const std::vector<Point2>& Polygon, double X, double Y)
		{
			bool bInside = false;
			const std::size_t Count = Polygon.size();
			for (std::size_t I = 0, J = Count - 1; I < Count; J = I++)
			{
				const Point2& A = Polygon[I];
				const Point2& B = Polygon[J];
				if (((A[1] > Y) != (B[1] > Y)) && (X < (B[0] - A[0]) * (Y - A[1]) / (B[1] - A[1]) + A[0]))
				{
					bInside = !bInside;
				}
			}
			return bInside;
		}

		bool SegmentsIntersect(const Point2& P1, const Point2& P2, const Point2& Q1, const Point2& Q2)
		{
			auto Orient = [](const Point2& A, const Point2& B, const Point2& C)
			{
				const double Value = (B[0] - A[0]) * (C[1] - A[1]) - (B[1] - A[1]) * (C[0] - A[0]);
				return Value > 0.0 ? 1 : (Value < 0.0 ? -1 : 0);
			};
			return Orient(P1, P2, Q1) != Orient(P1, P2, Q2) && Orient(Q1, Q2, P1) != Orient(Q1, Q2, P2);
		}

		bool SegmentHitsRect(const Point2& A, const Point2& B, const Rect& Area)
		{
			if (Area.Contains(A[0], A[1]) || Area.Contains(B[0], B[1]))
			{
				return true;
			}
			const Point2 C0{Area.MinX, Area.MinY};
			const Point2 C1{Area.MaxX, Area.MinY};
			const Point2 C2{Area.MaxX, Area.MaxY};
			const Point2 C3{Area.MinX, Area.MaxY};
			return SegmentsIntersect(A, B, C0, C1) || SegmentsIntersect(A, B, C1, C2) || SegmentsIntersect(A, B, C2, C3) ||
				SegmentsIntersect(A, B, C3, C0);
		}

		Rect Expand(const Rect& Area, double Margin)
		{
			return Rect{Area.MinX - Margin, Area.MinY - Margin, Area.MaxX + Margin, Area.MaxY + Margin};
		}
	}

	double CitySpan(int Grid, double BlockSize, double RoadWidth)
	{
		return std::max(2, Grid) * (BlockSize + RoadWidth) + RoadWidth;
	}

	Point2 StatePlan::ToWorld(double Lon, double Lat) const
	{
		const double MetresPerLon = 111320.0 * std::cos(OriginLat * detail_stateplan::Pi / 180.0);
		return {(Lon - OriginLon) * MetresPerLon / Scale, -(Lat - OriginLat) * 110574.0 / Scale};
	}

	bool StatePlan::Load(const JsonValue& Root, std::vector<std::string>& Errors)
	{
		using namespace detail_stateplan;
		Name = Root.GetString("name", "State");
		Abbreviation = Root.GetString("abbreviation");
		Scale = std::max(1.0, Root.GetNumber("scale", 25.0));
		VerticalScale = Root.GetNumber("verticalScale", 3.0);
		const JsonValue& Origin = Root.Get("origin");
		OriginLon = Origin.At(0).AsNumber();
		OriginLat = Origin.At(1).AsNumber();
		Seed = HashString(Name);

		auto ReadPoint = [this](const JsonValue& Pair) { return ToWorld(Pair.At(0).AsNumber(), Pair.At(1).AsNumber()); };
		for (const JsonValue& Pair : Root.Get("outline").Items())
		{
			Outline.push_back(ReadPoint(Pair));
		}
		if (Outline.size() < 3)
		{
			Errors.push_back("state.json: the outline needs at least three points");
			return false;
		}
		CoastStart = Clamp(Root.Get("coast").At(0).AsInt(0), 0, static_cast<int>(Outline.size()) - 1);
		CoastEnd = Clamp(Root.Get("coast").At(1).AsInt(0), CoastStart, static_cast<int>(Outline.size()) - 1);
		Bounds = Rect{1e18, 1e18, -1e18, -1e18};
		for (const Point2& Corner : Outline)
		{
			Bounds.MinX = std::min(Bounds.MinX, Corner[0]);
			Bounds.MinY = std::min(Bounds.MinY, Corner[1]);
			Bounds.MaxX = std::max(Bounds.MaxX, Corner[0]);
			Bounds.MaxY = std::max(Bounds.MaxY, Corner[1]);
		}
		BuildSea();
		for (const JsonValue& Pair : Root.Get("mountains").Get("outline").Items())
		{
			Mountains.push_back(ReadPoint(Pair));
		}
		PeakMeters = Root.Get("mountains").GetNumber("peakMeters", 1000.0);

		for (const JsonValue& CityJson : Root.Get("cities").Items())
		{
			StateCity City;
			City.Name = CityJson.GetString("name");
			const Point2 At = ToWorld(CityJson.GetNumber("lon"), CityJson.GetNumber("lat"));
			City.X = At[0];
			City.Y = At[1];
			if (!ParseCityStyle(CityJson.GetString("style", "Town"), City.Style))
			{
				Errors.push_back("state.json: city " + City.Name + " has an unknown style");
			}
			City.Grid = Clamp(CityJson.GetInt("grid", 3), 2, 12);
			City.bStart = CityJson.GetBool("start");
			for (const JsonValue& Pair : CityJson.Get("landmarks").Items())
			{
				LotRole Role = LotRole::Shop;
				if (!ParseLotRole(Pair.At(0).AsString(), Role))
				{
					Errors.push_back("state.json: " + City.Name + " landmark has unknown role " + Pair.At(0).AsString());
					continue;
				}
				City.Landmarks.emplace_back(Role, Pair.At(1).AsString());
			}
			Cities.push_back(City);
		}
		int Starts = 0;
		int Hubs = 0;
		for (const StateCity& City : Cities)
		{
			Starts += City.bStart ? 1 : 0;
			Hubs += City.Style == CityStyle::Hub ? 1 : 0;
		}
		if (Starts != 1 || Hubs != 1)
		{
			Errors.push_back("state.json: needs exactly one start city and one hub city");
		}
		for (const JsonValue& LakeJson : Root.Get("lakes").Items())
		{
			StateLake Lake;
			Lake.Name = LakeJson.GetString("name");
			const Point2 At = ToWorld(LakeJson.GetNumber("lon"), LakeJson.GetNumber("lat"));
			Lake.X = At[0];
			Lake.Y = At[1];
			Lake.RadiusM = LakeJson.GetNumber("radiusKm", 3.0) * 1000.0 / Scale;
			Lakes.push_back(Lake);
		}
		for (const JsonValue& RiverJson : Root.Get("rivers").Items())
		{
			StateRiver River;
			River.Name = RiverJson.GetString("name");
			River.WidthM = RiverJson.GetNumber("width", 8.0);
			for (const JsonValue& Pair : RiverJson.Get("points").Items())
			{
				River.Points.push_back(ReadPoint(Pair));
			}
			Rivers.push_back(River);
		}
		for (const JsonValue& VenueJson : Root.Get("venues").Items())
		{
			StateVenue Venue;
			Venue.Name = VenueJson.GetString("name");
			if (!ParseVenueKind(VenueJson.GetString("kind"), Venue.Kind))
			{
				Errors.push_back("state.json: venue " + Venue.Name + " has an unknown kind");
				continue;
			}
			const Point2 At = ToWorld(VenueJson.GetNumber("lon"), VenueJson.GetNumber("lat"));
			Venue.X = At[0];
			Venue.Y = At[1];
			Venues.push_back(Venue);
		}

		PlaceCities();
		for (StateLake& Lake : Lakes)
		{
			Lake.LevelM = std::max(0.5, NaturalHeight(Lake.X, Lake.Y) - 1.0);
		}
		std::vector<std::pair<std::string, std::vector<JsonValue>>> Routes;
		std::vector<bool> Interstate;
		for (const JsonValue& HighwayJson : Root.Get("highways").Items())
		{
			Routes.emplace_back(HighwayJson.GetString("name"), HighwayJson.Get("route").Items());
			Interstate.push_back(HighwayJson.GetBool("interstate"));
		}
		RouteHighways(Routes, Interstate, Errors);
		PlaceVenues();
		DetourAroundCities(); // venue connector roads too
		JoinCrossings();
		ComputeDecks();

		return Errors.empty();
	}

	Rect StatePlan::CityRect(const StateCity& City, double Margin) const
	{
		const double Half = 0.5 * CitySpan(City.Grid) + Margin;
		return Rect{City.X - Half, City.Y - Half, City.X + Half, City.Y + Half};
	}

	const StateCity* StatePlan::StartCity() const
	{
		for (const StateCity& City : Cities)
		{
			if (City.bStart)
			{
				return &City;
			}
		}
		return Cities.empty() ? nullptr : &Cities.front();
	}

	const StateCity* StatePlan::FindCity(const std::string& CityName) const
	{
		for (const StateCity& City : Cities)
		{
			if (City.Name == CityName)
			{
				return &City;
			}
		}
		return nullptr;
	}

	bool StatePlan::InsideState(double X, double Y) const
	{
		return detail_stateplan::PointInPolygon(Outline, X, Y);
	}

	double StatePlan::SegmentDistance(const Point2& A, const Point2& B, double X, double Y, double* OutT)
	{
		const double Dx = B[0] - A[0];
		const double Dy = B[1] - A[1];
		const double Length2 = Dx * Dx + Dy * Dy;
		double T = Length2 > 1e-9 ? ((X - A[0]) * Dx + (Y - A[1]) * Dy) / Length2 : 0.0;
		T = Clamp(T, 0.0, 1.0);
		if (OutT)
		{
			*OutT = T;
		}
		const double Px = A[0] + T * Dx - X;
		const double Py = A[1] + T * Dy - Y;
		return std::sqrt(Px * Px + Py * Py);
	}

	double StatePlan::CoastDistance(double X, double Y) const
	{
		double Best = 1e18;
		for (int Index = CoastStart; Index < CoastEnd; ++Index)
		{
			Best = std::min(Best, SegmentDistance(Outline[static_cast<std::size_t>(Index)], Outline[static_cast<std::size_t>(Index + 1)], X, Y));
		}
		return Best;
	}

	bool StatePlan::IsOcean(double X, double Y) const
	{
		return !InsideState(X, Y) && detail_stateplan::PointInPolygon(Sea, X, Y);
	}

	void StatePlan::BuildSea()
	{
		// The sea: the coastline, continued straight past both ends (the neighbouring states' coasts), closed far offshore.
		Sea.clear();
		if (CoastEnd <= CoastStart)
		{
			return;
		}
		const Point2& First = Outline[static_cast<std::size_t>(CoastStart)];
		const Point2& Second = Outline[static_cast<std::size_t>(CoastStart + 1)];
		const Point2& Last = Outline[static_cast<std::size_t>(CoastEnd)];
		const Point2& BeforeLast = Outline[static_cast<std::size_t>(CoastEnd - 1)];
		const double Far = 4.0 * std::max(Bounds.Width(), Bounds.Depth()) + 10000.0;
		auto Extend = [Far](const Point2& From, const Point2& Toward)
		{
			const double Dx = Toward[0] - From[0];
			const double Dy = Toward[1] - From[1];
			const double Length = std::max(1e-6, std::sqrt(Dx * Dx + Dy * Dy));
			return Point2{Toward[0] + Dx / Length * Far, Toward[1] + Dy / Length * Far};
		};
		// Offshore is away from the state's middle, perpendicular to the overall run of the coast.
		double CenterX = 0.0;
		double CenterY = 0.0;
		for (const Point2& Corner : Outline)
		{
			CenterX += Corner[0];
			CenterY += Corner[1];
		}
		CenterX /= static_cast<double>(Outline.size());
		CenterY /= static_cast<double>(Outline.size());
		const double RunX = Last[0] - First[0];
		const double RunY = Last[1] - First[1];
		double NormalX = -RunY;
		double NormalY = RunX;
		const double MidX = 0.5 * (First[0] + Last[0]);
		const double MidY = 0.5 * (First[1] + Last[1]);
		if (NormalX * (MidX - CenterX) + NormalY * (MidY - CenterY) < 0.0)
		{
			NormalX = -NormalX;
			NormalY = -NormalY;
		}
		const double NormalLength = std::max(1e-6, std::sqrt(NormalX * NormalX + NormalY * NormalY));
		const Point2 StartFar = Extend(Second, First);
		const Point2 EndFar = Extend(BeforeLast, Last);
		Sea.push_back(StartFar);
		for (int Index = CoastStart; Index <= CoastEnd; ++Index)
		{
			Sea.push_back(Outline[static_cast<std::size_t>(Index)]);
		}
		Sea.push_back(EndFar);
		Sea.push_back({EndFar[0] + NormalX / NormalLength * Far, EndFar[1] + NormalY / NormalLength * Far});
		Sea.push_back({StartFar[0] + NormalX / NormalLength * Far, StartFar[1] + NormalY / NormalLength * Far});
	}

	double StatePlan::Noise(double X, double Y, double Wavelength) const
	{
		const double Fx = X / Wavelength;
		const double Fy = Y / Wavelength;
		const double Ix = std::floor(Fx);
		const double Iy = std::floor(Fy);
		auto Corner = [this](double Cx, double Cy)
		{
			const std::uint64_t Key = (static_cast<std::uint64_t>(static_cast<std::int64_t>(Cx) + 100000) * 73856093ULL) ^
				(static_cast<std::uint64_t>(static_cast<std::int64_t>(Cy) + 100000) * 19349663ULL) ^ Seed;
			const std::uint64_t Mixed = (Key ^ (Key >> 29)) * 0xBF58476D1CE4E5B9ULL;
			return static_cast<double>((Mixed ^ (Mixed >> 32)) & 0xFFFFu) / 32767.5 - 1.0;
		};
		const double Tx = detail_stateplan::Smooth(Fx - Ix);
		const double Ty = detail_stateplan::Smooth(Fy - Iy);
		const double Top = Corner(Ix, Iy) + (Corner(Ix + 1.0, Iy) - Corner(Ix, Iy)) * Tx;
		const double Bottom = Corner(Ix, Iy + 1.0) + (Corner(Ix + 1.0, Iy + 1.0) - Corner(Ix, Iy + 1.0)) * Tx;
		return Top + (Bottom - Top) * Ty;
	}

	double StatePlan::NaturalHeight(double X, double Y) const
	{
		const double Coast = CoastDistance(X, Y);
		if (IsOcean(X, Y))
		{
			return -2.0 - std::min(12.0, Coast * 0.02); // the continental shelf
		}
		const double RealKm = Coast * Scale / 1000.0;
		// Coastal plain rising through the sandhills to the Piedmont, with rolling hills that grow inland.
		double Real = std::min(300.0, 0.85 * RealKm);
		const double Amplitude = 2.0 + std::min(25.0, RealKm * 0.12);
		Real += Amplitude * (Noise(X, Y, 900.0) + 0.35 * Noise(X, Y, 250.0));
		if (Mountains.size() >= 3)
		{
			const bool bInside = detail_stateplan::PointInPolygon(Mountains, X, Y);
			double Edge = 1e18;
			for (std::size_t Index = 0; Index < Mountains.size(); ++Index)
			{
				Edge = std::min(Edge, SegmentDistance(Mountains[Index], Mountains[(Index + 1) % Mountains.size()], X, Y));
			}
			const double Depth = bInside ? Edge : -Edge;
			const double Weight = detail_stateplan::Smooth((Depth + 600.0) / 1800.0);
			if (Weight > 0.0)
			{
				const double Ridges = 0.55 + 0.45 * std::fabs(Noise(X, Y, 700.0));
				Real += PeakMeters * Weight * Ridges;
			}
		}
		double Height = std::max(0.0, Real) / Scale * VerticalScale;
		if (Coast < 60.0)
		{
			Height = 0.6 + (Height - 0.6) * detail_stateplan::Smooth(Coast / 60.0); // the beach
		}
		return Height;
	}

	double StatePlan::RectDistance(const Rect& Area, double X, double Y)
	{
		const double Dx = std::max({Area.MinX - X, 0.0, X - Area.MaxX});
		const double Dy = std::max({Area.MinY - Y, 0.0, Y - Area.MaxY});
		return std::sqrt(Dx * Dx + Dy * Dy);
	}

	SurfaceInfo StatePlan::SurfaceAt(double X, double Y) const
	{
		using namespace detail_stateplan;
		SurfaceInfo Info;
		const double Natural = NaturalHeight(X, Y);
		Info.GroundM = Natural;
		if (IsOcean(X, Y))
		{
			Info.bWater = true;
			Info.bOcean = true;
			Info.WaterM = 0.0;
			return Info;
		}
		for (const StateLake& Lake : Lakes)
		{
			const double Dx = X - Lake.X;
			const double Dy = Y - Lake.Y;
			if (std::fabs(Dx) > Lake.RadiusM + 60.0 || std::fabs(Dy) > Lake.RadiusM + 60.0)
			{
				continue;
			}
			const double Distance = std::sqrt(Dx * Dx + Dy * Dy);
			if (Distance < Lake.RadiusM)
			{
				Info.bWater = true;
				Info.WaterM = Lake.LevelM;
				Info.GroundM = Lake.LevelM - 0.5 - 4.0 * std::sqrt(1.0 - Distance / Lake.RadiusM);
			}
			else if (Distance < Lake.RadiusM + 60.0)
			{
				const double Shore = detail_stateplan::Smooth((Distance - Lake.RadiusM) / 60.0);
				Info.GroundM = (Lake.LevelM + 0.3) + (Info.GroundM - (Lake.LevelM + 0.3)) * Shore;
			}
		}
		for (const StateRiver& River : Rivers)
		{
			for (std::size_t Index = 0; Index + 1 < River.Points.size(); ++Index)
			{
				const Point2& A = River.Points[Index];
				const Point2& B = River.Points[Index + 1];
				const double Reach = 0.5 * River.WidthM + 15.0;
				if (X < std::min(A[0], B[0]) - Reach || X > std::max(A[0], B[0]) + Reach || Y < std::min(A[1], B[1]) - Reach ||
					Y > std::max(A[1], B[1]) + Reach)
				{
					continue;
				}
				const double Distance = SegmentDistance(A, B, X, Y);
				const double Half = 0.5 * River.WidthM;
				if (Distance < Half)
				{
					Info.bWater = true;
					Info.WaterM = std::max(0.2, Natural - 0.8);
					Info.GroundM = std::min(Info.GroundM, Natural - 2.5);
				}
				else if (Distance < Reach)
				{
					const double Bank = detail_stateplan::Smooth((Distance - Half) / 15.0);
					Info.GroundM = std::min(Info.GroundM, (Natural - 0.5) + 0.5 * Bank);
				}
			}
		}
		// Blends toward several targets at once without seams: inside any target's core its value holds exactly;
		// outside, an inverse-distance average of the nearby targets is faded in by the strongest weight.
		struct Blend
		{
			bool bCore = false;
			double CoreSum = 0.0;
			int CoreCount = 0;
			double WeightSum = 0.0;
			double ValueSum = 0.0;
			double Strongest = 0.0;
			void Add(double Distance, double Fade, double Value)
			{
				if (Distance <= 0.0)
				{
					bCore = true;
					CoreSum += Value;
					++CoreCount;
					return;
				}
				if (Distance >= Fade)
				{
					return;
				}
				const double Weight = 1.0 - detail_stateplan::Smooth(Distance / Fade);
				Strongest = std::max(Strongest, Weight);
				const double Inverse = Weight / (Distance * Distance);
				WeightSum += Inverse;
				ValueSum += Inverse * Value;
			}
			double Apply(double Base) const
			{
				if (bCore)
				{
					return CoreSum / static_cast<double>(CoreCount);
				}
				return WeightSum > 0.0 ? Base + (ValueSum / WeightSum - Base) * Strongest : Base;
			}
		};
		// Roads are graded to a straight line between their vertices (over water too: that's the bridge deck).
		Blend Roads;
		for (const HighwayPlan& Road : Highways)
		{
			for (std::size_t Index = 0; Index + 1 < Road.Points.size(); ++Index)
			{
				const Point2& A = Road.Points[Index];
				const Point2& B = Road.Points[Index + 1];
				const double Reach = 0.5 * Road.WidthM + RoadShoulder + RoadFade;
				if (X < std::min(A[0], B[0]) - Reach || X > std::max(A[0], B[0]) + Reach || Y < std::min(A[1], B[1]) - Reach ||
					Y > std::max(A[1], B[1]) + Reach)
				{
					continue;
				}
				double T = 0.0;
				const double Distance = SegmentDistance(A, B, X, Y, &T);
				Roads.Add(Distance - 0.5 * Road.WidthM - RoadShoulder, RoadFade, AnchorHeight(A) + (AnchorHeight(B) - AnchorHeight(A)) * T);
			}
		}
		const double RoadGrade = Roads.Apply(Info.GroundM);
		// Cities and venues sit on level pads (filling any water they cover); roads blend into them.
		Blend Pads;
		for (const StateCity& City : Cities)
		{
			Pads.Add(RectDistance(CityRect(City, CityPad), X, Y), CityFade, City.LevelM);
		}
		for (std::size_t Index = 0; Index < Venues.size() && Index < VenueLayouts.size(); ++Index)
		{
			Pads.Add(RectDistance(detail_stateplan::Expand(VenueLayouts[Index].Area, VenuePad), X, Y), VenueFade, Venues[Index].LevelM);
		}
		const double LandHeight = Pads.Apply(RoadGrade);
		if (Info.bWater)
		{
			const double Bed = Pads.Apply(Info.GroundM);
			if (Pads.bCore || Bed >= Info.WaterM - 0.3)
			{
				Info.bWater = false; // filled in under a town or venue
			}
			else
			{
				Info.GroundM = Bed;
			}
		}
		if (!Info.bWater)
		{
			Info.GroundM = LandHeight;
		}
		Info.RoadGradeM = LandHeight;
		return Info;
	}

	double StatePlan::RoadHeightAt(double X, double Y) const
	{
		// On a highway: its deck profile. Elsewhere: the ground, or a deck just above any water.
		double Best = 1e18;
		double Deck = 0.0;
		for (const HighwayPlan& Road : Highways)
		{
			double Walked = 0.0;
			for (std::size_t Index = 0; Index + 1 < Road.Points.size(); ++Index)
			{
				const Point2& A = Road.Points[Index];
				const Point2& B = Road.Points[Index + 1];
				const double Length = std::hypot(B[0] - A[0], B[1] - A[1]);
				double T = 0.0;
				const double Distance = SegmentDistance(A, B, X, Y, &T);
				if (Distance <= 0.5 * Road.WidthM + 2.0 && Distance < Best && !Road.DeckM.empty())
				{
					Best = Distance;
					Deck = Road.At(Walked + T * Length)[2];
				}
				Walked += Length;
			}
		}
		if (Best < 1e17)
		{
			return Deck;
		}
		const SurfaceInfo Info = SurfaceAt(X, Y);
		return Info.bWater ? std::max(Info.RoadGradeM, Info.WaterM + 3.0) : Info.GroundM;
	}

	void StatePlan::HarmonizeLevels()
	{
		using namespace detail_stateplan;
		// Neighbouring pads (cities and venues) can't differ much in level across the gap between them, so the
		// land and the roads in between stay under about a 4% grade.
		struct Pad
		{
			Rect Area;
			double* Level;
		};
		std::vector<Pad> Pads;
		for (StateCity& City : Cities)
		{
			Pads.push_back({CityRect(City), &City.LevelM});
		}
		for (std::size_t Index = 0; Index < Venues.size() && Index < VenueLayouts.size(); ++Index)
		{
			Pads.push_back({VenueLayouts[Index].Area, &Venues[Index].LevelM});
		}
		for (int Iteration = 0; Iteration < 100; ++Iteration)
		{
			bool bChanged = false;
			for (std::size_t A = 0; A < Pads.size(); ++A)
			{
				for (std::size_t B = A + 1; B < Pads.size(); ++B)
				{
					const Rect& First = Pads[A].Area;
					const Rect& Second = Pads[B].Area;
					const double Dx = std::max({First.MinX - Second.MaxX, 0.0, Second.MinX - First.MaxX});
					const double Dy = std::max({First.MinY - Second.MaxY, 0.0, Second.MinY - First.MaxY});
					const double Gap = std::sqrt(Dx * Dx + Dy * Dy);
					if (Gap > 400.0)
					{
						continue;
					}
					// The fade between two pads starts at their level margins and is steepest mid-way (smoothstep).
					const double Allowed = 0.025 * std::max(Gap - 2.0 * CityPad, 10.0);
					const double Difference = *Pads[A].Level - *Pads[B].Level;
					if (std::fabs(Difference) <= Allowed + 1e-6)
					{
						continue;
					}
					const double Excess = 0.5 * (std::fabs(Difference) - Allowed) * (Difference > 0.0 ? 1.0 : -1.0);
					*Pads[A].Level -= Excess;
					*Pads[B].Level += Excess;
					bChanged = true;
				}
			}
			if (!bChanged)
			{
				break;
			}
		}
		for (StateCity& City : Cities)
		{
			City.LevelM = std::max(0.8, City.LevelM);
		}
	}

	void StatePlan::ComputeDecks()
	{
		// The deck follows the graded road; over water it lifts to clear the surface, and the lift tapers off at
		// no more than 6% either side so bridges have ramps rather than steps.
		constexpr double Clearance = 3.0;
		constexpr double MaxRamp = 0.06;
		for (HighwayPlan& Road : Highways)
		{
			const double Length = Road.LengthM();
			const std::size_t Count = static_cast<std::size_t>(std::ceil(Length / Road.DeckStepM)) + 1;
			std::vector<double> Grade(Count, 0.0);
			std::vector<double> Lift(Count, 0.0);
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				const std::array<double, 3> Where = Road.At(std::min(Length, static_cast<double>(Index) * Road.DeckStepM));
				const SurfaceInfo Info = SurfaceAt(Where[0], Where[1]);
				Grade[Index] = Info.RoadGradeM;
				Lift[Index] = Info.bWater ? std::max(0.0, Info.WaterM + Clearance - Info.RoadGradeM) : 0.0;
			}
			const double Taper = MaxRamp * Road.DeckStepM;
			for (std::size_t Index = 1; Index < Count; ++Index)
			{
				Lift[Index] = std::max(Lift[Index], Lift[Index - 1] - Taper);
			}
			for (std::size_t Index = Count - 1; Index-- > 0;)
			{
				Lift[Index] = std::max(Lift[Index], Lift[Index + 1] - Taper);
			}
			Road.DeckM.resize(Count);
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Road.DeckM[Index] = Grade[Index] + Lift[Index];
			}
		}
	}

	bool StatePlan::InsideFlattened(double X, double Y) const
	{
		using namespace detail_stateplan;
		for (const StateCity& City : Cities)
		{
			if (CityRect(City, CityPad).Contains(X, Y))
			{
				return true;
			}
		}
		for (const VenuePlan& Layout : VenueLayouts)
		{
			if (detail_stateplan::Expand(Layout.Area, VenuePad).Contains(X, Y))
			{
				return true;
			}
		}
		return false;
	}

	double StatePlan::AnchorHeight(const Point2& Where) const
	{
		using namespace detail_stateplan;
		for (const StateCity& City : Cities)
		{
			if (CityRect(City, CityPad).Contains(Where[0], Where[1]))
			{
				return City.LevelM;
			}
		}
		for (std::size_t Index = 0; Index < Venues.size() && Index < VenueLayouts.size(); ++Index)
		{
			if (detail_stateplan::Expand(VenueLayouts[Index].Area, VenuePad).Contains(Where[0], Where[1]))
			{
				return Venues[Index].LevelM;
			}
		}
		return std::max(0.5, NaturalHeight(Where[0], Where[1]));
	}

	Point2 StatePlan::NearestBorderPoint(double X, double Y) const
	{
		Point2 Best{X, Y};
		double BestDistance = 1e18;
		for (std::size_t Index = 0; Index < Outline.size(); ++Index)
		{
			const Point2& A = Outline[Index];
			const Point2& B = Outline[(Index + 1) % Outline.size()];
			double T = 0.0;
			const double Distance = SegmentDistance(A, B, X, Y, &T);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = {A[0] + (B[0] - A[0]) * T, A[1] + (B[1] - A[1]) * T};
			}
		}
		return Best;
	}

	void StatePlan::PlaceCities()
	{
		using namespace detail_stateplan;
		// Pull a footprint that pokes into the sea (or over the border) straight back onto land.
		auto PullInland = [this](StateCity& City)
		{
			for (int Step = 0; Step < 400 && !FootprintOnLand(City); ++Step)
			{
				const Rect Area = CityRect(City, 40.0);
				const Point2 Corners[4] = {{Area.MinX, Area.MinY}, {Area.MaxX, Area.MinY}, {Area.MinX, Area.MaxY}, {Area.MaxX, Area.MaxY}};
				double Dx = 0.0;
				double Dy = 0.0;
				for (const Point2& Corner : Corners)
				{
					if (!InsideState(Corner[0], Corner[1]))
					{
						const Point2 Border = NearestBorderPoint(Corner[0], Corner[1]);
						Dx += Border[0] - Corner[0];
						Dy += Border[1] - Corner[1];
					}
				}
				const double Length = std::sqrt(Dx * Dx + Dy * Dy);
				if (Length < 1e-6)
				{
					break;
				}
				const double Move = std::min(15.0, Length + 2.0);
				City.X += Dx / Length * Move;
				City.Y += Dy / Length * Move;
			}
		};
		for (StateCity& City : Cities)
		{
			PullInland(City);
		}
		// Real positions compress unevenly at game scale. Neighbours that overlap are pushed apart along the axis
		// that needs the smaller move; big cities move less, the start city not at all, and nobody into the sea.
		for (int Iteration = 0; Iteration < 400; ++Iteration)
		{
			bool bMoved = false;
			for (std::size_t A = 0; A < Cities.size(); ++A)
			{
				for (std::size_t B = A + 1; B < Cities.size(); ++B)
				{
					StateCity& First = Cities[A];
					StateCity& Second = Cities[B];
					const double FirstSpan = CitySpan(First.Grid);
					const double SecondSpan = CitySpan(Second.Grid);
					const double Need = 0.5 * (FirstSpan + SecondSpan) + CityMargin;
					const double Dx = Second.X - First.X;
					const double Dy = Second.Y - First.Y;
					const double OverX = Need - std::fabs(Dx);
					const double OverY = Need - std::fabs(Dy);
					if (OverX <= 0.0 || OverY <= 0.0)
					{
						continue;
					}
					const bool bAlongX = OverX < OverY;
					const double Push = (bAlongX ? OverX : OverY) + 1.0;
					const double Sign = bAlongX ? (Dx >= 0.0 ? 1.0 : -1.0) : (Dy >= 0.0 ? 1.0 : -1.0);
					const double FirstArea = FirstSpan * FirstSpan;
					const double SecondArea = SecondSpan * SecondSpan;
					double FirstShare = First.bStart ? 0.0 : (Second.bStart ? 1.0 : SecondArea / (FirstArea + SecondArea));
					const StateCity FirstBefore = First;
					const StateCity SecondBefore = Second;
					auto Apply = [&](double Share)
					{
						First = FirstBefore;
						Second = SecondBefore;
						(bAlongX ? First.X : First.Y) -= Sign * Push * Share;
						(bAlongX ? Second.X : Second.Y) += Sign * Push * (1.0 - Share);
					};
					Apply(FirstShare);
					// If one of them would end up in the sea, the other takes the whole move.
					if (!FootprintOnLand(First) && FootprintOnLand(FirstBefore) && !Second.bStart)
					{
						FirstShare = 0.0;
						Apply(FirstShare);
					}
					else if (!FootprintOnLand(Second) && FootprintOnLand(SecondBefore) && !First.bStart)
					{
						FirstShare = 1.0;
						Apply(FirstShare);
					}
					PullInland(First);
					PullInland(Second);
					bMoved = true;
				}
			}
			if (!bMoved)
			{
				break;
			}
		}
		for (StateCity& City : Cities)
		{
			City.Bounds = CityRect(City);
			City.LevelM = std::max(1.0, NaturalHeight(City.X, City.Y));
		}
	}

	void StatePlan::RouteHighways(const std::vector<std::pair<std::string, std::vector<JsonValue>>>& Routes, const std::vector<bool>& Interstate,
		std::vector<std::string>& Errors)
	{
		using namespace detail_stateplan;
		const double Pitch = BlockSize + RoadWidth;
		// The middle road on the side of a city facing a point: exactly one of the city's own intersections.
		auto Gate = [&](const StateCity& City, const Point2& Toward)
		{
			const Rect Area = CityRect(City);
			const double Dx = Toward[0] - City.X;
			const double Dy = Toward[1] - City.Y;
			const double MidX = Area.MinX + (City.Grid / 2) * Pitch + 0.5 * RoadWidth;
			const double MidY = Area.MinY + (City.Grid / 2) * Pitch + 0.5 * RoadWidth;
			if (std::fabs(Dx) > std::fabs(Dy))
			{
				return Point2{Dx > 0.0 ? Area.MaxX - 0.5 * RoadWidth : Area.MinX + 0.5 * RoadWidth, MidY};
			}
			return Point2{MidX, Dy > 0.0 ? Area.MaxY - 0.5 * RoadWidth : Area.MinY + 0.5 * RoadWidth};
		};

		for (std::size_t RouteIndex = 0; RouteIndex < Routes.size(); ++RouteIndex)
		{
			const std::string& RoadName = Routes[RouteIndex].first;
			// Resolve every step to a point, remembering which steps are cities.
			std::vector<Point2> Steps;
			std::vector<int> CityOf;
			for (const JsonValue& Step : Routes[RouteIndex].second)
			{
				if (Step.IsArray())
				{
					Steps.push_back(ToWorld(Step.At(0).AsNumber(), Step.At(1).AsNumber()));
					CityOf.push_back(-1);
					continue;
				}
				int Found = -1;
				for (std::size_t Index = 0; Index < Cities.size(); ++Index)
				{
					if (Cities[Index].Name == Step.AsString())
					{
						Found = static_cast<int>(Index);
					}
				}
				if (Found < 0)
				{
					Errors.push_back("state.json: highway " + RoadName + " goes through unknown city " + Step.AsString());
					continue;
				}
				Steps.push_back({Cities[static_cast<std::size_t>(Found)].X, Cities[static_cast<std::size_t>(Found)].Y});
				CityOf.push_back(Found);
			}
			// Cut the route at every city: the highway meets the city's grid at a gate on the facing side.
			std::vector<Point2> Piece;
			auto Flush = [&]()
			{
				if (Piece.size() >= 2)
				{
					HighwayPlan Road;
					Road.Name = RoadName;
					Road.Points = Piece;
					Road.bInterstate = Interstate[RouteIndex];
					Road.WidthM = Road.bInterstate ? 22.0 : (RoadName.rfind("US-", 0) == 0 ? 16.0 : 12.0);
					Highways.push_back(Road);
				}
				Piece.clear();
			};
			for (std::size_t Index = 0; Index < Steps.size(); ++Index)
			{
				if (CityOf[Index] < 0)
				{
					Piece.push_back(Steps[Index]);
					continue;
				}
				const StateCity& City = Cities[static_cast<std::size_t>(CityOf[Index])];
				if (Index > 0)
				{
					Piece.push_back(Gate(City, Steps[Index - 1]));
					Flush();
				}
				if (Index + 1 < Steps.size())
				{
					Piece.push_back(Gate(City, Steps[Index + 1]));
				}
			}
			Flush();
		}

		DetourAroundCities();
	}

	bool StatePlan::FootprintOnLand(const StateCity& City) const
	{
		const Rect Area = CityRect(City, 40.0);
		return InsideState(Area.MinX, Area.MinY) && InsideState(Area.MaxX, Area.MinY) && InsideState(Area.MinX, Area.MaxY) &&
			InsideState(Area.MaxX, Area.MaxY);
	}

	void StatePlan::DetourAroundCities()
	{
		using namespace detail_stateplan;
		// A segment that would plough through a city it doesn't serve goes around one or two of its corners instead,
		// choosing the shortest way that clears the city and doesn't cut a corner through another one.
		auto InsideAnyCity = [this](const Point2& Where)
		{
			for (const StateCity& City : Cities)
			{
				if (CityRect(City, 10.0).Contains(Where[0], Where[1]))
				{
					return true;
				}
			}
			return false;
		};
		for (HighwayPlan& Road : Highways)
		{
			for (int Pass = 0; Pass < 8; ++Pass)
			{
				bool bChanged = false;
				for (std::size_t Index = 0; Index + 1 < Road.Points.size() && Road.Points.size() < 96 && !bChanged; ++Index)
				{
					const Point2 A = Road.Points[Index];
					const Point2 B = Road.Points[Index + 1];
					for (const StateCity& City : Cities)
					{
						// A segment ending at one of this city's gates may touch its edge, but not cut across it.
						const Rect Gates = CityRect(City, 1.0);
						const bool bGate = Gates.Contains(A[0], A[1]) || Gates.Contains(B[0], B[1]);
						const Rect Blocked = bGate ? CityRect(City, -25.0) : CityRect(City, 15.0);
						if (!SegmentHitsRect(A, B, Blocked))
						{
							continue;
						}
						const Rect Around = CityRect(City, 45.0);
						const Point2 Corners[4] = {{Around.MinX, Around.MinY}, {Around.MaxX, Around.MinY}, {Around.MaxX, Around.MaxY}, {Around.MinX, Around.MaxY}};
						std::vector<Point2> Best;
						double BestLength = 1e18;
						auto Consider = [&](const std::vector<Point2>& Via)
						{
							Point2 From = A;
							double Length = 0.0;
							for (std::size_t Step = 0; Step <= Via.size(); ++Step)
							{
								const Point2 To = Step < Via.size() ? Via[Step] : B;
								if (SegmentHitsRect(From, To, Blocked))
								{
									return;
								}
								Length += std::hypot(To[0] - From[0], To[1] - From[1]);
								From = To;
							}
							double Penalty = 0.0;
							for (const Point2& Corner : Via)
							{
								Penalty += (InsideAnyCity(Corner) || !InsideState(Corner[0], Corner[1])) ? 1e6 : 0.0;
							}
							if (Length + Penalty < BestLength)
							{
								BestLength = Length + Penalty;
								Best = Via;
							}
						};
						for (std::size_t Corner = 0; Corner < 4; ++Corner)
						{
							Consider({Corners[Corner]});
							Consider({Corners[Corner], Corners[(Corner + 1) % 4]});
							Consider({Corners[(Corner + 1) % 4], Corners[Corner]});
						}
						if (Best.empty())
						{
							continue;
						}
						Road.Points.insert(Road.Points.begin() + static_cast<std::ptrdiff_t>(Index + 1), Best.begin(), Best.end());
						bChanged = true;
						break;
					}
				}
				if (!bChanged)
				{
					break;
				}
			}
		}
	}

	void StatePlan::PlaceVenues()
	{
		using namespace detail_stateplan;
		auto Clear = [&](const Rect& Area)
		{
			if (!InsideState(Area.MinX, Area.MinY) || !InsideState(Area.MaxX, Area.MaxY) || !InsideState(Area.MinX, Area.MaxY) ||
				!InsideState(Area.MaxX, Area.MinY))
			{
				return false;
			}
			const Rect Padded = Expand(Area, 40.0);
			for (const StateCity& City : Cities)
			{
				if (CityRect(City).Overlaps(Padded))
				{
					return false;
				}
			}
			for (const StateLake& Lake : Lakes)
			{
				const double Nx = Clamp(Lake.X, Padded.MinX, Padded.MaxX);
				const double Ny = Clamp(Lake.Y, Padded.MinY, Padded.MaxY);
				if (std::hypot(Nx - Lake.X, Ny - Lake.Y) < Lake.RadiusM + 20.0)
				{
					return false;
				}
			}
			for (const StateRiver& River : Rivers)
			{
				for (std::size_t Index = 0; Index + 1 < River.Points.size(); ++Index)
				{
					if (SegmentHitsRect(River.Points[Index], River.Points[Index + 1], Expand(Padded, River.WidthM)))
					{
						return false;
					}
				}
			}
			for (const HighwayPlan& Road : Highways)
			{
				for (std::size_t Index = 0; Index + 1 < Road.Points.size(); ++Index)
				{
					if (SegmentHitsRect(Road.Points[Index], Road.Points[Index + 1], Expand(Area, 0.5 * Road.WidthM + 10.0)))
					{
						return false;
					}
				}
			}
			for (const StateVenue& Other : Venues)
			{
				if (Other.LevelM > 0.0 && MakeVenue(Other.Kind, Other.Name, Other.X, Other.Y).Area.Overlaps(Padded))
				{
					return false; // another venue already placed here
				}
			}
			return true;
		};
		for (StateVenue& Venue : Venues)
		{
			const double StartX = Venue.X;
			const double StartY = Venue.Y;
			bool bPlaced = Clear(MakeVenue(Venue.Kind, Venue.Name, StartX, StartY).Area);
			for (double Radius = 60.0; Radius <= 3000.0 && !bPlaced; Radius += 60.0)
			{
				for (int Angle = 0; Angle < 16 && !bPlaced; ++Angle)
				{
					const double Theta = Angle * Pi / 8.0;
					const double X = StartX + Radius * std::cos(Theta);
					const double Y = StartY + Radius * std::sin(Theta);
					if (Clear(MakeVenue(Venue.Kind, Venue.Name, X, Y).Area))
					{
						Venue.X = X;
						Venue.Y = Y;
						bPlaced = true;
					}
				}
			}
			Venue.LevelM = std::max(1.0, NaturalHeight(Venue.X, Venue.Y));
			VenueLayouts.push_back(MakeVenue(Venue.Kind, Venue.Name, Venue.X, Venue.Y));
		}
		HarmonizeLevels();

		// A connector road from each venue's entrance to the nearest highway vertex or city gate.
		for (const VenuePlan& Layout : VenueLayouts)
		{
			const Point2 Entrance = VenueEntrance(Layout);
			Point2 Best = Entrance;
			double BestDistance = 1e18;
			for (const HighwayPlan& Road : Highways)
			{
				for (const Point2& Vertex : Road.Points)
				{
					const double Distance = std::hypot(Vertex[0] - Entrance[0], Vertex[1] - Entrance[1]);
					if (Distance < BestDistance)
					{
						BestDistance = Distance;
						Best = Vertex;
					}
				}
			}
			if (BestDistance < 1e17)
			{
				HighwayPlan Connector;
				Connector.Name = Layout.Name + " Rd";
				Connector.Points = {Entrance, Best};
				Connector.WidthM = 12.0;
				Highways.push_back(Connector);
			}
		}
	}

	void StatePlan::JoinCrossings()
	{
		// Where two highways cross, both get a vertex at the crossing so the junction joins the graph.
		std::vector<HighwayPlan>& Roads = Highways;
		for (std::size_t First = 0; First < Roads.size(); ++First)
		{
			for (std::size_t Second = First + 1; Second < Roads.size(); ++Second)
			{
				for (std::size_t I = 0; I + 1 < Roads[First].Points.size(); ++I)
				{
					for (std::size_t J = 0; J + 1 < Roads[Second].Points.size(); ++J)
					{
						const Point2 P1 = Roads[First].Points[I];
						const Point2 P2 = Roads[First].Points[I + 1];
						const Point2 Q1 = Roads[Second].Points[J];
						const Point2 Q2 = Roads[Second].Points[J + 1];
						const double Rx = P2[0] - P1[0];
						const double Ry = P2[1] - P1[1];
						const double Sx = Q2[0] - Q1[0];
						const double Sy = Q2[1] - Q1[1];
						const double Cross = Rx * Sy - Ry * Sx;
						if (std::fabs(Cross) < 1e-9)
						{
							continue;
						}
						const double T = ((Q1[0] - P1[0]) * Sy - (Q1[1] - P1[1]) * Sx) / Cross;
						const double U = ((Q1[0] - P1[0]) * Ry - (Q1[1] - P1[1]) * Rx) / Cross;
						if (T <= 0.02 || T >= 0.98 || U <= 0.02 || U >= 0.98)
						{
							continue; // shared endpoints are already joined
						}
						const Point2 Hit{P1[0] + Rx * T, P1[1] + Ry * T};
						Roads[First].Points.insert(Roads[First].Points.begin() + static_cast<std::ptrdiff_t>(I + 1), Hit);
						Roads[Second].Points.insert(Roads[Second].Points.begin() + static_cast<std::ptrdiff_t>(J + 1), Hit);
						break; // the next pass of I tests the new (Hit, P2) piece
					}
				}
			}
		}
	}

	CityPlan StatePlan::BuildWorld(const CityConfig& HubTemplate) const
	{
		CityPlan World;
		World.Config = HubTemplate;
		int NextBuilding = 1;
		int NextNode = 0;
		for (const StateCity& City : Cities)
		{
			CityConfig Config = HubTemplate;
			Config.Name = City.Name;
			Config.OriginX = City.X;
			Config.OriginY = City.Y;
			Config.Style = City.Style;
			Config.bHub = City.Style == CityStyle::Hub;
			Config.bVenues = false;
			Config.GridSize = City.Grid;
			Config.Landmarks = City.Landmarks;
			Config.FirstBuildingId = NextBuilding;
			Config.FirstIntersectionId = NextNode;
			Config.Seed = HubTemplate.Seed ^ HashString(City.Name);
			CityPlan Part = GenerateCity(Config);
			CityInfo Info;
			Info.Name = City.Name;
			Info.Style = City.Style;
			Info.Bounds = Part.Bounds;
			Info.FirstBuildingId = NextBuilding;
			Info.bStart = City.bStart;
			for (const BuildingPlan& Building : Part.Buildings)
			{
				NextBuilding = std::max(NextBuilding, Building.Id + 1);
			}
			for (const Intersection& Node : Part.Intersections)
			{
				NextNode = std::max(NextNode, Node.Id + 1);
			}
			Info.LastBuildingId = NextBuilding - 1;
			if (City.bStart)
			{
				World.StarterHomeId = Part.StarterHomeId;
				World.SpawnX = Part.SpawnX;
				World.SpawnY = Part.SpawnY;
			}
			Part.Cities = {Info};
			World.Merge(Part);
		}

		// Highways join the road graph: reuse a city gate or a nearby highway node, otherwise add a node.
		std::vector<int> HighwayNodes;
		auto NodeAt = [&](const Point2& Where)
		{
			for (Intersection& Node : World.Intersections)
			{
				const bool bHighway = std::find(HighwayNodes.begin(), HighwayNodes.end(), Node.Id) != HighwayNodes.end();
				const double Snap = bHighway ? 40.0 : 1.0;
				if (std::hypot(Node.X - Where[0], Node.Y - Where[1]) <= Snap)
				{
					return Node.Id;
				}
			}
			Intersection Node;
			Node.Id = NextNode++;
			Node.X = Where[0];
			Node.Y = Where[1];
			World.Intersections.push_back(Node);
			HighwayNodes.push_back(Node.Id);
			return Node.Id;
		};
		auto Link = [&](int A, int B)
		{
			if (A == B)
			{
				return;
			}
			for (Intersection& Node : World.Intersections)
			{
				if (Node.Id == A && std::find(Node.Links.begin(), Node.Links.end(), B) == Node.Links.end()) Node.Links.push_back(B);
				if (Node.Id == B && std::find(Node.Links.begin(), Node.Links.end(), A) == Node.Links.end()) Node.Links.push_back(A);
			}
		};
		for (const HighwayPlan& Road : Highways)
		{
			int Previous = -1;
			for (const Point2& Vertex : Road.Points)
			{
				const int Current = NodeAt(Vertex);
				if (Previous >= 0)
				{
					Link(Previous, Current);
				}
				Previous = Current;
			}
		}
		World.Highways = Highways;
		World.Venues = VenueLayouts;
		World.Bounds = Bounds;
		return World;
	}
}
