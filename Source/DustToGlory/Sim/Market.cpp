#include "Sim/Market.h"

#include "Sim/GameTheory.h"

namespace dtg::sim
{
	namespace detail_market
	{
		const std::vector<std::string>& ReviewAuthors()
		{
			static const std::vector<std::string> Names = {
				"Dana R.", "Marcus T.", "Priya K.", "Jim B.", "Alicia M.", "Hector G.", "Mei L.", "Tom W.", "Grace O.",
				"Luis F.", "Sandra P.", "Kofi A.", "Beth H.", "Raj S.", "Carmen D.", "Ollie N."};
			return Names;
		}

		const std::string& ReviewText(int Stars, Rng& Random)
		{
			static const std::vector<std::string> Five = {"Fantastic. Will be back!", "Best in town, hands down.", "Friendly people and great value.", "Exceeded every expectation."};
			static const std::vector<std::string> Four = {"Really good overall.", "Solid experience, minor waits.", "Would recommend to friends."};
			static const std::vector<std::string> Three = {"It was fine. Nothing special.", "Hit or miss depending on the day.", "Okay, but they could do better."};
			static const std::vector<std::string> Two = {"Disappointing visit.", "Slow and disorganized.", "Not what it used to be."};
			static const std::vector<std::string> One = {"Terrible experience.", "Avoid.", "Nobody seemed to care."};
			switch (Clamp(Stars, 1, 5))
			{
			case 5: return Random.Pick(Five);
			case 4: return Random.Pick(Four);
			case 3: return Random.Pick(Three);
			case 2: return Random.Pick(Two);
			default: return Random.Pick(One);
			}
		}

		double SectorMultiple(const std::string& Sector)
		{
			if (Sector == "Tech") return 12.0;
			if (Sector == "Media") return 9.0;
			if (Sector == "Health") return 8.0;
			if (Sector == "Finance") return 7.0;
			if (Sector == "Auto") return 6.0;
			if (Sector == "Energy") return 6.0;
			if (Sector == "Logistics") return 5.0;
			if (Sector == "Faith") return 3.0;
			return 4.0; // restaurants, retail, services
		}

		double KindBaseWeekly(WorkKind Kind)
		{
			switch (Kind)
			{
			case WorkKind::Song: return 400.0;
			case WorkKind::Book: return 300.0;
			case WorkKind::App: return 600.0;
			case WorkKind::Invention: return 900.0;
			case WorkKind::Film: return 2000.0;
			case WorkKind::VideoGame: return 1200.0;
			case WorkKind::Medicine: return 2500.0;
			case WorkKind::Count: break;
			}
			return 300.0;
		}

		double KindDecay(WorkKind Kind, bool bProtected)
		{
			switch (Kind)
			{
			case WorkKind::Song: return 0.97;
			case WorkKind::Book: return 0.985;
			case WorkKind::App: return 0.94;
			case WorkKind::Invention: return bProtected ? 0.995 : 0.95;
			case WorkKind::Film: return 0.90;
			case WorkKind::VideoGame: return 0.93;
			case WorkKind::Medicine: return bProtected ? 0.997 : 0.95; // generics arrive without a patent
			case WorkKind::Count: break;
			}
			return 0.95;
		}
	}

	double Gaussian(Rng& Random)
	{
		const double U1 = std::max(1e-12, Random.NextDouble());
		const double U2 = Random.NextDouble();
		return std::sqrt(-2.0 * std::log(U1)) * std::cos(2.0 * 3.14159265358979 * U2);
	}

	void Reputation::AddReview(const Review& Value)
	{
		const double Weight = static_cast<double>(std::min(ReviewCount, 50));
		Rating = Clamp((Rating * Weight + Clamp(Value.Stars, 1, 5)) / (Weight + 1.0), 1.0, 5.0);
		ReviewCount += 1;
		Recent.push_back(Value);
		if (Recent.size() > MaxRecent)
		{
			Recent.erase(Recent.begin());
		}
	}

	double Reputation::Score() const
	{
		return Clamp((Rating - 1.0) / 4.0 * 60.0 + (Sentiment + 1.0) * 0.5 * 20.0 + Trust * 20.0, 0.0, 100.0);
	}

	const char* ProblemName(ProblemKind Kind)
	{
		switch (Kind)
		{
		case ProblemKind::OutdatedTech: return "Outdated technology";
		case ProblemKind::PoorService: return "Poor customer service";
		case ProblemKind::WeakMarketing: return "Weak marketing";
		case ProblemKind::CashFlow: return "Cash-flow trouble";
		case ProblemKind::Management: return "Disorganized management";
		case ProblemKind::SecurityHoles: return "Security holes";
		case ProblemKind::RundownFacilities: return "Run-down facilities";
		case ProblemKind::Compliance: return "Compliance issues";
		case ProblemKind::Count: break;
		}
		return "Problem";
	}

	Skill ProblemSkill(ProblemKind Kind)
	{
		switch (Kind)
		{
		case ProblemKind::OutdatedTech: return Skill::Tech;
		case ProblemKind::PoorService: return Skill::People;
		case ProblemKind::WeakMarketing: return Skill::Creativity;
		case ProblemKind::CashFlow: return Skill::Business;
		case ProblemKind::Management: return Skill::Business;
		case ProblemKind::SecurityHoles: return Skill::Tech;
		case ProblemKind::RundownFacilities: return Skill::Trades;
		case ProblemKind::Compliance: return Skill::Business;
		case ProblemKind::Count: break;
		}
		return Skill::Business;
	}

