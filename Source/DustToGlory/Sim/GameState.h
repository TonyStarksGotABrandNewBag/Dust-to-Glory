// GameState: all of the player's progress plus the loaded content, with one place for game rules that
// span systems (paying shifts into the wallet, Sunday offerings, owner income, royalties, save/load).
// The Unreal layer owns one of these inside UDTGGameSubsystem.
#pragma once

#include "Sim/CityLayout.h"
#include "Sim/Careers.h"
#include "Sim/Clock.h"
#include "Sim/Economy.h"
#include "Sim/Events.h"
#include "Sim/Furniture.h"
#include "Sim/GameTheory.h"
#include "Sim/Garage.h"
#include "Sim/Hacking.h"
#include "Sim/Json.h"
#include "Sim/Market.h"
#include "Sim/Ministry.h"
#include "Sim/NpcMinds.h"
#include "Sim/Politics.h"
#include "Sim/Powers.h"
#include "Sim/Tuning.h"
#include "Sim/Vehicles.h"
#include "Sim/Wanted.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	/** Raw JSON text of each data file in Content/Data. */
	struct ContentFiles
	{
		std::string Economy;
		std::string Vehicles;
		std::string Parts;
		std::string Careers;
		std::string Powers;
		std::string Furniture;
		std::string Companies;
		std::string Laws;
	};

	struct ContentReport
	{
		std::vector<std::string> Errors;
		int Brands = 0;
		int Nameplates = 0;
		int Models = 0;
		int Parts = 0;
		int Careers = 0;
		int Educations = 0;
		int Powers = 0;
		int Mutations = 0;
		int FurnitureItems = 0;
		int Businesses = 0;
		int Buildings = 0;
		int Laws = 0;
		bool Ok() const { return Errors.empty(); }
	};

	// Individual loaders, exposed so tests can validate each file on its own.
	bool LoadEconomyJson(const JsonValue& Root, EconomyIndex& Out, std::vector<std::string>& Errors);
	bool LoadVehiclesJson(const JsonValue& Root, VehicleCatalog& Out, std::vector<std::string>& Errors);
	bool LoadPartsJson(const JsonValue& Root, PartCatalog& Out, std::vector<std::string>& Errors);
	bool LoadCareersJson(const JsonValue& Root, CareerBook& Out, std::vector<std::string>& Errors);
	bool LoadPowersJson(const JsonValue& Root, PowerRoster& Out, std::vector<std::string>& Errors);
	bool LoadFurnitureJson(const JsonValue& Root, HomeAndFurniture& Out, std::vector<std::string>& Errors);
	bool LoadLawsJson(const JsonValue& Root, std::vector<LawDef>& Out, std::vector<std::string>& Errors);

	struct ActionResult
	{
		bool bOk = false;
		std::string Message;

		static ActionResult Success(std::string Text) { return ActionResult{true, std::move(Text)}; }
		static ActionResult Failure(std::string Text) { return ActionResult{false, std::move(Text)}; }
	};

	struct TickReport
	{
		std::int64_t MinutesElapsed = 0;
		bool bWeatherChanged = false;
		bool bNewDay = false;
		bool bNewWeek = false;
		std::vector<DeliveryEvent> Deliveries;
		bool bOffering = false;
		OfferingResult Offering;
		Money OwnerIncome = 0;
		Money Royalties = 0;
		bool bPowerSurge = false;
		MutationEvent Surge;
		ElectionResult Election;
		std::vector<std::string> Messages;
	};

	class GameState
	{
	public:
		static constexpr const char* PlayerMinistryId = "player-ministry";
		static constexpr int SaveVersion = 1;

		// ---------------------------------------------------------------- Content (rebuilt on load)
		EconomyIndex Economy;
		VehicleCatalog Vehicles;
		PartCatalog Parts;
		CareerBook Careers;
		PowerRoster Powers;
		CityPlan City;
		std::vector<LawDef> LawBook;
		std::uint64_t WorldSeed = 1977;

		// ---------------------------------------------------------------- Progress (saved)
		std::string PlayerName = "Player";
		Wallet Funds{Dollars(2500)};
		GameClock Clock;
		WeatherSystem Weather;
		Profile Player;
		std::vector<CareerProgress> Jobs;
		PowerSet PlayerPowers;
		HomeAndFurniture Homes;
		std::vector<OwnedVehicle> Garage;
		std::string ActiveVehicleUid;
		HackToolkit HackTools;
		std::vector<HackContract> HackBoard;
		WantedSystem Wanted;
		Ministry PlayerMinistry;
		Market Markets;
		IpPortfolio Ip;
		NpcPopulation Npcs;
		UsedCarMarket UsedCars;
		PoliticalCareer Politics;
		Rng Random{1977};
		int NextVehicleNumber = 1;
		int EventsWon = 0;

		/** Parses every data file and builds the catalog, city and businesses. */
		bool LoadContent(const ContentFiles& Files, ContentReport& Report);
		/** Fresh game on top of loaded content. */
		void NewGame(std::uint64_t Seed, const std::string& Name);

		/** Advances real time; runs per-minute, daily and weekly rules. */
		TickReport Advance(double RealSeconds);
		/** Jumps the clock (shifts, classes, sleeping) and runs the same rules for the skipped time. */
		TickReport Skip(std::int64_t Minutes);

		// ---------------------------------------------------------------- Vehicles
		ActionResult BuyVehicle(const std::string& ModelId);
		ActionResult SellVehicle(const std::string& Uid);
		ActionResult InstallPart(const std::string& Uid, const std::string& PartId);
		ActionResult RemovePart(const std::string& Uid, PartCategory Category);
		ActionResult SetTuning(const std::string& Uid, const TuningSheet& Sheet);
		ActionResult Paint(const std::string& Uid, std::uint32_t Rgb, int Finish);
		ActionResult Repair(const std::string& Uid);
		OwnedVehicle* FindOwned(const std::string& Uid);
		const OwnedVehicle* FindOwned(const std::string& Uid) const;
		const OwnedVehicle* ActiveVehicle() const;
		/** Physics setup for one of the player's cars (or a stock catalog model when Uid is empty). */
		VehicleSetup SetupFor(const std::string& ModelId, const VehicleBuild* Build = nullptr) const;
		/** Grants a car for free (event machinery, story rewards). */
		std::string GrantVehicle(const std::string& ModelId);

		// ---------------------------------------------------------------- Used cars and DIY
		void RefreshUsedCars();
		HaggleResult HaggleUsed(const std::string& ListingId, Money Offer);
		ActionResult BuyUsed(const std::string& ListingId, Money Price);
		RepairQuote Quote(const std::string& Uid, CarIssue Issue) const;
		RepairResult RepairDiy(const std::string& Uid, CarIssue Issue, double Performance);
		ActionResult RepairAtShop(const std::string& Uid, CarIssue Issue);
		Money PrivateOffer(const std::string& Uid);
		ActionResult SellPrivately(const std::string& Uid, Money Price);
		/** Setup for an owned car including its faults. */
		VehicleSetup SetupForOwned(const OwnedVehicle& Car) const;

		// ---------------------------------------------------------------- Careers and education
		CareerProgress* FindJob(const std::string& CareerId);
		ActionResult ApplyForCareer(const std::string& CareerId);
		ShiftResult WorkShift(const std::string& CareerId, double Performance, TickReport* OutTick = nullptr);
		ActionResult Enroll(const std::string& EducationId);
		ClassResult AttendClass(double Performance, int Hours = 3);

		// ---------------------------------------------------------------- Powers
		ActionResult ChoosePowers(const std::vector<std::string>& Ids);

		// ---------------------------------------------------------------- Homes and furniture
		ActionResult BuyHome(int BuildingId);
		std::string PropertyIdFor(int BuildingId) const { return "bldg-" + std::to_string(BuildingId); }
		ActionResult OrderFurniture(const std::vector<std::string>& ItemIds, int HomeBuildingId, DeliverySpeed Speed);

		// ---------------------------------------------------------------- Hacking
		void RefreshHackBoard();
		ActionResult BuyHackTool(int Which); // 0 probe kit, 1 ghost relay, 2 pattern engine
		HackConfig ConfigFor(const HackContract& Contract) const;
		ActionResult CompleteHack(const std::string& ContractId, bool bSuccess);

		// ---------------------------------------------------------------- Ministry
		ActionResult FoundMinistry(const std::string& Name);
		SermonResult Preach(SermonTheme Theme, double Performance);
		ActionResult RunOutreach(OutreachKind Kind);
		ActionResult UpgradeMinistry(Facility Value);
		ActionResult Telethon();
		ActionResult Accountability();

		// ---------------------------------------------------------------- Market, business, IP
		ActionResult BuyShares(const std::string& Ticker, std::int64_t Shares);
		ActionResult SellShares(const std::string& Ticker, std::int64_t Shares);
		ActionResult BuyBusiness(const std::string& BusinessId);
		/** Reputation attacks and press; the player's own ministry is affected through ApplyScandal. */
		AttackResult Attack(const std::string& BusinessId, AttackKind Kind);
		CreativeWork& CreateWork(WorkKind Kind, const std::string& Title, int Hours, double Performance);

		// ---------------------------------------------------------------- Politics and law
		const LawDef* FindLaw(const std::string& Id) const;
		LawEffects ActiveLaws() const;
		ActionResult RunForOffice(Office Target);
		ActionResult CampaignRally(double Performance);
		ActionResult CampaignAds(Money Amount);
		ActionResult CampaignDebate(double Performance);
		/** The Nash advisor: the campaign game for the player's skills, and its equilibrium. */
		MatrixGame CampaignMatrix() const;
		CampaignRound CampaignStrategy(int Choice, double Performance);
		LawVote ProposeLaw(const std::string& LawId);
		LawVote LobbyForLaw(const std::string& LawId, Money Amount);
		/** Crime reporting that respects the laws in force (speed limits, hacking penalties). */
		void ReportCrime(Crime Value, bool bPoliceWitness, bool bCivilianWitness);
		/** Inspection-game equilibrium: how likely police are patrolling where a crime with this payoff happens. */
		double PolicePatrolChance(double CrimeGain) const;

		// ---------------------------------------------------------------- Events and the law
		Money EventEntryFee(EventKind Kind, int Tier) const;
		ActionResult EnterEvent(EventKind Kind, int Tier);
		Money FinishEvent(EventKind Kind, int Tier, int Position, int FieldSize);
		/** Busted by police: fine and clear heat. */
		Money Busted();
		/** Knocked out (lost a fight or a crash): hospital bill. */
		Money Hospitalized();

		Money NetWorth() const;
		std::vector<std::string> TakeNotifications();
		void Notify(const std::string& Text);

		// ---------------------------------------------------------------- Save/load
		std::string SaveToJson() const;
		bool LoadFromJson(const std::string& Text, std::string* OutError);

	private:
		void BuildBusinesses(const JsonValue& Companies, ContentReport& Report);
		void SyncMinistryBusiness();
		void RunMinute(std::int64_t Minute, TickReport& Report);
		void RunDay(std::int64_t Minute, TickReport& Report);
		void RunWeek(std::int64_t Minute, TickReport& Report);

		std::vector<std::string> PendingNotifications;
		JsonValue CompaniesData;
	};
}
