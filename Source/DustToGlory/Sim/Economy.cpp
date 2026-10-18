#include "Sim/Economy.h"

namespace dtg::sim
{
	const char* MoneyCategoryName(MoneyCategory Category)
	{
		switch (Category)
		{
		case MoneyCategory::Job: return "Job";
		case MoneyCategory::Ministry: return "Ministry";
		case MoneyCategory::Hacking: return "Hacking";
		case MoneyCategory::Event: return "Event";
		case MoneyCategory::Vehicle: return "Vehicle";
		case MoneyCategory::Furniture: return "Furniture";
		case MoneyCategory::Property: return "Property";
		case MoneyCategory::Education: return "Education";
		case MoneyCategory::Medical: return "Medical";
		case MoneyCategory::Fine: return "Fine";
		case MoneyCategory::Tools: return "Tools";
		case MoneyCategory::Reward: return "Reward";
		case MoneyCategory::Other: return "Other";
		case MoneyCategory::Business: return "Business";
		case MoneyCategory::Count: break;
		}
		return "Other";
	}

	void Wallet::Record(Money Signed, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute)
	{
		Transaction Entry;
		Entry.GameMinute = GameMinute;
		Entry.Amount = Signed;
		Entry.Category = Category;
		Entry.Note = Note;
		History.push_back(std::move(Entry));
		if (History.size() > MaxHistory)
		{
			History.erase(History.begin());
		}
	}

	void Wallet::Deposit(Money Amount, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute)
	{
		if (Amount <= 0)
		{
			return;
		}
		BalanceValue += Amount;
		Earned[static_cast<std::size_t>(Category)] += Amount;
		Record(Amount, Category, Note, GameMinute);
	}

	bool Wallet::TrySpend(Money Amount, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute)
	{
		if (Amount < 0 || Amount > BalanceValue)
		{
			return false;
		}
		if (Amount == 0)
		{
			return true;
		}
		BalanceValue -= Amount;
		Spent[static_cast<std::size_t>(Category)] += Amount;
		Record(-Amount, Category, Note, GameMinute);
		return true;
	}

	Money Wallet::Charge(Money Amount, MoneyCategory Category, const std::string& Note, std::int64_t GameMinute)
	{
		const Money Taken = Clamp<Money>(Amount, 0, BalanceValue < 0 ? 0 : BalanceValue);
		if (Taken > 0)
		{
			BalanceValue -= Taken;
			Spent[static_cast<std::size_t>(Category)] += Taken;
			Record(-Taken, Category, Note, GameMinute);
		}
		return Taken;
	}

	Money Wallet::LifetimeEarned(MoneyCategory Category) const
	{
		return Earned[static_cast<std::size_t>(Category)];
	}

	Money Wallet::LifetimeEarnedTotal() const
	{
		Money Total = 0;
		for (const Money Value : Earned)
		{
			Total += Value;
		}
		return Total;
	}

	Money Wallet::LifetimeSpent(MoneyCategory Category) const
	{
		return Spent[static_cast<std::size_t>(Category)];
	}

	void Wallet::RestoreState(Money InBalance, const std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)>& InEarned,
		const std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)>& InSpent, std::vector<Transaction> InHistory)
	{
		BalanceValue = InBalance;
		Earned = InEarned;
		Spent = InSpent;
		History = std::move(InHistory);
		while (History.size() > MaxHistory)
		{
			History.erase(History.begin());
		}
	}
}