	const char* AttackKindName(AttackKind Kind)
	{
		switch (Kind)
		{
		case AttackKind::SmearCampaign: return "Smear campaign";
		case AttackKind::Allegation: return "Allegation";
		case AttackKind::DoctrineExpose: return "Doctrine expose";
		case AttackKind::PositivePress: return "Positive press";
		case AttackKind::Count: break;
		}
		return "Story";
	}

	const char* WorkKindName(WorkKind Kind)
	{
		switch (Kind)
		{
		case WorkKind::Song: return "Song";
		case WorkKind::Book: return "Book";
		case WorkKind::App: return "App";
		case WorkKind::Invention: return "Invention";
		case WorkKind::Film: return "Film";
		case WorkKind::VideoGame: return "Video game";
		case WorkKind::Medicine: return "Medicine";
		case WorkKind::Count: break;
		}
		return "Work";
	}

	namespace detail_competition
	{
		// Differentiated Bertrand demand q = Base - Own * p + Cross * p_rival with marginal cost Cost.
		constexpr double Base = 1.0;
		constexpr double Own = 1.0;
		constexpr double Cross = 0.5;
		constexpr double Cost = 0.6;

		double EquilibriumPrice()
		{
			return BertrandDuopoly(Base, Own, Cross, Cost, Cost).PriceA;
		}
	}

	double Market::EquilibriumPriceLevel()
	{
		return 1.0; // price levels are normalized so the Nash equilibrium is 1.0
	}

	double Business::CompetitionRevenueFactor() const
	{
		using namespace detail_competition;
		const double Scale = EquilibriumPrice();
		const double P = PriceLevel * Scale;
		const double R = RivalPrice * Scale;
		const double Q = std::max(0.0, Base - Own * P + Cross * R);
		const double QStar = std::max(1e-9, Base - Own * Scale + Cross * Scale);
		return (P * Q) / (Scale * QStar);
	}

	double Business::CompetitionCostFactor() const
	{
		using namespace detail_competition;
		const double Scale = EquilibriumPrice();
		const double Q = std::max(0.0, Base - Own * PriceLevel * Scale + Cross * RivalPrice * Scale);
		const double QStar = std::max(1e-9, Base - Own * Scale + Cross * Scale);
		// Half of costs are fixed, half scale with volume.
		return 0.5 + 0.5 * Q / QStar;
	}

	Money Business::MonthlyRevenue() const
	{
		if (bDissolved)
		{
			return 0;
		}
		const double Factor = (0.55 + 0.6 * Health) * (0.75 + 0.5 * (Rep.Rating - 1.0) / 4.0) * CompetitionRevenueFactor();
		return static_cast<Money>(std::llround(static_cast<double>(BaseRevenue) * Factor));
	}

	Money Business::MonthlyCosts() const
	{
		if (bDissolved)
		{
			return 0;
		}
		return static_cast<Money>(std::llround(static_cast<double>(BaseCosts) * (1.0 + 0.06 * static_cast<double>(Problems.size())) * CompetitionCostFactor()));
	}

	void Market::AddBusiness(Business Value)
	{
		BusinessList.push_back(std::move(Value));
	}

	void Market::ListStock(const std::string& BusinessId, const std::string& Ticker, double Price, std::int64_t Shares, double Volatility)
	{
		Stock Entry;
		Entry.Ticker = Ticker;
		Entry.BusinessId = BusinessId;
		Entry.Price = Price;
		Entry.PreviousClose = Price;
		Entry.SharesOutstanding = Shares;
		Entry.Volatility = Volatility;
		Entry.History.push_back(Price);
		StockList.push_back(Entry);
		if (Business* Owner = FindBusiness(BusinessId))
		{
			Owner->bPublic = true;
			Owner->Ticker = Ticker;
		}
	}

	Business* Market::FindBusiness(const std::string& Id)
	{
		for (Business& Value : BusinessList)
		{
			if (Value.Id == Id) return &Value;
		}
		return nullptr;
	}

	const Business* Market::FindBusiness(const std::string& Id) const
	{
		for (const Business& Value : BusinessList)
		{
			if (Value.Id == Id) return &Value;
		}
		return nullptr;
	}

	Business* Market::FindByBuilding(int BuildingId)
	{
		for (Business& Value : BusinessList)
		{
			if (Value.BuildingId == BuildingId) return &Value;
		}
		return nullptr;
	}

	Stock* Market::FindStock(const std::string& Ticker)
	{
		for (Stock& Value : StockList)
		{
			if (Value.Ticker == Ticker) return &Value;
		}
		return nullptr;
	}

	const Stock* Market::FindStock(const std::string& Ticker) const
	{
		for (const Stock& Value : StockList)
		{
			if (Value.Ticker == Ticker) return &Value;
		}
		return nullptr;
	}

	void Market::PostNews(const std::string& Headline, const std::string& EntityId, double Impact, std::int64_t Minute)
	{
		NewsItem Item;
		Item.Headline = Headline;
		Item.EntityId = EntityId;
		Item.Impact = Impact;
		Item.Minute = Minute;
		NewsFeed.push_back(Item);
		if (NewsFeed.size() > MaxNews)
		{
			NewsFeed.erase(NewsFeed.begin());
		}
		ApplyImpact(EntityId, Impact);
	}

