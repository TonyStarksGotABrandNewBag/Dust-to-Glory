#include "Sim/CityLayout.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	namespace detail_city
	{
		struct RoleInfo
		{
			LotRole Role;
			const char* Name;
			std::uint32_t Color;
		};

		// Muted, believable facade colours for the graybox.
		constexpr RoleInfo Roles[] = {
			{LotRole::Home, "Home", 0xD8CBB0},
			{LotRole::Apartment, "Apartment", 0xB9A38C},
			{LotRole::Dealership, "Dealership", 0xC8CED6},
			{LotRole::Church, "Church", 0xE6E1D6},
			{LotRole::Hospital, "Hospital", 0xE9ECEF},
			{LotRole::University, "University", 0xA9846A},
			{LotRole::CareerCenter, "CareerCenter", 0x9FB3C8},
			{LotRole::Office, "Office", 0x7E93A8},
			{LotRole::TechCampus, "TechCampus", 0x8FA7B5},
			{LotRole::PowerPlant, "PowerPlant", 0x8C8C84},
			{LotRole::GardenCenter, "GardenCenter", 0x9DB58A},
			{LotRole::PoliceStation, "PoliceStation", 0x6E7C91},
			{LotRole::FireStation, "FireStation", 0xB0524A},
			{LotRole::FurnitureStore, "FurnitureStore", 0xC9B79C},
			{LotRole::ModShop, "ModShop", 0x5A5F66},
			{LotRole::ElectronicsStore, "ElectronicsStore", 0x6F7F8F},
			{LotRole::Bank, "Bank", 0xD2C7B1},
			{LotRole::Diner, "Diner", 0xC9876E},
			{LotRole::Gym, "Gym", 0x7A7F86},
			{LotRole::Library, "Library", 0xB8A07E},
			{LotRole::Courthouse, "Courthouse", 0xDDD5C3},
			{LotRole::Stadium, "Stadium", 0x9AA2AA},
			{LotRole::TrackField, "TrackField", 0xA8473F},
			{LotRole::RealEstate, "RealEstate", 0xBFB39B},
			{LotRole::Shop, "Shop", 0xC2B49C},
			{LotRole::School, "School", 0xB96F55},
			{LotRole::Hotel, "Hotel", 0xA59A8D},
			{LotRole::Museum, "Museum", 0xE2DCCF},
			{LotRole::Studio, "Studio", 0x5E5866},
			{LotRole::ClassicDealer, "ClassicDealer", 0x8D6E63},
			{LotRole::UsedCarLot, "UsedCarLot", 0xB0A47F},
			{LotRole::Jail, "Jail", 0x8A8D91},
		};

		const RoleInfo& Info(LotRole Role)
		{
			for (const RoleInfo& Entry : Roles)
			{
				if (Entry.Role == Role)
				{
					return Entry;
				}
			}
			return Roles[0];
		}

		const std::vector<std::string>& AvenueNames()
		{
			static const std::vector<std::string> Names = {
				"Harbor Rd", "Maple Ave", "Oak Ave", "Cedar Ave", "Elm Ave", "Birch Ave", "Willow Ave", "Aspen Ave",
				"Spruce Ave", "Juniper Ave", "Laurel Ave", "Magnolia Ave", "Sycamore Ave", "Hawthorn Ave", "Chestnut Ave", "Summit Rd", "Ridge Rd"};
			return Names;
		}

		const std::vector<std::string>& StreetNames()
		{
			static const std::vector<std::string> Names = {
				"1st St", "2nd St", "3rd St", "4th St", "5th St", "6th St", "7th St", "8th St", "9th St",
				"10th St", "11th St", "12th St", "13th St", "14th St", "15th St", "16th St", "17th St"};
			return Names;
		}

		const std::vector<std::string>& Companies()
		{
			static const std::vector<std::string> Names = {
				"Brightwater Logistics", "Tidewell Health", "Northgate Transit", "Sable Media", "Keystone Utilities",
				"Summit Insurance", "Cardinal Freight", "Lakeside Partners", "Atlas Engineering", "Beacon Legal",
				"Pioneer Analytics", "Harborview Capital", "Copperline Telecom", "Evergreen Architects"};
			return Names;
		}

		const std::vector<std::string>& ShopNames()
		{
			static const std::vector<std::string> Names = {
				"Corner Market", "Pine Street Books", "Sunrise Bakery", "Tidewater Outfitters", "Velvet Music",
				"Main Street Pharmacy", "Bloom & Stem Florist", "Two Wheels Bicycles", "Lucky Paws Pet Supply",
				"Union Hardware", "Silver Spoon Cafe", "Harbor Thrift", "Starlight Arcade", "Fresh Fold Laundry"};
			return Names;
		}

		enum class Zone
		{
			Downtown,
			Mid,
			Outer
		};

		struct Lot
		{
			Rect Area;
			Zone Where = Zone::Outer;
			int BlockI = 0;
			int BlockJ = 0;
			int SubA = 0;
			int SubB = 0;
			bool bMotorMile = false;
			bool bTaken = false;
		};

		struct Required
		{
			LotRole Role;
			Zone Preferred;
			const char* Name;
		};
	}

	const char* LotRoleName(LotRole Role)
	{
		return detail_city::Info(Role).Name;
	}

	const char* CityStyleName(CityStyle Style)
	{
		switch (Style)
		{
		case CityStyle::Hub: return "Hub";
		case CityStyle::City: return "City";
		case CityStyle::Town: return "Town";
		case CityStyle::Rural: return "Rural";
		case CityStyle::Beach: return "Beach";
		case CityStyle::Count: break;
		}
		return "Town";
	}

	bool ParseCityStyle(const std::string& Text, CityStyle& Out)
	{
		for (int Index = 0; Index < static_cast<int>(CityStyle::Count); ++Index)
		{
			if (ToLowerAscii(CityStyleName(static_cast<CityStyle>(Index))) == ToLowerAscii(Text))
			{
				Out = static_cast<CityStyle>(Index);
				return true;
			}
		}
		return false;
	}

	bool ParseLotRole(const std::string& Text, LotRole& Out)
	{
		const std::string Lower = ToLowerAscii(Text);
		for (const auto& Entry : detail_city::Roles)
		{
			if (ToLowerAscii(Entry.Name) == Lower)
			{
				Out = Entry.Role;
				return true;
			}
		}
		return false;
	}

	bool IsResidential(LotRole Role)
	{
		return Role == LotRole::Home || Role == LotRole::Apartment;
	}

	std::vector<std::string> ServicesForRole(LotRole Role)
	{
		switch (Role)
		{
		case LotRole::Home: return {"Visit", "Buy or enter home", "Arrange furniture"};
		case LotRole::Apartment: return {"Visit a neighbor", "Buy or enter apartment", "Arrange furniture"};
		case LotRole::Dealership: return {"Browse every model", "Test drive", "Used car lot", "Sell a car"};
		case LotRole::Church: return {"Ministry office", "Attend service", "Volunteer"};
		case LotRole::Hospital: return {"Work a shift", "Get treated", "Research a medicine", "Recruit a lab contact"};
		case LotRole::University: return {"Enroll", "Attend class", "Work a shift", "Research a medicine"};
		case LotRole::CareerCenter: return {"Job board", "Career advice"};
		case LotRole::Office: return {"Work a shift", "Hacking contracts", "Business consulting"};
		case LotRole::TechCampus: return {"Work a shift", "Hacking contracts", "Create IP"};
		case LotRole::PowerPlant: return {"Work a shift"};
		case LotRole::GardenCenter: return {"Work a shift", "Shop plants"};
		case LotRole::PoliceStation: return {"Work a shift", "Pay fines"};
		case LotRole::FireStation: return {"Work a shift"};
		case LotRole::FurnitureStore: return {"Shop furniture", "Track deliveries"};
		case LotRole::ModShop: return {"Parts and tuning", "Paint shop", "Dyno", "DIY garage"};
		case LotRole::ElectronicsStore: return {"Hacking tools", "Gadgets"};
		case LotRole::Bank: return {"Account summary", "Stock exchange", "Work a shift"};
		case LotRole::Diner: return {"Eat", "Work a shift"};
		case LotRole::Gym: return {"Train", "Work a shift"};
		case LotRole::Library: return {"Study", "Create IP"};
		case LotRole::Courthouse: return {"Run for office", "Legislature", "Public records", "Turn yourself in", "Work a shift"};
		case LotRole::Stadium: return {"Tryouts and games", "Watch a game"};
		case LotRole::TrackField: return {"Train", "Compete"};
		case LotRole::RealEstate: return {"Browse homes", "Buy a business"};
		case LotRole::Shop: return {"Browse", "Work a shift"};
		case LotRole::School: return {"Work a shift", "Volunteer"};
		case LotRole::Hotel: return {"Rent a room", "Work a shift"};
		case LotRole::Museum: return {"Explore exhibits"};
		case LotRole::Studio: return {"Create IP", "Work a shift"};
		case LotRole::ClassicDealer: return {"Browse classics", "Restoration shop", "Sell a car"};
		case LotRole::UsedCarLot: return {"Browse used cars", "Sell a car"};
		case LotRole::Jail: return {"Visit", "Serve your time", "Plan a way out", "Court and records"};
		case LotRole::Count: break;
		}
		return {"Visit"};
	}

	const BuildingPlan* CityPlan::FindBuilding(int Id) const
	{
		for (const BuildingPlan& Building : Buildings)
		{
			if (Building.Id == Id)
			{
				return &Building;
			}
		}
		return nullptr;
	}

	const BuildingPlan* CityPlan::FirstWithRole(LotRole Role) const
	{
		for (const BuildingPlan& Building : Buildings)
		{
			if (Building.Role == Role)
			{
				return &Building;
			}
		}
		return nullptr;
	}

	std::vector<const BuildingPlan*> CityPlan::WithRole(LotRole Role) const
	{
		std::vector<const BuildingPlan*> Result;
		for (const BuildingPlan& Building : Buildings)
		{
			if (Building.Role == Role)
			{
				Result.push_back(&Building);
			}
		}
		return Result;
	}

	const VenuePlan* CityPlan::FindVenue(EventKind Kind) const
	{
		for (const VenuePlan& Venue : Venues)
		{
			if (Venue.Kind == Kind)
			{
				return &Venue;
			}
		}
		return nullptr;
	}

	int CityPlan::Grid::CellFor(double X, double Y) const
	{
		if (!Ready())
		{
			return -1;
		}
		const int Column = static_cast<int>(std::floor((X - OriginX) / CellSizeM));
		const int Row = static_cast<int>(std::floor((Y - OriginY) / CellSizeM));
		if (Column < 0 || Row < 0 || Column >= Stride || Row >= Rows)
		{
			return -1;
		}
		return Column + Row * Stride;
	}

	void CityPlan::Grid::CellsNear(double X, double Y, double RadiusM, std::vector<int>& Out) const
	{
		Out.clear();
		if (!Ready())
		{
			return;
		}
		const int Reach = static_cast<int>(std::ceil(RadiusM / CellSizeM));
		const int Column = static_cast<int>(std::floor((X - OriginX) / CellSizeM));
		const int Row = static_cast<int>(std::floor((Y - OriginY) / CellSizeM));
		for (int J = Row - Reach; J <= Row + Reach; ++J)
		{
			if (J < 0 || J >= Rows)
			{
				continue;
			}
			for (int I = Column - Reach; I <= Column + Reach; ++I)
			{
				if (I >= 0 && I < Stride)
				{
					Out.push_back(I + J * Stride);
				}
			}
		}
	}

	void CityPlan::Index()
	{
		// One pass over everything, so the thousands of "what's near me?" questions after it are cheap.
		auto Setup = [this](Grid& Target)
		{
			Target.OriginX = Bounds.MinX - CellSizeM;
			Target.OriginY = Bounds.MinY - CellSizeM;
			Target.Stride = std::max(1, static_cast<int>(std::ceil((Bounds.Width() + 2.0 * CellSizeM) / CellSizeM)));
			Target.Rows = std::max(1, static_cast<int>(std::ceil((Bounds.Depth() + 2.0 * CellSizeM) / CellSizeM)));
			Target.Cells.assign(static_cast<std::size_t>(Target.Stride) * static_cast<std::size_t>(Target.Rows), {});
		};
		if (Bounds.Width() <= 0.0 || Bounds.Depth() <= 0.0)
		{
			BuildingGrid = Grid();
			NodeGrid = Grid();
			return;
		}
		Setup(BuildingGrid);
		Setup(NodeGrid);
		for (std::size_t Which = 0; Which < Buildings.size(); ++Which)
		{
			const int Cell = BuildingGrid.CellFor(Buildings[Which].DoorX, Buildings[Which].DoorY);
			if (Cell >= 0)
			{
				BuildingGrid.Cells[static_cast<std::size_t>(Cell)].push_back(static_cast<int>(Which));
			}
		}
		for (std::size_t Which = 0; Which < Intersections.size(); ++Which)
		{
			const int Cell = NodeGrid.CellFor(Intersections[Which].X, Intersections[Which].Y);
			if (Cell >= 0)
			{
				NodeGrid.Cells[static_cast<std::size_t>(Cell)].push_back(static_cast<int>(Which));
			}
		}
	}

	std::vector<const BuildingPlan*> CityPlan::BuildingsNear(double X, double Y, double RadiusM) const
	{
		std::vector<std::pair<double, const BuildingPlan*>> Found;
		const double Reach = RadiusM * RadiusM;
		auto Consider = [&](const BuildingPlan& Building)
		{
			const double Dx = Building.DoorX - X;
			const double Dy = Building.DoorY - Y;
			const double Distance = Dx * Dx + Dy * Dy;
			if (Distance <= Reach)
			{
				Found.emplace_back(Distance, &Building);
			}
		};
		if (BuildingGrid.Ready())
		{
			std::vector<int> Cells;
			BuildingGrid.CellsNear(X, Y, RadiusM, Cells);
			for (const int Cell : Cells)
			{
				for (const int Which : BuildingGrid.Cells[static_cast<std::size_t>(Cell)])
				{
					Consider(Buildings[static_cast<std::size_t>(Which)]);
				}
			}
		}
		else
		{
			// No index (a hand-built plan in a test): correct, just slower.
			for (const BuildingPlan& Building : Buildings)
			{
				Consider(Building);
			}
		}
		std::sort(Found.begin(), Found.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		std::vector<const BuildingPlan*> Out;
		Out.reserve(Found.size());
		for (const auto& Entry : Found)
		{
			Out.push_back(Entry.second);
		}
		return Out;
	}

	const BuildingPlan* CityPlan::NearestBuilding(double X, double Y, double RadiusM) const
	{
		const std::vector<const BuildingPlan*> Near = BuildingsNear(X, Y, RadiusM);
		return Near.empty() ? nullptr : Near.front();
	}

	int CityPlan::NearestIntersection(double X, double Y) const
	{
		int Best = -1;
		double BestDistance = 1e18;
		auto Consider = [&](const Intersection& Node)
		{
			const double Dx = Node.X - X;
			const double Dy = Node.Y - Y;
			const double Distance = Dx * Dx + Dy * Dy;
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Node.Id;
			}
		};
		if (NodeGrid.Ready())
		{
			// Widen the search until something turns up: out in the country the nearest junction is far.
			std::vector<int> Cells;
			for (double Radius = CellSizeM; Radius < Bounds.Width() + Bounds.Depth(); Radius *= 2.0)
			{
				NodeGrid.CellsNear(X, Y, Radius, Cells);
				for (const int Cell : Cells)
				{
					for (const int Which : NodeGrid.Cells[static_cast<std::size_t>(Cell)])
					{
						Consider(Intersections[static_cast<std::size_t>(Which)]);
					}
				}
				// A hit inside the searched radius is the nearest; anything outside it might not be.
				if (Best >= 0 && BestDistance <= Radius * Radius)
				{
					return Best;
				}
			}
		}
		for (const Intersection& Node : Intersections)
		{
			Consider(Node);
		}
		return Best;
	}

	const Intersection* CityPlan::FindIntersection(int Id) const
	{
		for (const Intersection& Node : Intersections)
		{
			if (Node.Id == Id)
			{
				return &Node;
			}
		}
		return nullptr;
	}

	const CityInfo* CityPlan::CityAt(double X, double Y) const
	{
		for (const CityInfo& Info : Cities)
		{
			if (Info.Bounds.Contains(X, Y))
			{
				return &Info;
			}
		}
		return nullptr;
	}

	const CityInfo* CityPlan::FindCity(const std::string& Name) const
	{
		for (const CityInfo& Info : Cities)
		{
			if (Info.Name == Name)
			{
				return &Info;
			}
		}
		return nullptr;
	}

	void CityPlan::Merge(const CityPlan& Other)
	{
		const bool bEmpty = Buildings.empty() && Roads.empty();
		Roads.insert(Roads.end(), Other.Roads.begin(), Other.Roads.end());
		Intersections.insert(Intersections.end(), Other.Intersections.begin(), Other.Intersections.end());
		Buildings.insert(Buildings.end(), Other.Buildings.begin(), Other.Buildings.end());
		Blocks.insert(Blocks.end(), Other.Blocks.begin(), Other.Blocks.end());
		Parks.insert(Parks.end(), Other.Parks.begin(), Other.Parks.end());
		Venues.insert(Venues.end(), Other.Venues.begin(), Other.Venues.end());
		Highways.insert(Highways.end(), Other.Highways.begin(), Other.Highways.end());
		Cities.insert(Cities.end(), Other.Cities.begin(), Other.Cities.end());
		if (bEmpty)
		{
			Bounds = Other.Bounds;
		}
		else
		{
			Bounds.MinX = std::min(Bounds.MinX, Other.Bounds.MinX);
			Bounds.MinY = std::min(Bounds.MinY, Other.Bounds.MinY);
			Bounds.MaxX = std::max(Bounds.MaxX, Other.Bounds.MaxX);
			Bounds.MaxY = std::max(Bounds.MaxY, Other.Bounds.MaxY);
		}
		Index();
	}

	double HighwayPlan::LengthM() const
	{
		double Total = 0.0;
		for (std::size_t Index = 0; Index + 1 < Points.size(); ++Index)
		{
			Total += std::hypot(Points[Index + 1][0] - Points[Index][0], Points[Index + 1][1] - Points[Index][1]);
		}
		return Total;
	}

	std::array<double, 3> HighwayPlan::At(double Distance) const
	{
		std::array<double, 3> Out{0.0, 0.0, 0.0};
		if (Points.empty())
		{
			return Out;
		}
		Out = {Points.front()[0], Points.front()[1], 0.0};
		double Walked = 0.0;
		for (std::size_t Index = 0; Index + 1 < Points.size(); ++Index)
		{
			const double Length = std::hypot(Points[Index + 1][0] - Points[Index][0], Points[Index + 1][1] - Points[Index][1]);
			const double T = Length > 1e-9 ? Clamp((Distance - Walked) / Length, 0.0, 1.0) : 1.0;
			Out[0] = Points[Index][0] + (Points[Index + 1][0] - Points[Index][0]) * T;
			Out[1] = Points[Index][1] + (Points[Index + 1][1] - Points[Index][1]) * T;
			Walked += Length;
			if (Walked >= Distance)
			{
				break;
			}
		}
		if (!DeckM.empty())
		{
			const double Slot = Clamp(Distance / DeckStepM, 0.0, static_cast<double>(DeckM.size() - 1));
			const std::size_t Low = static_cast<std::size_t>(Slot);
			const std::size_t High = std::min(Low + 1, DeckM.size() - 1);
			Out[2] = DeckM[Low] + (DeckM[High] - DeckM[Low]) * (Slot - static_cast<double>(Low));
		}
		return Out;
	}

	CityPlan GenerateCity(const CityConfig& InConfig)
	{
		using namespace detail_city;
		CityPlan Plan;
		Plan.Config = InConfig;
		const CityConfig& Config = Plan.Config;
		Rng Random(Config.Seed);

		const int Grid = std::max(2, Config.GridSize);
		const double Road = Config.RoadWidth;
		const double Block = Config.BlockSize;
		const double Pitch = Block + Road;
		const double Span = Grid * Pitch + Road;
		const double OriginX = Config.OriginX - 0.5 * Span;
		const double OriginY = Config.OriginY - 0.5 * Span;
		Plan.Bounds = Rect{OriginX, OriginY, OriginX + Span, OriginY + Span};

		// Roads and intersections.
		for (int K = 0; K <= Grid; ++K)
		{
			const double LineX = OriginX + K * Pitch;
			const double LineY = OriginY + K * Pitch;
			RoadPlan Vertical;
			Vertical.Area = Rect{LineX, OriginY, LineX + Road, OriginY + Span};
			Vertical.bHorizontal = false;
			Vertical.Name = StreetNames()[static_cast<std::size_t>(K) % StreetNames().size()];
			Plan.Roads.push_back(Vertical);
			RoadPlan Horizontal;
			Horizontal.Area = Rect{OriginX, LineY, OriginX + Span, LineY + Road};
			Horizontal.bHorizontal = true;
			Horizontal.Name = AvenueNames()[static_cast<std::size_t>(K) % AvenueNames().size()];
			Plan.Roads.push_back(Horizontal);
		}
		for (int L = 0; L <= Grid; ++L)
		{
			for (int K = 0; K <= Grid; ++K)
			{
				Intersection Node;
				Node.Id = Config.FirstIntersectionId + L * (Grid + 1) + K;
				Node.X = OriginX + K * Pitch + 0.5 * Road;
				Node.Y = OriginY + L * Pitch + 0.5 * Road;
				if (K > 0) Node.Links.push_back(Node.Id - 1);
				if (K < Grid) Node.Links.push_back(Node.Id + 1);
				if (L > 0) Node.Links.push_back(Node.Id - (Grid + 1));
				if (L < Grid) Node.Links.push_back(Node.Id + (Grid + 1));
				Plan.Intersections.push_back(Node);
			}
		}

		// Parks: a few blocks of open green space away from downtown and the motor mile.
		const double Center = 0.5 * (Grid - 1);
		const CityStyle Style = Config.Style;
		auto ZoneOf = [Center, Style](int I, int J)
		{
			const double Distance = std::max(std::fabs(I - Center), std::fabs(J - Center));
			switch (Style)
			{
			case CityStyle::Rural: return Zone::Outer;
			case CityStyle::Town: return Distance <= 0.7 ? Zone::Mid : Zone::Outer;
			case CityStyle::City: return Distance <= 0.5 ? Zone::Downtown : (Distance <= 1.5 ? Zone::Mid : Zone::Outer);
			default: break;
			}
			if (Distance <= 1.0) return Zone::Downtown;
			if (Distance <= 2.5) return Zone::Mid;
			return Zone::Outer;
		};
		std::vector<std::pair<int, int>> ParkBlocks;
		int ParkAttempts = 0;
		std::size_t ParkTarget = 3;
		switch (Style)
		{
		case CityStyle::City: ParkTarget = 2; break;
		case CityStyle::Town:
		case CityStyle::Beach: ParkTarget = 1; break;
		case CityStyle::Rural: ParkTarget = 0; break;
		default: break;
		}
		while (ParkBlocks.size() < ParkTarget && ParkAttempts < 200)
		{
			++ParkAttempts;
			const int I = Random.RangeInt(0, Grid - 1);
			const int J = Random.RangeInt(1, Grid - 1);
			if (ZoneOf(I, J) == Zone::Downtown)
			{
				continue;
			}
			bool bDuplicate = false;
			for (const auto& Existing : ParkBlocks)
			{
				bDuplicate = bDuplicate || (std::abs(Existing.first - I) <= 1 && std::abs(Existing.second - J) <= 1);
			}
			if (!bDuplicate)
			{
				ParkBlocks.emplace_back(I, J);
			}
		}

		// Lots: four per block.
		std::vector<Lot> Lots;
		const double Sidewalk = Config.SidewalkWidth;
		const double Alley = 4.0;
		const double LotSize = 0.5 * (Block - 2.0 * Sidewalk - Alley);
		for (int J = 0; J < Grid; ++J)
		{
			for (int I = 0; I < Grid; ++I)
			{
				const double BlockMinX = OriginX + I * Pitch + Road;
				const double BlockMinY = OriginY + J * Pitch + Road;
				Plan.Blocks.push_back(Rect{BlockMinX, BlockMinY, BlockMinX + Block, BlockMinY + Block});
				bool bPark = false;
				for (const auto& Park : ParkBlocks)
				{
					bPark = bPark || (Park.first == I && Park.second == J);
				}
				if (bPark)
				{
					Plan.Parks.push_back(Rect{BlockMinX, BlockMinY, BlockMinX + Block, BlockMinY + Block});
					continue;
				}
				for (int B = 0; B < 2; ++B)
				{
					for (int A = 0; A < 2; ++A)
					{
						Lot Entry;
						const double X0 = BlockMinX + Sidewalk + A * (LotSize + Alley);
						const double Y0 = BlockMinY + Sidewalk + B * (LotSize + Alley);
						Entry.Area = Rect{X0, Y0, X0 + LotSize, Y0 + LotSize};
						Entry.Where = ZoneOf(I, J);
						Entry.BlockI = I;
						Entry.BlockJ = J;
						Entry.SubA = A;
						Entry.SubB = B;
						Entry.bMotorMile = Config.bHub && J == 0 && I >= 1 && I <= Grid - 2;
						Lots.push_back(Entry);
					}
				}
			}
		}

		int NextId = Config.FirstBuildingId;
		auto MakeBuilding = [&](Lot& Where, LotRole Role, const std::string& Name, const std::string& Param)
		{
			Where.bTaken = true;
			BuildingPlan Building;
			Building.Id = NextId++;
			Building.Role = Role;
			Building.Name = Name;
			Building.Param = Param;
			Building.bDowntown = Where.Where == Zone::Downtown;
			Building.ColorRgb = Info(Role).Color;

			// Footprint: shrink the lot by a setback; homes are smaller and sit toward the street.
			double Setback = Where.Where == Zone::Downtown ? 1.5 : Random.Range(3.0, 6.0);
			Rect Area = Where.Area;
			Area.MinX += Setback;
			Area.MaxX -= Setback;
			Area.MinY += Setback;
			Area.MaxY -= Setback;
			if (Role == LotRole::Home)
			{
				const double Width = Random.Range(14.0, 20.0);
				const double Depth = Random.Range(12.0, 16.0);
				const double Cx = Area.CenterX();
				Area.MinX = Cx - 0.5 * Width;
				Area.MaxX = Cx + 0.5 * Width;
				if (Where.SubB == 0)
				{
					Area.MaxY = Area.MinY + Depth;
				}
				else
				{
					Area.MinY = Area.MaxY - Depth;
				}
			}
			Building.Footprint = Area;

			int Floors = 1;
			switch (Where.Where)
			{
			case Zone::Downtown: Floors = Random.RangeInt(6, 22); break;
			case Zone::Mid: Floors = Random.RangeInt(2, 6); break;
			case Zone::Outer: Floors = Random.RangeInt(1, 2); break;
			}
			if (Role == LotRole::Home) Floors = Random.RangeInt(1, 2);
			if (Role == LotRole::Dealership || Role == LotRole::ModShop || Role == LotRole::GardenCenter || Role == LotRole::FurnitureStore) Floors = 1;
			if (Role == LotRole::Stadium || Role == LotRole::TrackField || Role == LotRole::PowerPlant) Floors = 3;
			if (Role == LotRole::Church) Floors = 2;
			Building.Floors = Floors;
			Building.HeightM = Floors * 3.6 + 1.0;

			// Doors face the avenue on the lot's outer edge.
			Building.DoorSide = Where.SubB == 0 ? 0 : 2;
			Building.DoorX = Area.CenterX();
			Building.DoorY = Where.SubB == 0 ? Area.MinY : Area.MaxY;
			const int AvenueIndex = Where.SubB == 0 ? Where.BlockJ : Where.BlockJ + 1;
			const int Number = 100 * (Where.BlockI + 1) + 20 * Where.SubA + 2 * (Where.SubB + 1);
			Building.Address = std::to_string(Number) + " " + AvenueNames()[static_cast<std::size_t>(AvenueIndex) % AvenueNames().size()];
			if (!Config.Name.empty())
			{
				Building.Address += ", " + Config.Name;
			}

			if (IsResidential(Role))
			{
				const double Area2 = Area.Width() * Area.Depth();
				const double ZoneFactor = Where.Where == Zone::Downtown ? 1.6 : (Where.Where == Zone::Mid ? 1.2 : 1.0);
				// Mid-1990s prices: a family house runs roughly $90k-$150k, a condo unit a little less.
				const double Base = Role == LotRole::Home ? 420.0 * Area2 : 55.0 * Area2;
				Building.Price = ToMoney(std::round(Base * ZoneFactor * Random.Range(0.85, 1.15) / 1000.0) * 1000.0);
				if (Building.Name.empty())
				{
					Building.Name = Building.Address;
				}
			}
			Plan.Buildings.push_back(Building);
			return Plan.Buildings.back().Id;
		};

		auto FindLot = [&](Zone Preferred, bool bAllowMotorMile) -> Lot*
		{
			std::vector<Lot*> Candidates;
			for (Lot& Entry : Lots)
			{
				if (!Entry.bTaken && Entry.Where == Preferred && (bAllowMotorMile || !Entry.bMotorMile))
				{
					Candidates.push_back(&Entry);
				}
			}
			if (Candidates.empty())
			{
				for (Lot& Entry : Lots)
				{
					if (!Entry.bTaken && (bAllowMotorMile || !Entry.bMotorMile))
					{
						Candidates.push_back(&Entry);
					}
				}
			}
			if (Candidates.empty())
			{
				return nullptr;
			}
			return Candidates[Random.Index(Candidates.size())];
		};

		// 1. Motor Mile: a showroom for every brand along the south avenue.
		if (Config.bHub)
		{
			std::vector<Lot*> Mile;
			for (Lot& Entry : Lots)
			{
				if (Entry.bMotorMile)
				{
					Mile.push_back(&Entry);
				}
			}
			std::sort(Mile.begin(), Mile.end(), [](const Lot* A, const Lot* B)
			{
				if (A->SubB != B->SubB) return A->SubB < B->SubB;
				return A->Area.MinX < B->Area.MinX;
			});
			std::size_t Next = 0;
			for (const auto& BrandEntry : Config.Brands)
			{
				Lot* Where = Next < Mile.size() ? Mile[Next++] : FindLot(Zone::Mid, true);
				if (Where)
				{
					MakeBuilding(*Where, LotRole::Dealership, BrandEntry.second + " Showroom", BrandEntry.first);
				}
			}
		}

		// 2. Civic and commercial landmarks.
		const Required Landmarks[] = {
			{LotRole::CareerCenter, Zone::Downtown, "City Career Center"},
			{LotRole::Bank, Zone::Downtown, "Coastline Savings Bank"},
			{LotRole::Courthouse, Zone::Downtown, "County Courthouse"},
			{LotRole::Hotel, Zone::Downtown, "Ocean Crest Hotel"},
			{LotRole::TechCampus, Zone::Mid, "Orchard Robotics Campus"},
			{LotRole::Hospital, Zone::Mid, "Mercy General Hospital"},
			{LotRole::University, Zone::Mid, "Palmetto Coast University"},
			{LotRole::Library, Zone::Mid, "City Library"},
			{LotRole::PoliceStation, Zone::Mid, "Central Police Precinct"},
			{LotRole::FireStation, Zone::Mid, "Fire Station 7"},
			{LotRole::Church, Zone::Outer, "Grace Harbor Church"},
			{LotRole::School, Zone::Outer, "Riverside High School"},
			{LotRole::Gym, Zone::Mid, "Iron Temple Gym"},
			{LotRole::Diner, Zone::Mid, "Route 9 Diner"},
			{LotRole::Diner, Zone::Outer, "Blue Plate Diner"},
			{LotRole::ModShop, Zone::Mid, "Ironworks Customs"},
			{LotRole::ElectronicsStore, Zone::Mid, "Circuit Depot"},
			{LotRole::RealEstate, Zone::Mid, "Keyline Realty"},
			{LotRole::Stadium, Zone::Outer, "Glory Field"},
			{LotRole::TrackField, Zone::Outer, "Olympic Training Center"},
			{LotRole::PowerPlant, Zone::Outer, "Keystone Power Station"},
			{LotRole::GardenCenter, Zone::Outer, "Greenleaf Garden Center"},
			{LotRole::Museum, Zone::Downtown, "City Museum of Science"},
			{LotRole::Studio, Zone::Mid, "Soundwave Studios"},
			{LotRole::ClassicDealer, Zone::Mid, "Heritage Motor Classics"},
			{LotRole::UsedCarLot, Zone::Outer, "Second Wind Used Cars"},
		};
		if (Config.bHub)
		{
			for (const Required& Entry : Landmarks)
			{
				if (Lot* Where = FindLot(Entry.Preferred, false))
				{
					MakeBuilding(*Where, Entry.Role, Entry.Name, std::string());
				}
			}
		}
		for (const auto& Entry : Config.Landmarks)
		{
			if (Lot* Where = FindLot(Zone::Mid, false))
			{
				MakeBuilding(*Where, Entry.first, Entry.second, std::string());
			}
		}

		// 3. Any workplace the career data needs that is still missing.
		for (const std::string& RoleName : Config.bHub ? Config.RequiredWorkplaces : std::vector<std::string>())
		{
			LotRole Role = LotRole::Office;
			if (ParseLotRole(RoleName, Role) && !Plan.FirstWithRole(Role))
			{
				if (Lot* Where = FindLot(Zone::Mid, false))
				{
					MakeBuilding(*Where, Role, RoleName, std::string());
				}
			}
		}

		// 4. Furniture stores.
		for (const std::string& Store : Config.bHub ? Config.FurnitureStores : std::vector<std::string>())
		{
			if (Lot* Where = FindLot(Zone::Mid, false))
			{
				MakeBuilding(*Where, LotRole::FurnitureStore, Store, Store);
			}
		}

		// 5. Fill every remaining lot so that no lot is empty.
		std::size_t CompanyIndex = 0;
		std::size_t ShopIndex = 0;
		for (Lot& Entry : Lots)
		{
			if (Entry.bTaken)
			{
				continue;
			}
			const double Roll = Random.NextDouble();
			LotRole Role = LotRole::Home;
			if (Style == CityStyle::Rural)
			{
				Role = Roll < 0.75 ? LotRole::Home : (Roll < 0.87 ? LotRole::Shop : (Roll < 0.93 ? LotRole::Diner : LotRole::Church));
			}
			else if (Style == CityStyle::Beach && Entry.Where != Zone::Outer)
			{
				Role = Roll < 0.3 ? LotRole::Hotel : (Roll < 0.5 ? LotRole::Shop : (Roll < 0.65 ? LotRole::Diner : (Roll < 0.9 ? LotRole::Apartment : LotRole::Home)));
			}
			else if (Style == CityStyle::Town && Entry.Where == Zone::Outer)
			{
				Role = Roll < 0.6 ? LotRole::Home : (Roll < 0.7 ? LotRole::Apartment : (Roll < 0.85 ? LotRole::Shop : (Roll < 0.93 ? LotRole::Diner : LotRole::Church)));
			}
			else if (Entry.bMotorMile)
			{
				Role = Roll < 0.5 ? LotRole::Shop : LotRole::Office;
			}
			else if (Entry.Where == Zone::Downtown)
			{
				Role = Roll < 0.5 ? LotRole::Office : (Roll < 0.7 ? LotRole::Shop : (Roll < 0.8 ? LotRole::Hotel : LotRole::Apartment));
			}
			else if (Entry.Where == Zone::Mid)
			{
				Role = Roll < 0.35 ? LotRole::Apartment : (Roll < 0.6 ? LotRole::Shop : (Roll < 0.75 ? LotRole::Office : (Roll < 0.85 ? LotRole::Diner : LotRole::Home)));
			}
			else
			{
				Role = Roll < 0.65 ? LotRole::Home : (Roll < 0.8 ? LotRole::Apartment : (Roll < 0.95 ? LotRole::Shop : LotRole::Diner));
			}
			std::string Name;
			switch (Role)
			{
			case LotRole::Office: Name = Companies()[CompanyIndex++ % Companies().size()] + (Entry.Where == Zone::Downtown ? " Tower" : " Offices"); break;
			case LotRole::Shop: Name = ShopNames()[ShopIndex++ % ShopNames().size()]; break;
			case LotRole::Hotel:
			{
				static const char* const Inns[] = {"Harborlight Inn", "Sea Breeze Resort", "Sandpiper Suites", "Driftwood Motel", "Palm Court Resort",
					"Tidewater Inn", "Seaspray Tower", "Dune Crest Hotel"};
				Name = Inns[Random.Index(8)];
				break;
			}
			case LotRole::Church: Name = (Config.Name.empty() ? std::string("Community") : Config.Name) + " Community Church"; break;
			case LotRole::Diner: Name = "Neighborhood Diner"; break;
			case LotRole::Apartment: Name = std::string("The ") + AvenueNames()[Random.Index(AvenueNames().size())] + " Residences"; break;
			default: break;
			}
			MakeBuilding(Entry, Role, Name, std::string());
		}

		// Starter home: the apartment nearest the city centre (a small house in towns without apartments).
		double BestDistance = 1e18;
		bool bAnyApartment = false;
		for (const BuildingPlan& Building : Plan.Buildings)
		{
			bAnyApartment = bAnyApartment || Building.Role == LotRole::Apartment;
		}
		for (const BuildingPlan& Building : Plan.Buildings)
		{
			if (Building.Role == (bAnyApartment ? LotRole::Apartment : LotRole::Home))
			{
				const double Dx = Building.DoorX - Config.OriginX;
				const double Dy = Building.DoorY - Config.OriginY;
				const double Distance = Dx * Dx + Dy * Dy;
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					Plan.StarterHomeId = Building.Id;
				}
			}
		}
		if (const BuildingPlan* Starter = Plan.FindBuilding(Plan.StarterHomeId))
		{
			Plan.SpawnX = Starter->DoorX;
			Plan.SpawnY = Starter->DoorY + (Starter->DoorSide == 0 ? -3.0 : 3.0);
		}

		if (!Config.bVenues)
		{
			Plan.Index();
			return Plan;
		}
		// Venues outside the grid, each with a connector road.
		const double MidRoadY = OriginY + (Grid / 2) * Pitch;
		{
			VenuePlan Drag;
			Drag.Kind = EventKind::DragRace;
			Drag.Name = "County Line Dragway";
			const double X0 = Plan.Bounds.MaxX + 120.0;
			Drag.Area = Rect{X0, MidRoadY - 20.0, X0 + 720.0, MidRoadY + Road + 20.0};
			const double LaneY = MidRoadY + 0.5 * Road;
			Drag.Path = {{X0 + 60.0, LaneY}, {X0 + 60.0 + 402.336, LaneY}, {X0 + 700.0, LaneY}};
			Drag.TrackWidthM = 24.0;
			Plan.Venues.push_back(Drag);
			RoadPlan Connector;
			Connector.Area = Rect{Plan.Bounds.MaxX, MidRoadY, X0, MidRoadY + Road};
			Connector.bHorizontal = true;
			Connector.Name = "Dragway Rd";
			Plan.Roads.push_back(Connector);
		}
		{
			VenuePlan Derby;
			Derby.Kind = EventKind::DemolitionDerby;
			Derby.Name = "County Fairgrounds Arena";
			const double Cx = Plan.Bounds.MaxX - 0.25 * Span;
			const double Cy = Plan.Bounds.MinY - 160.0;
			Derby.Area = Rect{Cx - 55.0, Cy - 38.0, Cx + 55.0, Cy + 38.0};
			Derby.Path = {{Cx, Cy}};
			Plan.Venues.push_back(Derby);
			RoadPlan Connector;
			Connector.Area = Rect{Cx - 0.5 * Road, Cy + 38.0, Cx + 0.5 * Road, Plan.Bounds.MinY};
			Connector.bHorizontal = false;
			Connector.Name = "Fairgrounds Rd";
			Plan.Roads.push_back(Connector);
		}
		{
			VenuePlan Monster;
			Monster.Kind = EventKind::MonsterJam;
			Monster.Name = "Thunderdome Stadium";
			const double Cx = Plan.Bounds.MinX + 0.3 * Span;
			const double Cy = Plan.Bounds.MinY - 220.0;
			Monster.Area = Rect{Cx - 75.0, Cy - 48.0, Cx + 75.0, Cy + 48.0};
			Monster.Path = {{Cx - 50.0, Cy}, {Cx, Cy}, {Cx + 50.0, Cy}};
			Plan.Venues.push_back(Monster);
			RoadPlan Connector;
			Connector.Area = Rect{Cx - 0.5 * Road, Cy + 48.0, Cx + 0.5 * Road, Plan.Bounds.MinY};
			Connector.bHorizontal = false;
			Connector.Name = "Stadium Rd";
			Plan.Roads.push_back(Connector);
		}
		{
			VenuePlan Circuit;
			Circuit.Kind = EventKind::Formula;
			Circuit.Name = "Glory Park Grand Prix Circuit";
			const double Cx = Plan.Bounds.MinX - 520.0;
			const double Cy = MidRoadY;
			const double HalfW = 340.0;
			const double HalfH = 200.0;
			const double Radius = 70.0;
			Circuit.Area = Rect{Cx - HalfW - 30.0, Cy - HalfH - 30.0, Cx + HalfW + 30.0, Cy + HalfH + 30.0};
			// Rounded rectangle, counter-clockwise, with a chicane on the back straight.
			const double Corners[4][3] = {
				{Cx + HalfW - Radius, Cy - HalfH + Radius, -90.0},
				{Cx + HalfW - Radius, Cy + HalfH - Radius, 0.0},
				{Cx - HalfW + Radius, Cy + HalfH - Radius, 90.0},
				{Cx - HalfW + Radius, Cy - HalfH + Radius, 180.0}};
			Circuit.Path.push_back({Cx, Cy - HalfH}); // start/finish line on the main straight
			for (int Corner = 0; Corner < 4; ++Corner)
			{
				for (int Step = 0; Step <= 4; ++Step)
				{
					const double Angle = (Corners[Corner][2] + 22.5 * Step) * 3.14159265358979 / 180.0;
					Circuit.Path.push_back({Corners[Corner][0] + Radius * std::cos(Angle), Corners[Corner][1] + Radius * std::sin(Angle)});
				}
				if (Corner == 1)
				{
					Circuit.Path.push_back({Cx + 60.0, Cy + HalfH});
					Circuit.Path.push_back({Cx + 20.0, Cy + HalfH - 25.0});
					Circuit.Path.push_back({Cx - 20.0, Cy + HalfH - 25.0});
					Circuit.Path.push_back({Cx - 60.0, Cy + HalfH});
				}
			}
			Circuit.TrackWidthM = 14.0;
			Plan.Venues.push_back(Circuit);
			RoadPlan Connector;
			Connector.Area = Rect{Cx + HalfW + 30.0, MidRoadY, Plan.Bounds.MinX, MidRoadY + Road};
			Connector.bHorizontal = true;
			Connector.Name = "Circuit Rd";
			Plan.Roads.push_back(Connector);
		}
		Plan.Index();
		return Plan;
	}

	VenuePlan MakeVenue(EventKind Kind, const std::string& Name, double Cx, double Cy)
	{
		VenuePlan Venue;
		Venue.Kind = Kind;
		Venue.Name = Name;
		switch (Kind)
		{
		case EventKind::DragRace:
		{
			const double X0 = Cx - 360.0;
			Venue.Area = Rect{X0, Cy - 29.0, X0 + 720.0, Cy + 29.0};
			Venue.Path = {{X0 + 60.0, Cy}, {X0 + 60.0 + 402.336, Cy}, {X0 + 700.0, Cy}};
			Venue.TrackWidthM = 24.0;
			break;
		}
		case EventKind::DemolitionDerby:
			Venue.Area = Rect{Cx - 55.0, Cy - 38.0, Cx + 55.0, Cy + 38.0};
			Venue.Path = {{Cx, Cy}};
			break;
		case EventKind::MonsterJam:
			Venue.Area = Rect{Cx - 75.0, Cy - 48.0, Cx + 75.0, Cy + 48.0};
			Venue.Path = {{Cx - 50.0, Cy}, {Cx, Cy}, {Cx + 50.0, Cy}};
			break;
		case EventKind::Formula:
		{
			const double HalfW = 340.0;
			const double HalfH = 200.0;
			const double Radius = 70.0;
			Venue.Area = Rect{Cx - HalfW - 30.0, Cy - HalfH - 30.0, Cx + HalfW + 30.0, Cy + HalfH + 30.0};
			const double Corners[4][3] = {
				{Cx + HalfW - Radius, Cy - HalfH + Radius, -90.0},
				{Cx + HalfW - Radius, Cy + HalfH - Radius, 0.0},
				{Cx - HalfW + Radius, Cy + HalfH - Radius, 90.0},
				{Cx - HalfW + Radius, Cy - HalfH + Radius, 180.0}};
			Venue.Path.push_back({Cx, Cy - HalfH});
			for (int Corner = 0; Corner < 4; ++Corner)
			{
				for (int Step = 0; Step <= 4; ++Step)
				{
					const double Angle = (Corners[Corner][2] + 22.5 * Step) * 3.14159265358979 / 180.0;
					Venue.Path.push_back({Corners[Corner][0] + Radius * std::cos(Angle), Corners[Corner][1] + Radius * std::sin(Angle)});
				}
				if (Corner == 1)
				{
					Venue.Path.push_back({Cx + 60.0, Cy + HalfH});
					Venue.Path.push_back({Cx + 20.0, Cy + HalfH - 25.0});
					Venue.Path.push_back({Cx - 20.0, Cy + HalfH - 25.0});
					Venue.Path.push_back({Cx - 60.0, Cy + HalfH});
				}
			}
			Venue.TrackWidthM = 14.0;
			break;
		}
		case EventKind::Count:
			break;
		}
		return Venue;
	}

	std::array<double, 2> VenueEntrance(const VenuePlan& Venue)
	{
		switch (Venue.Kind)
		{
		case EventKind::DragRace: return {Venue.Area.MinX, Venue.Area.CenterY()};
		case EventKind::Formula: return Venue.Path.empty() ? std::array<double, 2>{Venue.Area.CenterX(), Venue.Area.MinY} : Venue.Path.front();
		default: return {Venue.Area.CenterX(), Venue.Area.MinY};
		}
	}
}
