// Owned cars, wear and faults, DIY and shop repairs, and the used-car market for flipping.
#pragma once

#include "Sim/Careers.h"
#include "Sim/SimCommon.h"
#include "Sim/Tuning.h"
#include "Sim/Vehicles.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	enum class CarIssue : int
	{
		WornEngine,
		BlownHeadGasket,
		SlippingTransmission,
		WornSuspension,
		WornBrakes,
		BaldTires,
		ElectricalGremlins,
		Rust,
		FadedPaint,
		TornInterior,
		Count
	};

	struct IssueInfo
	{
		CarIssue Issue;
		const char* Name;
		double ValuePenalty;   // fraction of value lost while present
		double PartsFraction;  // parts cost as a fraction of the car's value...
		double PartsMinimum;   // ...but never less than this many dollars
		int DiyHours;
		int Difficulty;        // Trades level for a comfortable DIY job
		bool bCosmetic;
	};

	const IssueInfo& GetIssueInfo(CarIssue Issue);
	bool ParseCarIssue(const std::string& Text, CarIssue& Out);

	/** How far a car has been restored. A resto-mod keeps the classic body but modern running gear. */
	enum class RestorationLevel : int
	{
		None,
		Driver,     // mechanically sorted, honest paint: a classic you can use
		Show,       // body-off, everything refinished
		Concours,   // factory-correct to the last bolt; aftermarket parts come off
		RestoMod,   // show-quality body, modern suspension, brakes, wheels and (optionally) a crate engine
		Count
	};
	const char* RestorationName(RestorationLevel Level);

	/** A car in the player's garage. */
	struct OwnedVehicle
	{
		std::string Uid;
		std::string ModelId;
		VehicleBuild Build;
		double Condition = 1.0;     // 0 wrecked .. 1 showroom
		double MileageKm = 0.0;
		std::vector<CarIssue> Issues;
		RestorationLevel Restoration = RestorationLevel::None;
		bool bCertified = false;              // bought certified pre-owned
		std::int64_t WarrantyUntilMinute = 0; // free shop repairs until then (new and CPO cars)
		std::int64_t ReadyMinute = 0;         // in the restoration shop until then

		bool Has(CarIssue Issue) const;
		bool InShop(std::int64_t NowMinute) const { return ReadyMinute > NowMinute; }
		bool UnderWarranty(std::int64_t NowMinute) const { return WarrantyUntilMinute > NowMinute; }
	};

	/** What a buyer would pay for this car in its current state. */
	Money MarketValue(const VehicleModel& Model, double Condition, double MileageKm, const std::vector<CarIssue>& Issues);
	Money ResaleValue(const VehicleModel& Model, const OwnedVehicle& Car);
	/** Collector value of a classic in this state: condition and originality matter far more than mileage. */
	double ClassicConditionFactor(double Condition);
	/** Dealers pay less than private buyers. */
	Money TradeInOffer(const VehicleModel& Model, const OwnedVehicle& Car);
	/** A private buyer's offer (market value +/- haggling, plus a little for tasteful mods). */
	Money PrivateSaleOffer(const VehicleModel& Model, const OwnedVehicle& Car, const PartCatalog& Parts, Rng& Random);

	/** Worn parts change how the car drives until they are fixed. */
	void ApplyIssues(VehicleSetup& Setup, const std::vector<CarIssue>& Issues);

	struct RepairQuote
	{
		Money Parts = 0;
		Money ShopLabor = 0;
		int DiyHours = 0;
		double DiySuccessChance = 0.0;
	};

	RepairQuote QuoteRepair(const VehicleModel& Model, CarIssue Issue, const Profile& Player);

	struct RepairResult
	{
		bool bFixed = false;
		Money PartsSpent = 0;
		int Hours = 0;
		int SkillXp = 0;
	};

	/** Do it yourself: cheaper, takes time, may fail (half the parts wasted). Performance is the minigame score. */
	RepairResult DiyRepair(OwnedVehicle& Car, CarIssue Issue, const RepairQuote& Quote, Profile& Player, double Performance, Rng& Random);
	/** Pay a shop: always fixed. */
	void ShopRepair(OwnedVehicle& Car, CarIssue Issue);

	/** Who is selling: the used lot, a brand showroom's certified pre-owned row, the classic dealer, or a car a
	    showroom tracked down on request. */
	enum class DealerKind : int
	{
		UsedLot,
		Certified,
		Classic,
		Located,
		Count
	};
	const char* DealerKindName(DealerKind Kind);

	struct UsedListing
	{
		std::string Id;
		std::string ModelId;
		double Condition = 0.6;
		double MileageKm = 100000.0;
		std::vector<CarIssue> Issues;
		Money AskingPrice = 0;
		std::string Seller;
		bool bBarnFind = false;
		int Rounds = 0;            // haggling rounds used
		DealerKind Dealer = DealerKind::UsedLot;
		std::string ShowroomId;    // certified and located cars: the showroom selling them
		RestorationLevel Restoration = RestorationLevel::None;
		std::vector<std::string> Parts; // resto-mods come with their modern parts fitted
		bool bNoHaggle = false;    // certified pre-owned: posted price
	};

	/** Certified pre-owned: recent, inspected, low miles, a year of warranty, a premium over the used lot. */
	std::vector<UsedListing> GenerateCertified(const VehicleCatalog& Catalog, const std::vector<std::string>& BrandIds,
		const std::string& ShowroomId, Rng& Random, int Count);
	/** The classic dealer: 25+ year-old cars from projects to concours winners, plus resto-mods. */
	std::vector<UsedListing> GenerateClassics(const VehicleCatalog& Catalog, Rng& Random, int Count);
	/** A showroom tracks down one example of any model its brands ever built. */
	UsedListing LocateExample(const VehicleModel& Model, const std::string& ShowroomId, Rng& Random);

	// ------------------------------------------------------------------ Restoration
	struct RestorationQuote
	{
		bool bAllowed = false;
		std::string Reason;          // why not, when not allowed
		Money ShopCost = 0;          // labor + parts at the restoration shop
		int ShopDays = 0;
		Money DiyParts = 0;          // parts only, doing it yourself
		int DiyHours = 0;
		double DiySuccessChance = 0.0;
		Money ValueBefore = 0;
		Money ValueAfter = 0;
		std::vector<std::string> PartsFitted; // resto-mod running gear
	};
	/** bCrateEngine adds the modern crate V8 to a resto-mod. */
	RestorationQuote QuoteRestoration(const VehicleModel& Model, const OwnedVehicle& Car, RestorationLevel Level, const Profile& Player,
		const PartCatalog& Parts, bool bCrateEngine = false);
	/** Applies the finished restoration (condition, faults, parts and level). */
	void ApplyRestoration(OwnedVehicle& Car, RestorationLevel Level, const RestorationQuote& Quote, const PartCatalog& Parts, double Quality);

	enum class HaggleOutcome : int
	{
		Accepted,
		Countered,
		WalkedAway
	};

	struct HaggleResult
	{
		HaggleOutcome Outcome = HaggleOutcome::WalkedAway;
		Money Price = 0;
		std::string Reply;
	};

	class UsedCarMarket
	{
	public:
		/** Replaces the listings (called each morning). */
		void Refresh(const VehicleCatalog& Catalog, Rng& Random, int Count = 10);
		const std::vector<UsedListing>& Listings() const { return ListingList; }
		UsedListing* Find(const std::string& Id);
		HaggleResult Haggle(const std::string& ListingId, Money Offer, const VehicleCatalog& Catalog, const Profile& Player, Rng& Random);
		/** Removes the listing and returns the car it described (the caller handles payment). */
		bool Take(const std::string& ListingId, OwnedVehicle& OutCar);
		void Restore(std::vector<UsedListing> InListings) { ListingList = std::move(InListings); }
		/** Replaces one dealer's stock (the used lot's Refresh only touches the used lot). */
		void ReplaceStock(DealerKind Kind, const std::string& ShowroomId, std::vector<UsedListing> Stock);
		void Add(UsedListing Listing) { ListingList.push_back(std::move(Listing)); }
		std::vector<const UsedListing*> Stock(DealerKind Kind, const std::string& ShowroomId = std::string()) const;

	private:
		std::vector<UsedListing> ListingList;
	};
}