	void Market::ApplyImpact(const std::string& BusinessId, double Impact)
	{
		for (Stock& Entry : StockList)
		{
			if (Entry.BusinessId == BusinessId)
			{
				Entry.PendingImpact += Impact * 0.12; // a strong story moves a stock ~12%
			}
		}
	}

	void Market::GenerateReviews(Business& Value, std::int64_t Minute, Rng& Random)
	{
		const int Count = Random.RangeInt(0, 2);
		for (int Index = 0; Index < Count; ++Index)
		{
			double Quality = 4.4 - 0.45 * static_cast<double>(Value.Problems.size()) + (Value.Health - 0.5);
			if (Value.Sector == "Faith")
			{
				Quality += (Value.Integrity - 0.7) * 2.0;
			}
			const int Stars = Clamp(static_cast<int>(std::lround(Quality + Gaussian(Random) * 0.8)), 1, 5);
			Review Entry;
			Entry.Stars = Stars;
			Entry.Author = Random.Pick(detail_market::ReviewAuthors());
			Entry.Text = detail_market::ReviewText(Stars, Random);
			Entry.Minute = Minute;
			Value.Rep.AddReview(Entry);
		}
	}

	Money Market::DailyUpdate(std::int64_t Minute, Rng& Random)
	{
		Money OwnerIncome = 0;
		for (Business& Value : BusinessList)
		{
			if (Value.bDissolved)
			{
				continue;
			}
			// The main competitor best-responds to the owner's price (Bertrand); left alone, prices sit at equilibrium.
			{
				using namespace detail_competition;
				const double Scale = EquilibriumPrice();
				Value.RivalPrice = BertrandBestResponse(Base, Own, Cross, Cost, Value.PriceLevel * Scale) / Scale;
			}
			const Money Revenue = Value.MonthlyRevenue();
			const Money Profit = Value.MonthlyProfit();
			Value.Cash += Profit / 30;
			const double Margin = Revenue > 0 ? static_cast<double>(Profit) / static_cast<double>(Revenue) : -1.0;
			double Target = 0.55 + 2.0 * Margin - 0.08 * static_cast<double>(Value.Problems.size()) + PolicyHealthBonus;
			Target += Value.Sector == "Health" ? 0.5 * DemandDrag : -DemandDrag;
			if (Value.Cash < 0)
			{
				Target -= 0.2;
			}
			Value.Health = Clamp(Value.Health + (Clamp(Target, 0.0, 1.0) - Value.Health) * 0.04, 0.0, 1.0);

			// New problems creep in over time.
			if (Value.Problems.size() < 4 && Random.Chance(0.01))
			{
				const ProblemKind Kind = static_cast<ProblemKind>(Random.RangeInt(0, static_cast<int>(ProblemKind::Count) - 1));
				bool bHas = false;
				for (const ProblemKind Existing : Value.Problems)
				{
					bHas = bHas || Existing == Kind;
				}
				if (!bHas)
				{
					Value.Problems.push_back(Kind);
				}
			}

			GenerateReviews(Value, Minute, Random);
			const double MoodTarget = (Value.Rep.Rating - 3.0) / 2.0 * 0.6;
			Value.Rep.Sentiment = Clamp(Value.Rep.Sentiment + (MoodTarget - Value.Rep.Sentiment) * 0.1, -1.0, 1.0);
			const double Growth = 0.002 * (Value.Rep.Score() - 45.0) / 50.0;
			Value.Rep.Followers = std::max(0, static_cast<int>(std::lround(Value.Rep.Followers * (1.0 + Growth))));

			// Unsound teaching slowly costs a ministry trust even before anyone exposes it.
			if (Value.Sector == "Faith" && Value.Doctrine < 0.5)
			{
				Value.Rep.Trust = Clamp(Value.Rep.Trust - 0.002, 0.0, 1.0);
			}

			// Occasional news events.
			if (Random.Chance(0.004))
			{
				if (Value.Integrity < 0.5 && Random.Chance(0.6))
				{
					LaunchAttack(Value.Id, AttackKind::Allegation, false, Minute, Random);
				}
				else
				{
					LaunchAttack(Value.Id, AttackKind::PositivePress, false, Minute, Random);
				}
			}

			if (Value.PlayerEquity > 0.0 && Profit > 0)
			{
				OwnerIncome += static_cast<Money>(std::llround(static_cast<double>(Profit) / 30.0 * Value.PlayerEquity));
			}

			// Running out of trust or money closes the doors.
			if (Value.Rep.Trust < 0.12 || Value.Cash < -3 * Value.BaseCosts)
			{
				Value.bDissolved = true;
				PostNews(Value.Name + " closes its doors", Value.Id, -1.0, Minute);
			}
		}

		for (Stock& Entry : StockList)
		{
			const Business* Owner = FindBusiness(Entry.BusinessId);
			double Drift = 0.0003;
			if (Owner)
			{
				Drift += 0.002 * (Owner->Health - 0.5) + 0.001 * (Owner->Rep.Score() - 50.0) / 50.0;
				if (Owner->bDissolved)
				{
					Drift -= 0.2;
				}
			}
			const double Applied = Entry.PendingImpact * 0.5;
			Entry.PendingImpact -= Applied;
			const double Return = Drift + Entry.Volatility * Gaussian(Random) + Applied;
			Entry.PreviousClose = Entry.Price;
			Entry.Price = std::max(0.25, Entry.Price * std::exp(Return));
			Entry.History.push_back(Entry.Price);
			if (Entry.History.size() > MaxHistory)
			{
				Entry.History.erase(Entry.History.begin());
			}
		}
		return OwnerIncome;
	}

