// The living economy: businesses with reputations, a stock market the player can move, business
// ownership, turnaround consulting paid in cash and equity, reputation attacks, and creative IP.
#pragma once

#include "Sim/Careers.h"
#include "Sim/Economy.h"
#include "Sim/SimCommon.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	struct Review
	{
		int Stars = 5;
		std::string Text;
		std::string Author;
		std::int64_t Minute = 0;
	};

	/** Public perception: stars from reviews, social-media mood, audience size and underlying trust. */
	struct Reputation
	{
		static constexpr std::size_t MaxRecent = 8;

		double Rating = 4.0;      // 1..5 running average
		int ReviewCount = 0;
		double Sentiment = 0.0;   // -1..1 social media mood
		int Followers = 100;
		double Trust = 0.7;       // 0..1 how much people believe in them; drives resilience
		std::vector<Review> Recent;

		void AddReview(const Review& Value);
		/** 0..100 headline score shown in menus. */
		double Score() const;
	};

	enum class ProblemKind : int
	{
		OutdatedTech,
		PoorService,
		WeakMarketing,
		CashFlow,
		Management,
		SecurityHoles,
		RundownFacilities,
		Compliance,
		Count
	};

	const char* ProblemName(ProblemKind Kind);
	/** The skill that fixes a problem. */
	Skill ProblemSkill(ProblemKind Kind);

	struct Business
	{
		std::string Id;
		std::string Name;
		std::string Sector;            // Restaurant, Retail, Tech, Media, Auto, Health, Faith, Logistics, Energy, Finance
		int BuildingId = -1;           // city building it occupies, -1 for off-map companies
		bool bPublic = false;
		std::string Ticker;
		Money BaseRevenue = 0;         // monthly revenue at full health
		Money BaseCosts = 0;           // monthly costs
		Money Cash = 0;
		double Health = 0.7;           // 0..1 financial health; below 0.3 is failing
		std::vector<ProblemKind> Problems;
		double Integrity = 0.8;        // hidden: how clean they really are
		double Doctrine = 1.0;         // hidden, faith organisations: how sound their teaching is
		double PlayerEquity = 0.0;     // 0..1 share owned by the player (private businesses)
		double PriceLevel = 1.0;       // relative price the owner sets (1 = market equilibrium)
		double RivalPrice = 1.0;       // the main competitor's price, which best-responds each day
		bool bDissolved = false;
		Reputation Rep;

		bool IsFailing() const { return Health < 0.3; }
		/** Demand and margin effect of the current price war (1 at the Nash equilibrium). */
		double CompetitionRevenueFactor() const;
		double CompetitionCostFactor() const;
		Money MonthlyRevenue() const;
		Money MonthlyCosts() const;
		Money MonthlyProfit() const { return MonthlyRevenue() - MonthlyCosts(); }
	};

	struct Stock
	{
		std::string Ticker;
		std::string BusinessId;
		double Price = 20.0;           // dollars per share
		double PreviousClose = 20.0;
		std::int64_t SharesOutstanding = 10000000;
		double Volatility = 0.02;      // daily
		std::vector<double> History;   // recent closes, oldest first
		double PendingImpact = 0.0;    // news waiting to be priced in
	};

	struct Holding
	{
		std::string Ticker;
		std::int64_t Shares = 0;
		double AverageCost = 0.0;
	};

	struct NewsItem
	{
		std::string Headline;
		std::string EntityId;
		double Impact = 0.0;           // -1..1
		std::int64_t Minute = 0;
	};

	struct TurnaroundOffer
	{
		std::string BusinessId;
		std::vector<ProblemKind> Problems;
		Money Cash = 0;
		double Equity = 0.0;           // share of the business offered
		int Hours = 8;
		bool bValid = false;
	};

	enum class NegotiationOutcome : int
	{
		Accepted,
		Countered,
		Rejected
	};

	struct NegotiationResult
	{
		NegotiationOutcome Outcome = NegotiationOutcome::Rejected;
		Money Cash = 0;
		double Equity = 0.0;
		std::string Reply;
	};

	struct TurnaroundResult
	{
		bool bValid = false;
		int Fixed = 0;
		int Attempted = 0;
		std::vector<ProblemKind> StillBroken;
		double HealthBefore = 0.0;
		double HealthAfter = 0.0;
		Money CashPaid = 0;
		double EquityGranted = 0.0;
		std::int64_t SharesGranted = 0;
	};

	enum class AttackKind : int
	{
		SmearCampaign,  // spreading damaging claims regardless of truth
		Allegation,     // a specific accusation that an investigation will confirm or clear
		DoctrineExpose, // faith organisations: unsound teaching brought to light
		PositivePress,  // good coverage, interviews, a viral act of kindness
		Count
	};

	const char* AttackKindName(AttackKind Kind);

	struct AttackResult
	{
		bool bLanded = false;
		bool bBackfired = false;    // the attacker was exposed instead
		bool bConfirmed = false;    // the claim turned out to be true
		bool bCollapsed = false;    // trust fell so far the organisation dissolved
		double RatingDelta = 0.0;
		double TrustDelta = 0.0;
		std::string Headline;
	};

	/** Businesses, the stock exchange, the player's portfolio and the news feed. */
	class Market
	{
	public:
		static constexpr std::size_t MaxNews = 30;
		static constexpr std::size_t MaxHistory = 60;

		void AddBusiness(Business Value);
		void ListStock(const std::string& BusinessId, const std::string& Ticker, double Price, std::int64_t Shares, double Volatility);

		Business* FindBusiness(const std::string& Id);
		const Business* FindBusiness(const std::string& Id) const;
		Business* FindByBuilding(int BuildingId);
		Stock* FindStock(const std::string& Ticker);
		const Stock* FindStock(const std::string& Ticker) const;
		const std::vector<Business>& Businesses() const { return BusinessList; }
		const std::vector<Stock>& Stocks() const { return StockList; }
		const std::vector<Holding>& Portfolio() const { return Holdings; }
		const std::vector<NewsItem>& News() const { return NewsFeed; }

		/** Once per game day: trading, reviews, random events. Returns owner income paid to the player. */
		Money DailyUpdate(std::int64_t Minute, Rng& Random);

		// Trading
		bool BuyShares(const std::string& Ticker, std::int64_t Shares, Wallet& Funds, std::int64_t Minute, std::string* OutError);
		bool SellShares(const std::string& Ticker, std::int64_t Shares, Wallet& Funds, std::int64_t Minute, std::string* OutError);
		Money PortfolioValue() const;
		std::int64_t SharesHeld(const std::string& Ticker) const;

		// Ownership
		Money Valuation(const Business& Value) const;
		Money AskingPrice(const std::string& BusinessId) const;
		bool BuyBusiness(const std::string& BusinessId, Wallet& Funds, std::int64_t Minute, std::string* OutError);
		/** Owner sets a price level; the rival best-responds at the next daily update. */
		bool SetPriceLevel(const std::string& BusinessId, double Level);
		/** The Bertrand-Nash equilibrium price level the "Nash advisor" recommends. */
		static double EquilibriumPriceLevel();

		// Turnarounds
		TurnaroundOffer ProposeTurnaround(const std::string& BusinessId) const;
		/** The Nash bargaining fee (in dollars of value) for this turnaround, and the player's bargaining power. */
		double NashFee(const TurnaroundOffer& Offer, const Profile& Player, double* OutPower = nullptr) const;
		/** Ask for cash and/or equity. Offers at or under the Nash bargaining fee are accepted. */
		NegotiationResult Negotiate(const TurnaroundOffer& Offer, Money AskCash, double AskEquity, const Profile& Player, int Round, Rng& Random) const;
		/** Do the work. Performance 0..1 from the minigame; each problem's fix chance depends on the matching skill. */
		TurnaroundResult PerformTurnaround(const std::string& BusinessId, Money AgreedCash, double AgreedEquity, Profile& Player,
			double Performance, Wallet& Funds, std::int64_t Minute, Rng& Random);

		// Reputation
		AttackResult LaunchAttack(const std::string& BusinessId, AttackKind Kind, bool bByPlayer, std::int64_t Minute, Rng& Random);
		/** Pro-business laws nudge every company's health target. */
		double PolicyHealthBonus = 0.0;

		/** Player-side reputation (fame), which hype, deals and backfires all touch. */
		Reputation& PlayerReputation() { return PlayerRep; }
		const Reputation& PlayerReputation() const { return PlayerRep; }

		void PostNews(const std::string& Headline, const std::string& EntityId, double Impact, std::int64_t Minute);

		/** Save/load. */
		void Restore(std::vector<Business> InBusinesses, std::vector<Stock> InStocks, std::vector<Holding> InHoldings,
			std::vector<NewsItem> InNews, Reputation InPlayerRep);

	private:
		void GenerateReviews(Business& Value, std::int64_t Minute, Rng& Random);
		void ApplyImpact(const std::string& BusinessId, double Impact);

		std::vector<Business> BusinessList;
		std::vector<Stock> StockList;
		std::vector<Holding> Holdings;
		std::vector<NewsItem> NewsFeed;
		Reputation PlayerRep;
	};

	// ------------------------------------------------------------------ Intellectual property

	enum class WorkKind : int
	{
		Song,
		Book,
		App,
		Invention,
		Film,
		VideoGame,
		Count
	};

	const char* WorkKindName(WorkKind Kind);

	struct CreativeWork
	{
		std::string Id;
		std::string Title;
		WorkKind Kind = WorkKind::Song;
		double Quality = 0.5;      // 0..1
		double Hype = 1.0;         // launch buzz multiplier
		int WeeksOut = 0;
		Money LifetimeRoyalties = 0;
		bool bProtected = false;   // copyright registered / patent granted
		std::string Licensee;      // business id that licensed it, empty when none
		double LicenseeShare = 0.0;
	};

	struct WeeklyRoyalty
	{
		std::string WorkId;
		Money Amount = 0;
	};

	class IpPortfolio
	{
	public:
		/** Creates a work. Quality comes from the relevant skills, effort hours and the creation minigame. */
		CreativeWork& Create(WorkKind Kind, const std::string& Title, Profile& Player, int EffortHours, double Performance,
			double PlayerFame, Rng& Random);
		/** Registering copyright or filing a patent: costs money, protects royalties from knock-offs. */
		static Money ProtectionCost(WorkKind Kind);
		bool Protect(const std::string& WorkId, Wallet& Funds, std::int64_t Minute, std::string* OutError);
		/** Licensing: an upfront payment now in exchange for a share of future royalties. */
		Money LicenseOffer(const std::string& WorkId) const;
		bool License(const std::string& WorkId, const std::string& BusinessId, Wallet& Funds, std::int64_t Minute);
		/** Weekly royalties (already split with any licensee). */
		std::vector<WeeklyRoyalty> WeeklyRoyalties(Rng& Random);

		const std::vector<CreativeWork>& Works() const { return WorkList; }
		CreativeWork* Find(const std::string& Id);
		void Restore(std::vector<CreativeWork> InWorks) { WorkList = std::move(InWorks); }

	private:
		std::vector<CreativeWork> WorkList;
	};

	/** Gaussian sample via Box-Muller (used for prices and quality rolls). */
	double Gaussian(Rng& Random);
}
