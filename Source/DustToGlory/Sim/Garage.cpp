#include "Sim/Garage.h"

#include "Sim/GameTheory.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	namespace detail_garage
	{
		const IssueInfo Issues[] = {
			{CarIssue::WornEngine, "Worn engine", 0.25, 0.20, 900.0, 14, 6, false},
			{CarIssue::BlownHeadGasket, "Blown head gasket", 0.35, 0.06, 350.0, 10, 5, false},
			{CarIssue::SlippingTransmission, "Slipping transmission", 0.25, 0.15, 700.0, 12, 6, false},
			{CarIssue::WornSuspension, "Worn suspension", 0.10, 0.06, 400.0, 6, 3, false},
			{CarIssue::WornBrakes, "Worn brakes", 0.06, 0.02, 120.0, 2, 1, false},
			{CarIssue::BaldTires, "Bald tires", 0.05, 0.03, 200.0, 1, 0, false},
			{CarIssue::ElectricalGremlins, "Electrical gremlins", 0.10, 0.03, 150.0, 5, 4, false},
			{CarIssue::Rust, "Rust", 0.15, 0.05, 300.0, 10, 4, true},
			{CarIssue::FadedPaint, "Faded paint", 0.08, 0.04, 250.0, 8, 3, true},
			{CarIssue::TornInterior, "Torn interior", 0.06, 0.03, 180.0, 4, 2, true},
		};

		const std::vector<std::string>& Sellers()
		{
			static const std::vector<std::string> Names = {
				"Retired teacher", "College student", "Busy dad", "Estate sale", "Car flipper", "Farmer", "Nervous first-timer",
				"Grandma (garage kept)", "Guy who 'knows cars'", "Divorcing couple", "Moving overseas", "Fleet auction"};
			return Names;
		}
	}

	const IssueInfo& GetIssueInfo(CarIssue Issue)
	{
		for (const IssueInfo& Entry : detail_garage::Issues)
		{
			if (Entry.Issue == Issue)
			{
				return Entry;
			}
		}
		return detail_garage::Issues[0];
	}

	bool ParseCarIssue(const std::string& Text, CarIssue& Out)
	{
		const std::string Lower = ToLowerAscii(Text);
		for (const IssueInfo& Entry : detail_garage::Issues)
		{
			if (ToLowerAscii(Entry.Name) == Lower || Slugify(Entry.Name) == Lower)
			{
				Out = Entry.Issue;
				return true;
			}
		}
		return false;
	}

	bool OwnedVehicle::Has(CarIssue Issue) const
	{
		for (const CarIssue Existing : Issues)
		{
			if (Existing == Issue) return true;
		}
		return false;
	}

	double ClassicConditionFactor(double Condition)
	{
		// Relative to a good driver-quality example (condition 0.8, the catalog's market price): a project car is
		// worth about a third of that, a concours car about a third more.
		const double Raw = 0.12 + 0.88 * std::pow(Clamp(Condition, 0.0, 1.0), 1.6);
		const double AtGood = 0.12 + 0.88 * std::pow(0.8, 1.6);
		return Raw / AtGood;
	}

	Money MarketValue(const VehicleModel& Model, double Condition, double MileageKm, const std::vector<CarIssue>& Issues)
	{
		double Value = ToDollars(Model.MarketPrice);
		if (Model.IsClassic())
		{
			Value *= 1.0 - 0.1 * std::min(1.0, std::max(0.0, MileageKm) / 300000.0);
			Value *= ClassicConditionFactor(Condition);
		}
		else
		{
			Value *= 1.0 - 0.3 * std::min(1.0, std::max(0.0, MileageKm) / 300000.0);
			Value *= 0.35 + 0.65 * Clamp(Condition, 0.0, 1.0);
		}
		for (const CarIssue Issue : Issues)
		{
			Value *= 1.0 - GetIssueInfo(Issue).ValuePenalty;
		}
		return ToMoney(std::round(std::max(150.0, Value) / 25.0) * 25.0);
	}

	Money ResaleValue(const VehicleModel& Model, const OwnedVehicle& Car)
	{
		const Money Base = MarketValue(Model, Car.Condition, Car.MileageKm, Car.Issues);
		if (!Model.IsClassic())
		{
			return Base;
		}
		// Collectors pay for originality; resto-mods have their own buyers; a hacked-up original loses value.
		double Factor = 1.0;
		switch (Car.Restoration)
		{
		case RestorationLevel::Concours: Factor = Car.Build.Parts.empty() ? 1.25 : 1.0; break;
		case RestorationLevel::Show: Factor = Car.Build.Parts.empty() ? 1.08 : 1.0; break;
		case RestorationLevel::RestoMod: Factor = 1.15; break;
		case RestorationLevel::Driver: Factor = 1.0; break;
		default: Factor = Car.Build.Parts.empty() ? 1.0 : 0.9; break;
		}
		return ToMoney(std::round(ToDollars(Base) * Factor / 25.0) * 25.0);
	}

	Money TradeInOffer(const VehicleModel& Model, const OwnedVehicle& Car)
	{
		return static_cast<Money>(std::llround(static_cast<double>(ResaleValue(Model, Car)) * 0.7));
	}

	Money PrivateSaleOffer(const VehicleModel& Model, const OwnedVehicle& Car, const PartCatalog& Parts, Rng& Random)
	{
		double Offer = ToDollars(ResaleValue(Model, Car)) * Random.Range(0.9, 1.1);
		for (const std::string& PartId : Car.Build.Parts)
		{
			if (const PartDef* Part = Parts.Find(PartId))
			{
				// Buyers pay for some upgrades, not all of them; resto-mod buyers want the modern parts.
				Offer += (Car.Restoration == RestorationLevel::RestoMod ? 0.6 : 0.3) * ToDollars(PartPrice(*Part, Model));
			}
		}
		return ToMoney(std::round(Offer / 25.0) * 25.0);
	}

	void ApplyIssues(VehicleSetup& Setup, const std::vector<CarIssue>& Issues)
	{
		for (const CarIssue Issue : Issues)
		{
			switch (Issue)
			{
			case CarIssue::WornEngine:
				Setup.Engine.PeakPowerKw *= 0.85;
				Setup.Engine.PeakTorqueNm *= 0.88;
				break;
			case CarIssue::BlownHeadGasket:
				Setup.Engine.PeakPowerKw *= 0.7;
				Setup.Engine.PeakTorqueNm *= 0.72;
				break;
			case CarIssue::SlippingTransmission:
				Setup.Gearbox.ClutchCapacityNm *= 0.6;
				Setup.Gearbox.ShiftSeconds *= 1.8;
				break;
			case CarIssue::WornSuspension:
				Setup.Suspension.BumpFront *= 0.55;
				Setup.Suspension.BumpRear *= 0.55;
				Setup.Suspension.ReboundFront *= 0.55;
				Setup.Suspension.ReboundRear *= 0.55;
				break;
			case CarIssue::WornBrakes:
				Setup.Brakes.MaxTorqueFront *= 0.65;
				Setup.Brakes.MaxTorqueRear *= 0.65;
				break;
			case CarIssue::BaldTires:
				Setup.FrontTire.Mu *= 0.85;
				Setup.RearTire.Mu *= 0.85;
				break;
			default:
				break;
			}
		}
	}

	RepairQuote QuoteRepair(const VehicleModel& Model, CarIssue Issue, const Profile& Player)
	{
		const IssueInfo& Info = GetIssueInfo(Issue);
		RepairQuote Quote;
		const double Value = ToDollars(Model.MarketPrice);
		Quote.Parts = ToMoney(std::round(std::max(Info.PartsMinimum, Info.PartsFraction * Value) / 5.0) * 5.0);
		Quote.ShopLabor = ToMoney(45.0 * Info.DiyHours * 0.8); // shops are faster than you, at 1995 labor rates
		const int Trades = Player.SkillLevel(Skill::Trades);
		Quote.DiyHours = std::max(1, static_cast<int>(std::lround(Info.DiyHours * (1.2 - 0.05 * Trades))));
		Quote.DiySuccessChance = Clamp(0.55 + 0.08 * (Trades - Info.Difficulty), 0.08, 0.97);
		return Quote;
	}

	RepairResult DiyRepair(OwnedVehicle& Car, CarIssue Issue, const RepairQuote& Quote, Profile& Player, double Performance, Rng& Random)
	{
		RepairResult Result;
		Result.Hours = Quote.DiyHours;
		const double Chance = Clamp(Quote.DiySuccessChance + 0.25 * (Clamp(Performance, 0.0, 1.0) - 0.5), 0.05, 0.99);
		Result.bFixed = Random.Chance(Chance);
		Result.PartsSpent = Result.bFixed ? Quote.Parts : Quote.Parts / 2;
		Result.SkillXp = 20 + 4 * Quote.DiyHours + (Result.bFixed ? 20 : 0);
		Player.AddSkillXp(Skill::Trades, Result.SkillXp);
		if (Result.bFixed)
		{
			ShopRepair(Car, Issue);
		}
		return Result;
	}

	void ShopRepair(OwnedVehicle& Car, CarIssue Issue)
	{
		for (auto It = Car.Issues.begin(); It != Car.Issues.end(); ++It)
		{
			if (*It == Issue)
			{
				Car.Issues.erase(It);
				break;
			}
		}
		Car.Condition = Clamp(Car.Condition + (GetIssueInfo(Issue).bCosmetic ? 0.12 : 0.08), 0.0, 1.0);
	}

	void UsedCarMarket::Refresh(const VehicleCatalog& Catalog, Rng& Random, int Count)
	{
		ListingList.erase(std::remove_if(ListingList.begin(), ListingList.end(),
			[](const UsedListing& Listing) { return Listing.Dealer == DealerKind::UsedLot; }), ListingList.end());
		// The used lot carries everyday cars from the last quarter century; barn finds dig deeper.
		std::vector<const VehicleModel*> Pool;
		for (const VehicleModel& Model : Catalog.Models())
		{
			if (!Model.bEventOnly && (Model.AgeYears > 0 || Model.YearStart < Catalog.CurrentYear() - 1) && Model.AgeYears < 25)
			{
				Pool.push_back(&Model);
			}
		}
		if (Pool.empty())
		{
			return;
		}
		for (int Index = 0; Index < Count; ++Index)
		{
			UsedListing Listing;
			const VehicleModel* Model = Pool[Random.Index(Pool.size())];
			const bool bBarnFind = Random.Chance(0.04);
			if (bBarnFind)
			{
				// Barn finds are old collectibles left to sit for decades.
				for (int Attempt = 0; Attempt < 40; ++Attempt)
				{
					const VehicleModel* Candidate = Pool[Random.Index(Pool.size())];
					if (Catalog.CurrentYear() - Candidate->YearEnd > 30 &&
						(Candidate->Body == BodyStyle::Muscle || Candidate->Body == BodyStyle::Sports || Candidate->Body == BodyStyle::Convertible))
					{
						Model = Candidate;
						break;
					}
				}
			}
			const int Age = std::max(0, Catalog.CurrentYear() - Model->YearEnd) + Random.RangeInt(0, 4);
			Listing.ModelId = Model->Id;
			Listing.bBarnFind = bBarnFind;
			Listing.Condition = bBarnFind ? Random.Range(0.15, 0.35) : Random.Range(0.25, 0.95);
			Listing.MileageKm = std::round(std::max(5000.0, Age * 15000.0 * Random.Range(0.4, 1.4)) / 100.0) * 100.0;
			const int IssueCount = Clamp(static_cast<int>(std::lround((1.0 - Listing.Condition) * 5.0 + Random.Range(-0.5, 0.5))), 0, 5);
			for (int Try = 0; Try < IssueCount * 3 && static_cast<int>(Listing.Issues.size()) < IssueCount; ++Try)
			{
				const CarIssue Issue = static_cast<CarIssue>(Random.RangeInt(0, static_cast<int>(CarIssue::Count) - 1));
				bool bHas = false;
				for (const CarIssue Existing : Listing.Issues) bHas = bHas || Existing == Issue;
				if (!bHas) Listing.Issues.push_back(Issue);
			}
			const double Value = ToDollars(MarketValue(*Model, Listing.Condition, Listing.MileageKm, Listing.Issues));
			// Sellers overprice; barn-find owners just want it gone.
			const double Asking = bBarnFind ? Value * Random.Range(0.4, 0.7) : Value * Random.Range(0.95, 1.3);
			Listing.AskingPrice = ToMoney(std::round(Asking / 50.0) * 50.0);
			Listing.Seller = Random.Pick(detail_garage::Sellers());
			Listing.Id = "used-" + std::to_string(Random.NextU32() % 1000000u);
			ListingList.push_back(Listing);
		}
	}

	UsedListing* UsedCarMarket::Find(const std::string& Id)
	{
		for (UsedListing& Listing : ListingList)
		{
			if (Listing.Id == Id) return &Listing;
		}
		return nullptr;
	}

	HaggleResult UsedCarMarket::Haggle(const std::string& ListingId, Money Offer, const VehicleCatalog& Catalog, const Profile& Player, Rng& Random)
	{
		HaggleResult Result;
		UsedListing* Listing = Find(ListingId);
		const VehicleModel* Model = Listing ? Catalog.FindModel(Listing->ModelId) : nullptr;
		if (!Listing || !Model)
		{
			Result.Reply = "That car already sold.";
			return Result;
		}
		if (Listing->bNoHaggle)
		{
			const bool bPays = Offer >= Listing->AskingPrice;
			Result.Outcome = bPays ? HaggleOutcome::Accepted : HaggleOutcome::WalkedAway;
			Result.Price = Listing->AskingPrice;
			Result.Reply = bPays ? "Certified, inspected and warrantied. Here are your keys." : "Certified cars are sold at the posted price.";
			return Result;
		}
		Listing->Rounds += 1;
		OwnedVehicle AsListed;
		AsListed.Condition = Listing->Condition;
		AsListed.MileageKm = Listing->MileageKm;
		AsListed.Issues = Listing->Issues;
		AsListed.Restoration = Listing->Restoration;
		AsListed.Build.Parts = Listing->Parts;
		const double Value = ToDollars(ResaleValue(*Model, AsListed));
		const double Asking = ToDollars(Listing->AskingPrice);
		// Nash bargaining between the seller's walk-away price and what the car is worth to a buyer. People skills
		// and knowing the faults (Trades) raise the player's bargaining power.
		const double SellerFloor = std::min(Asking, Value * (Listing->bBarnFind ? 0.45 : 0.85));
		const double BuyerCeiling = std::max(SellerFloor, Value * 1.1);
		const double Power = Clamp(0.3 + 0.04 * Player.SkillLevel(Skill::People) + 0.02 * Player.SkillLevel(Skill::Trades), 0.1, 0.85);
		const Bargain Deal = NashBargain(BuyerCeiling - SellerFloor, 0.0, 0.0, Power);
		const double NashPrice = std::min(Asking, BuyerCeiling - Deal.PayoffA);
		const double Price = ToDollars(Offer);
		if (Price >= NashPrice * Random.Range(0.97, 1.0) || Price >= Asking)
		{
			Result.Outcome = HaggleOutcome::Accepted;
			Result.Price = Offer;
			Result.Reply = "Deal. Cash only, and it's yours.";
			return Result;
		}
		if (Price < 0.7 * SellerFloor || Listing->Rounds > 3)
		{
			Result.Outcome = HaggleOutcome::WalkedAway;
			Result.Price = Listing->AskingPrice;
			Result.Reply = Listing->Rounds > 3 ? "We're going in circles. Asking price or nothing." : "Is that a joke? Asking price, take it or leave it.";
			return Result;
		}
		Result.Outcome = HaggleOutcome::Countered;
		const double Counter = Listing->Rounds <= 1 ? 0.5 * (Asking + NashPrice) : NashPrice;
		Result.Price = ToMoney(std::round(std::max(Counter, Price) / 50.0) * 50.0);
		Listing->AskingPrice = Result.Price;
		Result.Reply = "I could do " + FormatMoney(Result.Price) + ".";
		return Result;
	}

	bool UsedCarMarket::Take(const std::string& ListingId, OwnedVehicle& OutCar)
	{
		for (auto It = ListingList.begin(); It != ListingList.end(); ++It)
		{
			if (It->Id == ListingId)
			{
				OutCar.ModelId = It->ModelId;
				OutCar.Condition = It->Condition;
				OutCar.MileageKm = It->MileageKm;
				OutCar.Issues = It->Issues;
				OutCar.Restoration = It->Restoration;
				OutCar.bCertified = It->Dealer == DealerKind::Certified;
				OutCar.Build.Parts = It->Parts;
				ListingList.erase(It);
				return true;
			}
		}
		return false;
	}

	const char* RestorationName(RestorationLevel Level)
	{
		switch (Level)
		{
		case RestorationLevel::None: return "Original";
		case RestorationLevel::Driver: return "Driver restoration";
		case RestorationLevel::Show: return "Show restoration";
		case RestorationLevel::Concours: return "Concours restoration";
		case RestorationLevel::RestoMod: return "Resto-mod";
		case RestorationLevel::Count: break;
		}
		return "Original";
	}

	const char* DealerKindName(DealerKind Kind)
	{
		switch (Kind)
		{
		case DealerKind::UsedLot: return "Used car lot";
		case DealerKind::Certified: return "Certified pre-owned";
		case DealerKind::Classic: return "Classic car dealer";
		case DealerKind::Located: return "Located for you";
		case DealerKind::Count: break;
		}
		return "Dealer";
	}

	void UsedCarMarket::ReplaceStock(DealerKind Kind, const std::string& ShowroomId, std::vector<UsedListing> NewStock)
	{
		ListingList.erase(std::remove_if(ListingList.begin(), ListingList.end(), [&](const UsedListing& Listing)
		{
			return Listing.Dealer == Kind && Listing.ShowroomId == ShowroomId;
		}), ListingList.end());
		for (UsedListing& Listing : NewStock)
		{
			Listing.Dealer = Kind;
			Listing.ShowroomId = ShowroomId;
			ListingList.push_back(std::move(Listing));
		}
	}

	std::vector<const UsedListing*> UsedCarMarket::Stock(DealerKind Kind, const std::string& ShowroomId) const
	{
		std::vector<const UsedListing*> Result;
		for (const UsedListing& Listing : ListingList)
		{
			if (Listing.Dealer == Kind && (ShowroomId.empty() || Listing.ShowroomId == ShowroomId))
			{
				Result.push_back(&Listing);
			}
		}
		return Result;
	}

	namespace detail_garage
	{
		std::string ListingId(const char* Prefix, Rng& Random)
		{
			return std::string(Prefix) + "-" + std::to_string(Random.NextU32() % 10000000u);
		}

		const char* RestoModParts[] = {"susp-coilovers", "brakes-bbk", "wheels-plus1", "tires-sport"};
		constexpr const char* CrateEngine = "swap-crate-v8-modern";

		bool SuitsCrateEngine(const VehicleModel& Model)
		{
			return !Model.bElectric && (Model.Body == BodyStyle::Muscle || Model.Body == BodyStyle::Pickup || Model.Body == BodyStyle::Coupe ||
				Model.Body == BodyStyle::Sedan || Model.Body == BodyStyle::SUV || Model.Body == BodyStyle::Wagon);
		}
	}

	std::vector<UsedListing> GenerateCertified(const VehicleCatalog& Catalog, const std::vector<std::string>& BrandIds,
		const std::string& ShowroomId, Rng& Random, int Count)
	{
		std::vector<UsedListing> Result;
		std::vector<const VehicleModel*> Pool;
		for (const VehicleModel& Model : Catalog.Models())
		{
			const bool bBrand = std::find(BrandIds.begin(), BrandIds.end(), Model.BrandId) != BrandIds.end();
			const bool bRecent = Model.AgeYears <= 6 && Model.YearStart < Catalog.CurrentYear();
			if (bBrand && bRecent && !Model.bEventOnly)
			{
				Pool.push_back(&Model);
			}
		}
		for (int Index = 0; Index < Count && !Pool.empty(); ++Index)
		{
			const VehicleModel* Model = Pool[Random.Index(Pool.size())];
			UsedListing Listing;
			Listing.Id = detail_garage::ListingId("cpo", Random);
			Listing.ModelId = Model->Id;
			Listing.Dealer = DealerKind::Certified;
			Listing.ShowroomId = ShowroomId;
			Listing.Condition = Random.Range(0.88, 0.97);
			const int Age = std::max(1, Model->AgeYears + Random.RangeInt(0, 2));
			Listing.MileageKm = std::round(std::max(8000.0, Age * 14000.0 * Random.Range(0.5, 1.1)) / 100.0) * 100.0;
			const double Value = ToDollars(MarketValue(*Model, Listing.Condition, Listing.MileageKm, Listing.Issues));
			Listing.AskingPrice = ToMoney(std::round(Value * 1.12 / 50.0) * 50.0);
			Listing.Seller = "Certified pre-owned";
			Listing.bNoHaggle = true;
			Result.push_back(Listing);
		}
		return Result;
	}

	std::vector<UsedListing> GenerateClassics(const VehicleCatalog& Catalog, Rng& Random, int Count)
	{
		using namespace detail_garage;
		std::vector<UsedListing> Result;
		std::vector<const VehicleModel*> Pool;
		for (const VehicleModel& Model : Catalog.Models())
		{
			if (Model.IsClassic() && !Model.bEventOnly)
			{
				const bool bCollectible = Model.Body == BodyStyle::Muscle || Model.Body == BodyStyle::Sports || Model.Body == BodyStyle::Supercar ||
					Model.Body == BodyStyle::Convertible || Model.Body == BodyStyle::Coupe;
				for (int Weight = 0; Weight < (bCollectible ? 3 : 1); ++Weight)
				{
					Pool.push_back(&Model);
				}
			}
		}
		for (int Index = 0; Index < Count && !Pool.empty(); ++Index)
		{
			const VehicleModel* Model = Pool[Random.Index(Pool.size())];
			UsedListing Listing;
			Listing.Id = ListingId("classic", Random);
			Listing.ModelId = Model->Id;
			Listing.Dealer = DealerKind::Classic;
			const double Roll = Random.NextDouble();
			if (Roll < 0.15)
			{
				Listing.Condition = Random.Range(0.2, 0.4); // project car
				Listing.Issues = {CarIssue::Rust, CarIssue::FadedPaint, CarIssue::WornEngine};
				Listing.Seller = "Project car, needs everything";
			}
			else if (Roll < 0.5)
			{
				Listing.Condition = Random.Range(0.55, 0.75);
				Listing.Restoration = RestorationLevel::Driver;
				Listing.Seller = "Driver-quality classic";
			}
			else if (Roll < 0.8)
			{
				Listing.Condition = Random.Range(0.86, 0.95);
				Listing.Restoration = RestorationLevel::Show;
				Listing.Seller = "Restored, show quality";
			}
			else if (Roll < 0.9)
			{
				Listing.Condition = Random.Range(0.97, 1.0);
				Listing.Restoration = RestorationLevel::Concours;
				Listing.Seller = "Concours winner, numbers matching";
			}
			else
			{
				Listing.Condition = Random.Range(0.88, 0.95);
				Listing.Restoration = RestorationLevel::RestoMod;
				for (const char* PartId : RestoModParts)
				{
					Listing.Parts.push_back(PartId);
				}
				if (SuitsCrateEngine(*Model) && Random.Chance(0.6))
				{
					Listing.Parts.push_back(CrateEngine);
				}
				Listing.Seller = "Resto-mod: classic looks, modern manners";
			}
			Listing.MileageKm = std::round(Random.Range(40000.0, 260000.0) / 100.0) * 100.0;
			OwnedVehicle AsListed;
			AsListed.Condition = Listing.Condition;
			AsListed.MileageKm = Listing.MileageKm;
			AsListed.Issues = Listing.Issues;
			AsListed.Restoration = Listing.Restoration;
			AsListed.Build.Parts = Listing.Parts;
			double Value = ToDollars(ResaleValue(*Model, AsListed));
			if (Listing.Restoration == RestorationLevel::RestoMod)
			{
				Value *= 1.35; // the modern running gear is part of the price
			}
			Listing.AskingPrice = ToMoney(std::round(Value * Random.Range(1.05, 1.2) / 100.0) * 100.0);
			Result.push_back(Listing);
		}
		return Result;
	}

	UsedListing LocateExample(const VehicleModel& Model, const std::string& ShowroomId, Rng& Random)
	{
		UsedListing Listing;
		Listing.Id = detail_garage::ListingId("located", Random);
		Listing.ModelId = Model.Id;
		Listing.Dealer = DealerKind::Located;
		Listing.ShowroomId = ShowroomId;
		if (Model.IsClassic())
		{
			Listing.Condition = Random.Range(0.5, 0.85);
			Listing.MileageKm = std::round(Random.Range(60000.0, 240000.0) / 100.0) * 100.0;
		}
		else
		{
			Listing.Condition = Random.Range(0.6, 0.9);
			Listing.MileageKm = std::round(std::max(5000.0, Model.AgeYears * 15000.0 * Random.Range(0.6, 1.2)) / 100.0) * 100.0;
		}
		if (Listing.Condition < 0.65)
		{
			Listing.Issues.push_back(Random.Chance(0.5) ? CarIssue::WornSuspension : CarIssue::FadedPaint);
		}
		const double Value = ToDollars(MarketValue(Model, Listing.Condition, Listing.MileageKm, Listing.Issues));
		// The showroom adds its finder's fee and a margin.
		Listing.AskingPrice = ToMoney(std::round((Value * 1.08 + 250.0) / 50.0) * 50.0);
		Listing.Seller = "Tracked down by the dealership";
		return Listing;
	}

	RestorationQuote QuoteRestoration(const VehicleModel& Model, const OwnedVehicle& Car, RestorationLevel Level, const Profile& Player,
		const PartCatalog& Parts, bool bCrateEngine)
	{
		using namespace detail_garage;
		RestorationQuote Quote;
		if (Level == RestorationLevel::None || Level == RestorationLevel::Count)
		{
			Quote.Reason = "Pick a restoration level.";
			return Quote;
		}
		if (Model.AgeYears < 10)
		{
			Quote.Reason = "Too new to restore. Repair the faults instead.";
			return Quote;
		}
		if ((Level == RestorationLevel::Concours || Level == RestorationLevel::RestoMod) && !Model.IsClassic())
		{
			Quote.Reason = "Concours and resto-mod builds are for classics (25+ years old).";
			return Quote;
		}
		if (Level == RestorationLevel::RestoMod && Model.bElectric)
		{
			Quote.Reason = "Electric cars don't take resto-mod running gear.";
			return Quote;
		}
		struct LevelData
		{
			double Target;
			double ValueShare;
			double BaseCost;
			int Days;
			int DiyHours;
			int Difficulty;
		};
		const LevelData Data[] = {
			{0.0, 0.0, 0.0, 0, 0, 0},
			{0.75, 0.25, 2500.0, 2, 20, 3},
			{0.93, 0.6, 9000.0, 5, 60, 6},
			{1.0, 0.9, 18000.0, 8, 90, 9},
			{0.92, 0.7, 14000.0, 7, 70, 7},
		};
		const LevelData& Info = Data[static_cast<int>(Level)];
		if (Car.Condition >= Info.Target - 0.01 && Car.Restoration == Level)
		{
			Quote.Reason = "It's already at that standard.";
			return Quote;
		}
		const double GoodValue = ToDollars(Model.MarketPrice);
		const double Remaining = Clamp((Info.Target - Car.Condition) / std::max(0.1, Info.Target), 0.2, 1.0);
		double Cost = (Info.ValueShare * GoodValue + Info.BaseCost) * Remaining;
		double PartsCost = 0.0;
		if (Level == RestorationLevel::RestoMod)
		{
			for (const char* PartId : RestoModParts)
			{
				if (const PartDef* Part = Parts.Find(PartId))
				{
					PartsCost += ToDollars(PartPrice(*Part, Model));
					Quote.PartsFitted.push_back(PartId);
				}
			}
			if (bCrateEngine && SuitsCrateEngine(Model))
			{
				if (const PartDef* Crate = Parts.Find(CrateEngine))
				{
					PartsCost += ToDollars(PartPrice(*Crate, Model));
					Quote.PartsFitted.push_back(CrateEngine);
				}
			}
		}
		Cost += PartsCost;
		Quote.ShopCost = ToMoney(std::round(Cost / 100.0) * 100.0);
		Quote.ShopDays = Info.Days;
		Quote.DiyParts = ToMoney(std::round(((Cost - PartsCost) * 0.45 + PartsCost) / 100.0) * 100.0);
		Quote.DiyHours = Info.DiyHours;
		const int Trades = Player.SkillLevel(Skill::Trades);
		Quote.DiySuccessChance = Clamp(0.35 + 0.08 * (Trades - Info.Difficulty) + 0.3, 0.1, 0.97);
		Quote.ValueBefore = ResaleValue(Model, Car);
		OwnedVehicle After = Car;
		ApplyRestoration(After, Level, Quote, Parts, 1.0);
		Quote.ValueAfter = ResaleValue(Model, After);
		Quote.bAllowed = true;
		return Quote;
	}

	void ApplyRestoration(OwnedVehicle& Car, RestorationLevel Level, const RestorationQuote& Quote, const PartCatalog& Parts, double Quality)
	{
		const double Targets[] = {0.0, 0.75, 0.93, 1.0, 0.92};
		const double Target = Targets[Clamp(static_cast<int>(Level), 0, 4)];
		Car.Condition = std::max(Car.Condition, std::min(1.0, Target * (0.92 + 0.08 * Clamp(Quality, 0.0, 1.0))));
		if (Level == RestorationLevel::Driver)
		{
			// Mechanically sorted; cosmetic flaws stay (except rust, which can't be left).
			Car.Issues.erase(std::remove_if(Car.Issues.begin(), Car.Issues.end(), [](CarIssue Issue)
			{
				return !GetIssueInfo(Issue).bCosmetic || Issue == CarIssue::Rust;
			}), Car.Issues.end());
		}
		else
		{
			Car.Issues.clear();
		}
		if (Level == RestorationLevel::Concours)
		{
			Car.Build.Parts.clear(); // factory-correct
			Car.Build.Tuning = TuningSheet();
		}
		if (Level == RestorationLevel::RestoMod)
		{
			for (const std::string& PartId : Quote.PartsFitted)
			{
				if (const PartDef* Part = Parts.Find(PartId))
				{
					Car.Build.Install(Parts, *Part);
				}
			}
		}
		Car.Restoration = Level;
	}
}