	bool Market::BuyShares(const std::string& Ticker, std::int64_t Shares, Wallet& Funds, std::int64_t Minute, std::string* OutError)
	{
		Stock* Entry = FindStock(Ticker);
		if (!Entry || Shares <= 0)
		{
			if (OutError) *OutError = "Unknown stock.";
			return false;
		}
		const Money Cost = ToMoney(Entry->Price * static_cast<double>(Shares) + 10.0);
		if (!Funds.TrySpend(Cost, MoneyCategory::Other, "Bought " + std::to_string(Shares) + " " + Ticker, Minute))
		{
			if (OutError) *OutError = "You need " + FormatMoney(Cost) + ".";
			return false;
		}
		Holding* Held = nullptr;
		for (Holding& Existing : Holdings)
		{
			if (Existing.Ticker == Ticker) Held = &Existing;
		}
		if (!Held)
		{
			Holdings.push_back(Holding{Ticker, 0, 0.0});
			Held = &Holdings.back();
		}
		const double TotalCost = Held->AverageCost * static_cast<double>(Held->Shares) + Entry->Price * static_cast<double>(Shares);
		Held->Shares += Shares;
		Held->AverageCost = TotalCost / static_cast<double>(Held->Shares);
		// Big buys move the price: the player can influence the market.
		Entry->Price *= 1.0 + 5.0 * static_cast<double>(Shares) / static_cast<double>(std::max<std::int64_t>(1, Entry->SharesOutstanding));
		return true;
	}

	bool Market::SellShares(const std::string& Ticker, std::int64_t Shares, Wallet& Funds, std::int64_t Minute, std::string* OutError)
	{
		Stock* Entry = FindStock(Ticker);
		Holding* Held = nullptr;
		for (Holding& Existing : Holdings)
		{
			if (Existing.Ticker == Ticker) Held = &Existing;
		}
		if (!Entry || !Held || Shares <= 0 || Held->Shares < Shares)
		{
			if (OutError) *OutError = "You don't hold that many shares.";
			return false;
		}
		const Money Proceeds = ToMoney(std::max(0.0, Entry->Price * static_cast<double>(Shares) - 10.0));
		Funds.Deposit(Proceeds, MoneyCategory::Other, "Sold " + std::to_string(Shares) + " " + Ticker, Minute);
		Held->Shares -= Shares;
		Entry->Price = std::max(0.25, Entry->Price * (1.0 - 5.0 * static_cast<double>(Shares) / static_cast<double>(std::max<std::int64_t>(1, Entry->SharesOutstanding))));
		if (Held->Shares == 0)
		{
			Holdings.erase(std::remove_if(Holdings.begin(), Holdings.end(), [](const Holding& Value) { return Value.Shares == 0; }), Holdings.end());
		}
		return true;
	}

	Money Market::PortfolioValue() const
	{
		double Total = 0.0;
		for (const Holding& Held : Holdings)
		{
			if (const Stock* Entry = FindStock(Held.Ticker))
			{
				Total += Entry->Price * static_cast<double>(Held.Shares);
			}
		}
		return ToMoney(Total);
	}

	std::int64_t Market::SharesHeld(const std::string& Ticker) const
	{
		for (const Holding& Held : Holdings)
		{
			if (Held.Ticker == Ticker) return Held.Shares;
		}
		return 0;
	}

	Money Market::Valuation(const Business& Value) const
	{
		if (Value.bDissolved)
		{
			return 0;
		}
		const double AnnualProfit = std::max(0.0, ToDollars(Value.MonthlyProfit()) * 12.0);
		const double Assets = ToDollars(Value.BaseRevenue) * 2.0 + std::max(0.0, ToDollars(Value.Cash));
		const double Base = AnnualProfit * detail_market::SectorMultiple(Value.Sector) + Assets;
		const double Total = Base * (0.7 + 0.006 * Value.Rep.Score());
		return ToMoney(std::round(std::max(Total, ToDollars(Value.BaseRevenue)) / 1000.0) * 1000.0);
	}

	Money Market::AskingPrice(const std::string& BusinessId) const
	{
		const Business* Value = FindBusiness(BusinessId);
		if (!Value || Value->bPublic || Value->bDissolved || Value->PlayerEquity >= 1.0)
		{
			return 0;
		}
		const double Price = ToDollars(Valuation(*Value)) * 1.15 * (1.0 - Value->PlayerEquity);
		return ToMoney(std::round(Price / 1000.0) * 1000.0);
	}

	bool Market::SetPriceLevel(const std::string& BusinessId, double Level)
	{
		Business* Value = FindBusiness(BusinessId);
		if (!Value || Value->PlayerEquity < 0.5)
		{
			return false;
		}
		Value->PriceLevel = Clamp(Level, 0.6, 1.6);
		return true;
	}

	bool Market::BuyBusiness(const std::string& BusinessId, Wallet& Funds, std::int64_t Minute, std::string* OutError)
	{
		Business* Value = FindBusiness(BusinessId);
		const Money Price = AskingPrice(BusinessId);
		if (!Value || Price <= 0)
		{
			if (OutError) *OutError = Value && Value->bPublic ? "Public companies trade on the stock exchange." : "This business isn't for sale.";
			return false;
		}
		if (!Funds.TrySpend(Price, MoneyCategory::Property, "Bought " + Value->Name, Minute))
		{
			if (OutError) *OutError = "You need " + FormatMoney(Price) + ".";
			return false;
		}
		Value->PlayerEquity = 1.0;
		PostNews("Local entrepreneur buys " + Value->Name, Value->Id, 0.1, Minute);
		return true;
	}

