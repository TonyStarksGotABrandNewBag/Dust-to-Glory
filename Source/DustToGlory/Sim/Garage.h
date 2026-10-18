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

	/** A car in the player's garage. */
	struct OwnedVehicle
	{
		std::string Uid;
		std::string ModelId;
		VehicleBuild Build;
		double Condition = 1.0;     // 0 wrecked .. 1 showroom
		double MileageKm = 0.0;
		std::vector<CarIssue> Issues;

		bool Has(CarIssue Issue) const;
	};

	/** What a buyer would pay for this car in its current state. */
	Money MarketValue(const VehicleModel& Model, double Condition, double MileageKm, const std::vector<CarIssue>& Issues);
	Money ResaleValue(const VehicleModel& Model, const OwnedVehicle& Car);
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
	};

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

	private:
		std::vector<UsedListing> ListingList;
	};
}
