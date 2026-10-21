// Procedural city plan: road grid, lots, building roles and event venues.
//
// The plan is pure data (metres, X east / Y north). ADTGWorldBuilder turns it into actors. Every building
// gets a role and every role offers at least one service, which is how "every building is interactable"
// is guaranteed and unit-tested.
#pragma once

#include "Sim/Events.h"
#include "Sim/SimCommon.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace dtg::sim
{
	enum class LotRole : int
	{
		Home,
		Apartment,
		Dealership,
		Church,
		Hospital,
		University,
		CareerCenter,
		Office,
		TechCampus,
		PowerPlant,
		GardenCenter,
		PoliceStation,
		FireStation,
		FurnitureStore,
		ModShop,
		ElectronicsStore,
		Bank,
		Diner,
		Gym,
		Library,
		Courthouse,
		Stadium,
		TrackField,
		RealEstate,
		Shop,
		School,
		Hotel,
		Museum,
		Studio,
		ClassicDealer,
		UsedCarLot,
		Jail,
		Count
	};

	const char* LotRoleName(LotRole Role);
	bool ParseLotRole(const std::string& Text, LotRole& Out);
	/** Services offered inside a building of this role (shown in its menu). Never empty. */
	std::vector<std::string> ServicesForRole(LotRole Role);
	/** Whether the role is a home the player can buy. */
	bool IsResidential(LotRole Role);

	struct Rect
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;

		double Width() const { return MaxX - MinX; }
		double Depth() const { return MaxY - MinY; }
		double CenterX() const { return 0.5 * (MinX + MaxX); }
		double CenterY() const { return 0.5 * (MinY + MaxY); }
		bool Overlaps(const Rect& Other) const
		{
			return MinX < Other.MaxX && MaxX > Other.MinX && MinY < Other.MaxY && MaxY > Other.MinY;
		}
		bool Contains(double X, double Y) const { return X >= MinX && X <= MaxX && Y >= MinY && Y <= MaxY; }
	};

	/** How a city is laid out and what fills its lots. */
	enum class CityStyle : int
	{
		Hub,     // the big regional city: towers, every landmark, the motor mile
		City,
		Town,
		Rural,   // a crossroads community: homes, a store, a church
		Beach,   // resort strip: hotels, shops, diners
		Count
	};
	const char* CityStyleName(CityStyle Style);
	bool ParseCityStyle(const std::string& Text, CityStyle& Out);

	struct BuildingPlan
	{
		int Id = 0;
		LotRole Role = LotRole::Shop;
		std::string Name;
		std::string Param;           // brand id, furniture store name, ...
		std::string Address;
		Rect Footprint;
		int Floors = 1;
		double HeightM = 4.0;
		std::uint32_t ColorRgb = 0xB0A898;
		int DoorSide = 0;            // 0 south (-Y), 1 east (+X), 2 north (+Y), 3 west (-X)
		double DoorX = 0.0;
		double DoorY = 0.0;
		bool bDowntown = false;
		/** In a crossroads community (CityStyle::Rural): single-storey, metal roofs, a yard, a drive. */
		bool bRural = false;
		/** A home's kind: 0 a brick ranch with a hip roof, 1 a siding ranch with a porch, 2 a double-wide, 3 a two-storey farmhouse. */
		int HomeStyle = 0;
		Money Price = 0;             // for homes and apartments
	};

	struct VenuePlan
	{
		EventKind Kind = EventKind::DragRace;
		std::string Name;
		Rect Area;
		std::vector<std::array<double, 2>> Path; // centreline / checkpoints
		double TrackWidthM = 14.0;
	};

	struct Intersection
	{
		int Id = 0;
		double X = 0.0;
		double Y = 0.0;
		std::vector<int> Links;
	};

	struct RoadPlan
	{
		Rect Area;
		bool bHorizontal = true;
		std::string Name;
	};

	/** A road between cities: a polyline (interstates, US and state routes, venue connectors). */
	struct HighwayPlan
	{
		std::string Name;
		std::vector<std::array<double, 2>> Points;
		double WidthM = 16.0;
		bool bInterstate = false;
		/**
		 * The graded height at each point of the polyline: what the road was cut and filled to, rather
		 * than whatever the hillside happened to be doing there. Empty until the state plan grades it.
		 */
		std::vector<double> AnchorM;
		/** Road surface height every DeckStepM metres along the polyline (bridges and their ramps included). */
		std::vector<double> DeckM;
		double DeckStepM = 5.0;
		double LengthM() const;
		/** Point and deck height at a distance along the road. */
		std::array<double, 3> At(double Distance) const;
	};

	/** A settlement in a multi-city world. */
	struct CityInfo
	{
		std::string Name;
		CityStyle Style = CityStyle::Town;
		Rect Bounds;
		int FirstBuildingId = 0;
		int LastBuildingId = 0;
		bool bStart = false;
	};

	struct CityConfig
	{
		std::string Name;
		double OriginX = 0.0;        // centre of the city in world metres
		double OriginY = 0.0;
		CityStyle Style = CityStyle::Hub;
		bool bHub = true;            // motor mile, all landmarks, required workplaces, furniture stores
		bool bVenues = true;         // event venues around the edge (single-city worlds)
		std::vector<std::pair<LotRole, std::string>> Landmarks; // extra landmarks for this city
		int FirstBuildingId = 1;
		int FirstIntersectionId = 0;
		int GridSize = 8;
		double BlockSize = 110.0;
		double RoadWidth = 18.0;
		/** The side of the town that faces the sea (0 south, 1 east, 2 north, 3 west), or -1 for none. A Beach town stands its towers on that row and nothing taller than a motel behind them. */
		int CoastSide = -1;
		double SidewalkWidth = 4.0;
		std::uint64_t Seed = 1977;
		std::vector<std::pair<std::string, std::string>> Brands; // id, display name
		std::vector<std::string> FurnitureStores;
		/** Roles that careers need as workplaces; all are guaranteed to exist. */
		std::vector<std::string> RequiredWorkplaces;
	};

	struct CityPlan
	{
		CityConfig Config;
		std::vector<RoadPlan> Roads;
		std::vector<Intersection> Intersections;
		std::vector<BuildingPlan> Buildings;
		std::vector<HighwayPlan> Highways;
		std::vector<CityInfo> Cities;  // empty for a single-city world
		std::vector<Rect> Blocks;      // every city block between the roads (sidewalk slabs), parks included
		std::vector<Rect> Parks;
		std::vector<VenuePlan> Venues;
		Rect Bounds;
		double SpawnX = 0.0;
		double SpawnY = 0.0;
		int StarterHomeId = -1;

		const BuildingPlan* FindBuilding(int Id) const;
		const BuildingPlan* FirstWithRole(LotRole Role) const;
		std::vector<const BuildingPlan*> WithRole(LotRole Role) const;
		const VenuePlan* FindVenue(EventKind Kind) const;
		/** Nearest intersection to a point (for traffic and pedestrians). */
		int NearestIntersection(double X, double Y) const;
		/** By id, in constant time: the engine layer asks this once per car per frame. */
		const Intersection* FindIntersection(int Id) const;

		/** City containing a point ("" outside every city). */
		const CityInfo* CityAt(double X, double Y) const;
		const CityInfo* FindCity(const std::string& Name) const;
		/** Appends another city's plan (ids must already be unique). */
		void Merge(const CityPlan& Other);

		// ---------------------------------------------------------------- Looking around you
		//
		// A state has thousands of buildings and road nodes, and the game asks "what's near me?" several
		// times a frame. These answer from a uniform grid rather than walking everything.

		/** Side of one lookup cell, in metres. */
		static constexpr double CellSizeM = 120.0;
		/** Builds the lookup grid. Called by GenerateCity and Merge; call it again after editing by hand. */
		void Index();
		/** Buildings whose door is within Radius of a point, nearest first. */
		std::vector<const BuildingPlan*> BuildingsNear(double X, double Y, double RadiusM) const;
		/** The nearest building door within Radius, or null. */
		const BuildingPlan* NearestBuilding(double X, double Y, double RadiusM) const;
		/** Road junctions within a radius, for anything that draws or searches a patch of the map. */
		std::vector<const Intersection*> NodesNear(double X, double Y, double RadiusM) const;

	private:
		/** Cell -> indices into Buildings and Intersections. A cell is Index(x) + Index(y) * Stride. */
		struct Grid
		{
			std::vector<std::vector<int>> Cells;
			double OriginX = 0.0;
			double OriginY = 0.0;
			int Stride = 0;
			int Rows = 0;

			bool Ready() const { return Stride > 0 && Rows > 0; }
			int CellFor(double X, double Y) const;
			/** Appends every cell index within Radius of a point. */
			void CellsNear(double X, double Y, double RadiusM, std::vector<int>& Out) const;
		};
		Grid BuildingGrid;
		Grid NodeGrid;
		/**
		 * Id to index, for the two lookups that happen every frame rather than once: which building
		 * is this, and where is this car driving to. Both were a scan of a state's worth of them.
		 */
		std::vector<int> BuildingById;
		std::vector<int> NodeById;
	};

	CityPlan GenerateCity(const CityConfig& Config);
	/** An event venue laid out around a centre point (drag strip, derby arena, monster stadium, road course). */
	VenuePlan MakeVenue(EventKind Kind, const std::string& Name, double CenterX, double CenterY);
	/** Where cars drive in from (the end a connector road joins). */
	std::array<double, 2> VenueEntrance(const VenuePlan& Venue);
}