	TurnaroundOffer Market::ProposeTurnaround(const std::string& BusinessId) const
	{
		TurnaroundOffer Offer;
		const Business* Value = FindBusiness(BusinessId);
		if (!Value || Value->bDissolved || Value->Problems.empty())
		{
			return Offer;
		}
		Offer.BusinessId = BusinessId;
		Offer.Problems = Value->Problems;
		const double Count = static_cast<double>(Value->Problems.size());
		Offer.Cash = ToMoney(std::round(ToDollars(Value->BaseRevenue) * 0.1 * Count / 50.0) * 50.0);
		Offer.Equity = std::min(0.25, 0.03 * Count + (Value->IsFailing() ? 0.05 : 0.0));
		Offer.Hours = 4 * static_cast<int>(Count);
		Offer.bValid = true;
		return Offer;
	}

	double Market::NashFee(const TurnaroundOffer& Offer, const Profile& Player, double* OutPower) const
	{
		const Business* Value = FindBusiness(Offer.BusinessId);
		if (!Offer.bValid || !Value)
		{
			return 0.0;
		}
		// The pie: a few months of the revenue the fixes protect. The consultant's walk-away value is what the
		// same hours would earn elsewhere; the business's walk-away is limping on as it is (zero gain).
		const double Pie = ToDollars(Value->BaseRevenue) * 0.4 * static_cast<double>(Offer.Problems.size());
		const double ConsultantOutside = 25.0 * Offer.Hours;
		const double Negotiator = 0.5 * (Player.SkillLevel(Skill::People) + Player.SkillLevel(Skill::Business));
		const double Power = Clamp(0.35 + 0.03 * Negotiator + (Value->IsFailing() ? 0.1 : 0.0), 0.2, 0.8);
		if (OutPower)
		{
			*OutPower = Power;
		}
		const Bargain Deal = NashBargain(Pie, ConsultantOutside, 0.0, Power);
		return Deal.bDeal ? Deal.PayoffA : ConsultantOutside;
	}

	NegotiationResult Market::Negotiate(const TurnaroundOffer& Offer, Money AskCash, double AskEquity, const Profile& Player, int Round, Rng& Random) const
	{
		NegotiationResult Result;
		const Business* Value = FindBusiness(Offer.BusinessId);
		if (!Offer.bValid || !Value)
		{
			Result.Reply = "There's nothing to negotiate.";
			return Result;
		}
		const double Fee = NashFee(Offer, Player);
		const double CompanyValue = std::max(1.0, ToDollars(Valuation(*Value)));
		const double Asked = ToDollars(std::max<Money>(0, AskCash)) + std::max(0.0, AskEquity) * CompanyValue;
		// At or under the Nash bargaining point: accept (a little noise for human imperfection).
		if (Asked <= Fee * Random.Range(1.0, 1.05))
		{
			Result.Outcome = NegotiationOutcome::Accepted;
			Result.Cash = AskCash;
			Result.Equity = AskEquity;
			Result.Reply = "Deal. Let's shake on it.";
			return Result;
		}
		if (Round >= 3 || Asked > Fee * 2.0)
		{
			Result.Outcome = NegotiationOutcome::Rejected;
			Result.Reply = "We can't do that. Thanks for your time.";
			return Result;
		}
		// Counter at the Nash point. Cash-poor businesses pay more of it in equity.
		const double CashBudget = std::max(0.0, ToDollars(Value->Cash) * 0.5);
		const double CashPart = std::min(Fee * (Value->IsFailing() ? 0.4 : 0.8), CashBudget);
		const double EquityPart = std::min(0.4, std::max(0.0, Fee - CashPart) / CompanyValue);
		Result.Outcome = NegotiationOutcome::Countered;
		Result.Cash = ToMoney(std::round(CashPart / 50.0) * 50.0);
		Result.Equity = std::round(EquityPart * 1000.0) / 1000.0;
		Result.Reply = "Here's what's fair for both of us: " + FormatMoney(Result.Cash) +
			(Result.Equity > 0.0 ? " and " + std::to_string(static_cast<int>(std::lround(Result.Equity * 100.0))) + "% of the business." : ".");
		return Result;
	}

