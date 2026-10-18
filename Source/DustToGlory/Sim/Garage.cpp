#include "Sim/Garage.h"

#include "Sim/GameTheory.h"

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

	Money MarketValue(const VehicleModel& Model, double Condition, double MileageKm, const std::vector<CarIssue>& Issues)
	{
		double Value = ToDollars(Model.MarketPrice);
		Value *= 1.0 - 0.3 * std::min(1.0, std::max(0.0, MileageKm) / 300000.0);
		Value *= 0.35 + 0.65 * Clamp(Condition, 0.0, 1.0);
		for (const CarIssue Issue : Issues)
		{
			Value *= 1.0 - GetIssueInfo(Issue).ValuePenalty;
		}
		return ToMoney(std::round(std::max(150.0, Value) / 25.0) * 25.0);
	}

	Money ResaleValue(const VehicleModel& Model, const OwnedVehicle& Car)
	{
		return MarketValue(Model, Car.Condition, Car.MileageKm, Car.Issues);
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
				// Buyers pay for some upgrades, not all of them.
				Offer += 0.3 * ToDollars(PartPrice(*Part, Model));
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
		ListingList.clear();
		std::vector<const VehicleModel*> Pool;
		for (const VehicleModel& Model : Catalog.Models())
		{
			if (!Model.bEventOnly)
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
		Listing->Rounds += 1;
		const double Value = ToDollars(MarketValue(*Model, Listing->Condition, Listing->MileageKm, Listing->Issues));
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
				ListingList.erase(It);
				return true;
			}
		}
		return false;
	}
}
