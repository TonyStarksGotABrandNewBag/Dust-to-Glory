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
#include "Sim/Justice.h"
#include "Sim/Market.h"
#include "Sim/Ministry.h"
#include "Sim/NpcMinds.h"
#include "Sim/Politics.h"
#include "Sim/Powers.h"
#include "Sim/PrivateSales.h"
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
		std::vector<Message> NewMessages; // calls, texts and email that arrived this tick
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
		bool bManualTransmission = false;
		JusticeSystem Justice;
		std::vector<Crime> OpenCharges;          // crimes reported during the current police chase
		std::string PhoneNumber;                 // written on notes left for car owners
		std::vector<PrivateCar> PrivateCars;     // parked cars the player has dealt with (others are regenerated)
		std::vector<PendingOffer> PendingOffers; // notes and texted counters waiting for a reply
		Inbox Phone;                             // calls, texts and email
		int NextOfferId = 1;

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
		/** New cars are current models only; everything older is sold used, certified, classic or located. */
		Money NewCarPrice(const VehicleModel& Model) const;
		/** Weekly restock of the classic dealer and every showroom's certified pre-owned row. */
		void RefreshDealerStock();
		/** A showroom tracks down any model its brands ever built; the car appears in its Located row. */
		ActionResult LocateModel(const std::string& ShowroomId, const std::string& ModelId);
		RestorationQuote QuoteRestore(const std::string& Uid, RestorationLevel Level, bool bCrateEngine) const;
		ActionResult RestoreAtShop(const std::string& Uid, RestorationLevel Level, bool bCrateEngine);
		ActionResult RestoreDiy(const std::string& Uid, RestorationLevel Level, bool bCrateEngine, double Performance);
		/** Player preference: automatic or manual shifting (GT-style AT/MT). */
		ActionResult SetTransmission(bool bManual);
		bool AutomaticGearbox() const { return !bManualTransmission; }

		/** Brands sold in a showroom building (BuildingPlan::Param of a dealership). */
		std::vector<const Brand*> BrandsAtShowroom(const std::string& ShowroomId) const;
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

		// ---------------------------------------------------------------- Offers on any parked car
		/** The car parked in a spot (and its owner), created on first look. */
		PrivateCar& PrivateCarAt(const std::string& CarId);
		bool IsOwnerHomeNow(const std::string& CarId);
		/** Knock on the door and make an offer face to face. Accepted offers complete the purchase. */
		OfferResponse OfferInPerson(const std::string& CarId, Money Amount);
		/** Leave a note with the offer and your number; the owner calls, texts or emails back after they get home. */
		ActionResult LeaveNote(const std::string& CarId, Money Amount);
		/** Text a (counter-)offer in an owner's thread; they text back (Rubinstein alternating offers). */
		ActionResult TextOffer(const std::string& CarId, Money Amount);
		/** Accept a counter (or confirm an accepted offer) to buy, or decline. */
		ActionResult RespondToMessage(int MessageId, bool bAccept);
		/** System and business email (job offers, deliveries, consulting requests...). Returns the message id. */
		int SendEmail(MessageTopic Topic, const std::string& FromName, const std::string& FromAddress, const std::string& Subject,
			const std::string& Body);

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
		/** Busted by police: a fine for minor matters, otherwise a court case (see Justice). Returns the fine paid. */
		Money Busted(const std::string& CoDefendantId = std::string());
		ActionResult HireLawyer();
		Verdict TakePlea();
		Verdict GoToTrial();
		/** Prisoner's dilemma with a co-defendant: they decide from their repeated-game memory of the player. */
		Verdict CoDefendantChoice(bool bTestify);
		JailDayResult ServeJailDay(JailActivity Activity);
		ActionResult RequestParole();
		ActionResult FileAppeal();
		ActionResult ExpungeRecord();
		bool InJail() const { return Justice.Jail().bInJail; }
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
		ActionResult BuyPrivateCar(PrivateCar& Car, Money Price);
		void Deliver(Message Value, TickReport* Report);
		void ApplyVerdict(const Verdict& Result);

		std::vector<std::string> PendingNotifications;
		JsonValue CompaniesData;
	};
}