	TurnaroundResult Market::PerformTurnaround(const std::string& BusinessId, Money AgreedCash, double AgreedEquity, Profile& Player,
		double Performance, Wallet& Funds, std::int64_t Minute, Rng& Random)
	{
		TurnaroundResult Result;
		Business* Value = FindBusiness(BusinessId);
		if (!Value || Value->bDissolved)
		{
			return Result;
		}
		const double Perf = Clamp(Performance, 0.0, 1.0);
		Result.HealthBefore = Value->Health;
		std::vector<ProblemKind> Remaining;
		for (const ProblemKind Kind : Value->Problems)
		{
			const Skill Needed = ProblemSkill(Kind);
			const int Level = Player.SkillLevel(Needed);
			const double Chance = Clamp(0.25 + 0.07 * Level + 0.3 * Perf, 0.05, 0.97);
			Result.Attempted += 1;
			if (Random.Chance(Chance))
			{
				Result.Fixed += 1;
				Value->Health = Clamp(Value->Health + 0.12, 0.0, 1.0);
				Value->Rep.Rating = Clamp(Value->Rep.Rating + 0.15, 1.0, 5.0);
				Value->Rep.Trust = Clamp(Value->Rep.Trust + 0.03, 0.0, 1.0);
			}
			else
			{
				Remaining.push_back(Kind);
			}
			Player.AddSkillXp(Needed, 25 + 10 * Level);
		}
		Value->Problems = Remaining;
		Result.StillBroken = Remaining;
		Result.HealthAfter = Value->Health;

		// They pay in proportion to what actually got fixed.
		const double Delivered = Result.Attempted > 0 ? static_cast<double>(Result.Fixed) / Result.Attempted : 0.0;
		Result.CashPaid = static_cast<Money>(std::llround(static_cast<double>(AgreedCash) * (0.4 + 0.6 * Delivered)));
		Funds.Deposit(Result.CashPaid, MoneyCategory::Job, "Turnaround: " + Value->Name, Minute);
		Value->Cash -= Result.CashPaid;
		const double Equity = AgreedEquity * Delivered;
		if (Equity > 0.0)
		{
			if (Value->bPublic)
			{
				if (Stock* Entry = FindStock(Value->Ticker))
				{
					Result.SharesGranted = static_cast<std::int64_t>(std::llround(Equity * static_cast<double>(Entry->SharesOutstanding) * 0.02));
					Holding* Held = nullptr;
					for (Holding& Existing : Holdings)
					{
						if (Existing.Ticker == Entry->Ticker) Held = &Existing;
					}
					if (!Held)
					{
						Holdings.push_back(Holding{Entry->Ticker, 0, 0.0});
						Held = &Holdings.back();
					}
					Held->Shares += Result.SharesGranted;
				}
			}
			else
			{
				Value->PlayerEquity = Clamp(Value->PlayerEquity + Equity, 0.0, 1.0);
				Result.EquityGranted = Equity;
			}
		}
		if (Result.Fixed > 0)
		{
			PostNews(Value->Name + " turns a corner after a consultant's overhaul", Value->Id, 0.3 * Delivered, Minute);
			PlayerRep.Trust = Clamp(PlayerRep.Trust + 0.02 * Result.Fixed, 0.0, 1.0);
			PlayerRep.Followers += 50 * Result.Fixed;
		}
		Result.bValid = true;
		return Result;
	}

	AttackResult Market::LaunchAttack(const std::string& BusinessId, AttackKind Kind, bool bByPlayer, std::int64_t Minute, Rng& Random)
	{
		AttackResult Result;
		Business* Value = FindBusiness(BusinessId);
		if (!Value || Value->bDissolved)
		{
			return Result;
		}
		Reputation& Rep = Value->Rep;
		const double TrustBefore = Rep.Trust;
		const double RatingBefore = Rep.Rating;
		double Impact = 0.0;
		switch (Kind)
		{
		case AttackKind::SmearCampaign:
		{
			// Organisations with real integrity tend to expose a smear and the people behind it.
			if (bByPlayer && Random.Chance(0.6 * Value->Integrity))
			{
				Result.bBackfired = true;
				PlayerRep.Trust = Clamp(PlayerRep.Trust - 0.2, 0.0, 1.0);
				PlayerRep.Sentiment = Clamp(PlayerRep.Sentiment - 0.4, -1.0, 1.0);
				Rep.Trust = Clamp(Rep.Trust + 0.05, 0.0, 1.0);
				Result.Headline = "Smear campaign against " + Value->Name + " traced back to its source";
				Impact = 0.1;
				break;
			}
			if (Random.Chance(0.25 + 0.6 * (1.0 - Rep.Trust)))
			{
				Result.bLanded = true;
				Rep.Rating = Clamp(Rep.Rating - 0.5 * (1.3 - Rep.Trust), 1.0, 5.0);
				Rep.Trust = Clamp(Rep.Trust - 0.12, 0.0, 1.0);
				Rep.Sentiment = Clamp(Rep.Sentiment - 0.4, -1.0, 1.0);
				Rep.Followers = static_cast<int>(Rep.Followers * 0.85);
				Result.Headline = "Damaging rumors about " + Value->Name + " spread online";
				Impact = -0.5;
			}
			else
			{
				Result.Headline = "Rumors about " + Value->Name + " fail to gain traction";
			}
			break;
		}
		case AttackKind::Allegation:
		{
			Result.bLanded = true;
			Result.bConfirmed = Random.Chance(1.0 - Value->Integrity);
			if (Result.bConfirmed)
			{
				Rep.Trust = Clamp(Rep.Trust - 0.3, 0.0, 1.0);
				Rep.Rating = Clamp(Rep.Rating - 0.8, 1.0, 5.0);
				Rep.Sentiment = Clamp(Rep.Sentiment - 0.6, -1.0, 1.0);
				Result.Headline = "Investigation confirms allegations against " + Value->Name;
				Impact = -0.8;
			}
			else
			{
				Rep.Trust = Clamp(Rep.Trust + 0.05, 0.0, 1.0);
				Result.Headline = Value->Name + " cleared after allegations are disproven";
				Impact = 0.1;
				if (bByPlayer)
				{
					PlayerRep.Trust = Clamp(PlayerRep.Trust - 0.1, 0.0, 1.0);
				}
			}
			break;
		}
		case AttackKind::DoctrineExpose:
		{
			if (Value->Sector == "Faith" && Value->Doctrine < 0.5)
			{
				Result.bLanded = true;
				Result.bConfirmed = true;
				Rep.Trust = Clamp(Rep.Trust - 0.35, 0.0, 1.0);
				Rep.Rating = Clamp(Rep.Rating - 0.7, 1.0, 5.0);
				Rep.Followers = static_cast<int>(Rep.Followers * 0.7);
				Result.Headline = "Former members speak out about teaching at " + Value->Name;
				Impact = -0.7;
			}
			else
			{
				Result.Headline = "Critics find nothing amiss at " + Value->Name;
				if (bByPlayer)
				{
					PlayerRep.Trust = Clamp(PlayerRep.Trust - 0.05, 0.0, 1.0);
				}
			}
			break;
		}
		case AttackKind::PositivePress:
		{
			Result.bLanded = true;
			Rep.Trust = Clamp(Rep.Trust + 0.05, 0.0, 1.0);
			Rep.Rating = Clamp(Rep.Rating + 0.2, 1.0, 5.0);
			Rep.Sentiment = Clamp(Rep.Sentiment + 0.3, -1.0, 1.0);
			Rep.Followers = static_cast<int>(Rep.Followers * 1.1) + 10;
			Result.Headline = Value->Name + " wins praise from the community";
			Impact = 0.3;
			break;
		}
		case AttackKind::Count:
			break;
		}
		Result.TrustDelta = Rep.Trust - TrustBefore;
		Result.RatingDelta = Rep.Rating - RatingBefore;
		if (Rep.Trust < 0.12 && !Value->bPublic)
		{
			Value->bDissolved = true;
			Result.bCollapsed = true;
			Result.Headline += ". " + Value->Name + " has dissolved";
			Impact = -1.0;
		}
		if (!Result.Headline.empty())
		{
			PostNews(Result.Headline, Value->Id, Impact, Minute);
		}
		return Result;
	}

