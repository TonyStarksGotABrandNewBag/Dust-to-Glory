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

	struct CityConfig
	{
		int GridSize = 8;
		double BlockSize = 110.0;
		double RoadWidth = 18.0;
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
	};

	CityPlan GenerateCity(const CityConfig& Config);
}
