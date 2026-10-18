#pragma once

#include "Sim/SimCommon.h"

#include <array>
#include <string>
#include <vector>

namespace dtg::sim
{
	enum class MoneyCategory : int
	{
		Job,
		Ministry,
		Hacking,
		Event,
		Vehicle,
		Furniture,
		Property,
		Education,
		Medical,
		Fine,
		Tools,
		Reward,
		Other,
		Count
	};

	const char* MoneyCategoryName(MoneyCategory Category);

	/**
	 * Price levels relative to the 2010s baseline the catalog formulas use. The default world is priced like
	 * the mid-1990s US: cheap houses and cars, so an ordinary skilled job (help-desk IT) feels prosperous.
	 * Loaded from Content/Data/economy.json.
	 */
	struct EconomyIndex
	{
		std::string Era = "1995";
		double Vehicles = 0.55;   // a 2010s $30k sedan lists around $16.5k
		double Housing = 1.0;     // home prices in the city plan are already authored in era dollars
		double Goods = 0.55;      // parts, tools, furniture baselines that use modern formulas
		double Wages = 1.0;       // career pay in careers.json is authored in era dollars
		int CalendarYear = 1995;  // year shown on newspapers and receipts
	};

	struct Transaction
	{
		std::int64_t GameMinute = 0;
		Money Amount = 0; // positive = income, negative = expense
		MoneyCategory Category = MoneyCategory::Other;
		std::string Note;
	};

	/** The player's cash plus a short ledger of recent transactions and lifetime totals. */
	class Wallet
	{
	public:
		static constexpr std::size_t MaxHistory = 64;

		explicit Wallet(Money StartingBalance = 0) : BalanceValue(StartingBalance) {}

		Money Balance() const { return BalanceValue; }
		bool CanAfford(Money Amount) const { return Amount <= BalanceValue; }

		void Deposit(Money Amount, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute);
		/** Spends only if affordable. Returns false (and changes nothing) otherwise. */
		bool TrySpend(Money Amount, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute);
		/** Takes up to Amount, never going below zero (fines, hospital bills). Returns what was taken. */
		Money Charge(Money Amount, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute);

		Money LifetimeEarned(MoneyCategory Category) const;
		Money LifetimeEarnedTotal() const;
		Money LifetimeSpent(MoneyCategory Category) const;

		const std::vector<Transaction>& Recent() const { return History; }

		/** Used by save/load. */
		void RestoreState(Money InBalance, const std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)>& InEarned,
			const std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)>& InSpent, std::vector<Transaction> InHistory);
		const std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)>& EarnedTotals() const { return Earned; }
		const std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)>& SpentTotals() const { return Spent; }

	private:
		void Record(Money Signed, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute);

		Money BalanceValue = 0;
		std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)> Earned{};
		std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)> Spent{};
		std::vector<Transaction> History;
	};
}