	void Market::Restore(std::vector<Business> InBusinesses, std::vector<Stock> InStocks, std::vector<Holding> InHoldings,
		std::vector<NewsItem> InNews, Reputation InPlayerRep)
	{
		BusinessList = std::move(InBusinesses);
		StockList = std::move(InStocks);
		Holdings = std::move(InHoldings);
		NewsFeed = std::move(InNews);
		PlayerRep = std::move(InPlayerRep);
	}

	CreativeWork& IpPortfolio::Create(WorkKind Kind, const std::string& Title, Profile& Player, int EffortHours, double Performance,
		double PlayerFame, Rng& Random)
	{
		struct Weights
		{
			Skill A;
			double WeightA;
			Skill B;
			double WeightB;
		};
		Weights Mix{Skill::Creativity, 0.8, Skill::People, 0.2};
		switch (Kind)
		{
		case WorkKind::Song: Mix = {Skill::Creativity, 0.8, Skill::People, 0.2}; break;
		case WorkKind::Book: Mix = {Skill::Creativity, 0.6, Skill::Teaching, 0.4}; break;
		case WorkKind::App: Mix = {Skill::Tech, 0.8, Skill::Creativity, 0.2}; break;
		case WorkKind::Invention: Mix = {Skill::Science, 0.5, Skill::Trades, 0.5}; break;
		case WorkKind::Film: Mix = {Skill::Creativity, 0.6, Skill::People, 0.4}; break;
		case WorkKind::VideoGame: Mix = {Skill::Tech, 0.5, Skill::Creativity, 0.5}; break;
		case WorkKind::Medicine: Mix = {Skill::Science, 0.6, Skill::Medicine, 0.4}; break;
		case WorkKind::Count: break;
		}
		const double SkillScore = (Mix.WeightA * Player.SkillLevel(Mix.A) + Mix.WeightB * Player.SkillLevel(Mix.B)) / 10.0;
		const double Effort = 1.0 - std::exp(-std::max(0, EffortHours) / 40.0);
		const double Quality = Clamp(0.15 + 0.5 * SkillScore + 0.25 * Effort + 0.2 * Clamp(Performance, 0.0, 1.0) + 0.07 * Gaussian(Random), 0.02, 1.0);
		Player.AddSkillXp(Mix.A, 5 * std::max(1, EffortHours));
		Player.AddSkillXp(Mix.B, 2 * std::max(1, EffortHours));

		CreativeWork Work;
		Work.Kind = Kind;
		Work.Title = Title.empty() ? std::string("Untitled ") + WorkKindName(Kind) : Title;
		Work.Id = Slugify(Work.Title) + "-" + std::to_string(Random.NextU32() % 100000u);
		Work.Quality = std::round(Quality * 100.0) / 100.0;
		Work.Hype = 1.0 + Clamp(PlayerFame, 0.0, 100.0) / 100.0;
		WorkList.push_back(Work);
		return WorkList.back();
	}

	Money IpPortfolio::ProtectionCost(WorkKind Kind)
	{
		if (Kind == WorkKind::Medicine) return Dollars(6000);
		return Kind == WorkKind::Invention ? Dollars(4500) : Dollars(35);
	}

	CreativeWork* IpPortfolio::Find(const std::string& Id)
	{
		for (CreativeWork& Work : WorkList)
		{
			if (Work.Id == Id) return &Work;
		}
		return nullptr;
	}

	bool IpPortfolio::Protect(const std::string& WorkId, Wallet& Funds, std::int64_t Minute, std::string* OutError)
	{
		CreativeWork* Work = Find(WorkId);
		if (!Work || Work->bProtected)
		{
			if (OutError) *OutError = Work ? "Already protected." : "Unknown work.";
			return false;
		}
		const Money Cost = ProtectionCost(Work->Kind);
		if (!Funds.TrySpend(Cost, MoneyCategory::Other, std::string(Work->Kind == WorkKind::Invention ? "Patent: " : "Copyright: ") + Work->Title, Minute))
		{
			if (OutError) *OutError = "You need " + FormatMoney(Cost) + ".";
			return false;
		}
		Work->bProtected = true;
		return true;
	}

	Money IpPortfolio::LicenseOffer(const std::string& WorkId) const
	{
		for (const CreativeWork& Work : WorkList)
		{
			if (Work.Id == WorkId && Work.Licensee.empty())
			{
				const double Weekly = detail_market::KindBaseWeekly(Work.Kind) * Work.Quality * Work.Quality * 4.0 * Work.Hype;
				return ToMoney(std::round(Weekly * 20.0 / 50.0) * 50.0);
			}
		}
		return 0;
	}

	bool IpPortfolio::License(const std::string& WorkId, const std::string& BusinessId, Wallet& Funds, std::int64_t Minute)
	{
		const Money Upfront = LicenseOffer(WorkId);
		CreativeWork* Work = Find(WorkId);
		if (!Work || Upfront <= 0)
		{
			return false;
		}
		Funds.Deposit(Upfront, MoneyCategory::Reward, "License: " + Work->Title, Minute);
		Work->Licensee = BusinessId;
		Work->LicenseeShare = 0.4;
		return true;
	}

	std::vector<WeeklyRoyalty> IpPortfolio::WeeklyRoyalties(Rng& Random)
	{
		std::vector<WeeklyRoyalty> Payments;
		for (CreativeWork& Work : WorkList)
		{
			if (Work.Kind == WorkKind::Medicine && !Work.bApproved)
			{
				continue; // no sales until it's approved
			}
			const double Decay = std::max(0.05, std::pow(detail_market::KindDecay(Work.Kind, Work.bProtected), Work.WeeksOut));
			double Amount = detail_market::KindBaseWeekly(Work.Kind) * Work.Quality * Work.Quality * 4.0 * Work.Hype * Decay;
			Amount *= Work.bProtected ? 1.0 : 0.6; // unprotected works lose sales to knock-offs
			Amount *= 1.0 - Work.LicenseeShare;
			Amount *= Random.Range(0.8, 1.2);
			if (Work.Quality > 0.85 && Random.Chance(0.01))
			{
				Amount *= 10.0; // it went viral
			}
			Work.WeeksOut += 1;
			const Money Paid = ToMoney(std::round(Amount));
			Work.LifetimeRoyalties += Paid;
			if (Paid > 0)
			{
				Payments.push_back(WeeklyRoyalty{Work.Id, Paid});
			}
		}
		return Payments;
	}

	Money IpPortfolio::TrialCost(int Phase, bool bFastTrack)
	{
		const Money Costs[] = {Dollars(15000), Dollars(45000), Dollars(120000)};
		const Money Cost = Costs[Clamp(Phase, 0, 2)];
		return bFastTrack ? Cost / 2 : Cost;
	}

	int IpPortfolio::TrialDays(bool bFastTrack)
	{
		return bFastTrack ? 3 : 14;
	}

	TrialResult IpPortfolio::AdvanceTrial(const std::string& WorkId, bool bFastTrack, Wallet& Funds, std::int64_t Minute, Rng& Random)
	{
		TrialResult Result;
		CreativeWork* Work = Find(WorkId);
		if (!Work || Work->Kind != WorkKind::Medicine)
		{
			Result.Message = "That isn't a medicine.";
			return Result;
		}
		if (Work->bApproved || Work->bShelved)
		{
			Result.Message = Work->bApproved ? "Already approved." : "That program was shelved after a failed trial.";
			return Result;
		}
		if (Work->bInTrial)
		{
			if (Minute < Work->TrialReadyMinute)
			{
				Result.Message = "The trial is still running.";
				return Result;
			}
			// Results are in. Better science passes more often; later phases are harder.
			Work->bInTrial = false;
			Result.bFinished = true;
			const double PassChance = Clamp(0.45 + 0.5 * Work->Quality - 0.08 * Work->TrialPhase, 0.1, 0.95);
			if (!Random.Chance(PassChance))
			{
				Work->bShelved = true;
				Result.Message = "Phase " + std::to_string(Work->TrialPhase + 1) + " missed its goals. The program is shelved.";
				return Result;
			}
			Work->TrialPhase += 1;
			Result.bPassed = true;
			if (Work->TrialPhase >= 3)
			{
				Work->bApproved = true;
				Work->WeeksOut = 0;
				Result.bApproved = true;
				Result.Message = Work->Title + " is approved! It goes on sale this week.";
			}
			else
			{
				Result.Message = "Phase " + std::to_string(Work->TrialPhase) + " succeeded.";
			}
			return Result;
		}
		const Money Cost = TrialCost(Work->TrialPhase, bFastTrack);
		if (!Funds.TrySpend(Cost, MoneyCategory::Business, "Clinical trial: " + Work->Title, Minute))
		{
			Result.Message = "The next trial phase costs " + FormatMoney(Cost) + ".";
			return Result;
		}
		Work->bInTrial = true;
		Work->TrialReadyMinute = Minute + static_cast<std::int64_t>(TrialDays(bFastTrack)) * 24 * 60;
		Result.bStarted = true;
		Result.Message = "Phase " + std::to_string(Work->TrialPhase + 1) + " is underway. Results in " + std::to_string(TrialDays(bFastTrack)) + " days.";
		return Result;
	}
}
