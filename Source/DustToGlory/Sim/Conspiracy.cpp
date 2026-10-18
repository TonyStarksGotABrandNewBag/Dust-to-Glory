#include "Sim/Conspiracy.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	namespace detail_conspiracy
	{
		/** How much each role shifts the scheme: their share of the take, and what they do to the heat. */
		struct RoleTraits
		{
			double Share;      // baseline cut they expect
			double HeatPerDay; // what having them on the crew adds (negative: they clean up)
			double FlipBase;   // how ready they are to talk
		};

		RoleTraits TraitsFor(InsiderRole Role)
		{
			switch (Role)
			{
			case InsiderRole::LabTech: return {0.14, 0.0025, 0.010};        // the one who actually touched it
			case InsiderRole::Courier: return {0.06, 0.0015, 0.016};        // paid least, knows least, talks easiest
			case InsiderRole::HealthOfficial: return {0.18, 0.0030, 0.005}; // the most to lose
			case InsiderRole::Fixer: return {0.11, -0.0030, 0.004};
			case InsiderRole::Count: break;
			}
			return {0.1, 0.002, 0.01};
		}
	}

	const char* InsiderRoleName(InsiderRole Role)
	{
		switch (Role)
		{
		case InsiderRole::LabTech: return "Lab technician";
		case InsiderRole::Courier: return "Courier";
		case InsiderRole::HealthOfficial: return "Health official";
		case InsiderRole::Fixer: return "Fixer";
		case InsiderRole::Count: break;
		}
		return "Insider";
	}

	const char* InsiderRoleDescription(InsiderRole Role)
	{
		switch (Role)
		{
		case InsiderRole::LabTech: return "Works inside the research lab. Without one there's no sample and no scheme.";
		case InsiderRole::Courier: return "Moves things without paperwork. Keeps the early trail short -- but has the least to lose by talking.";
		case InsiderRole::HealthOfficial: return "Can misfile a report and lose a week. The outbreak runs longer, and your treatment sells harder.";
		case InsiderRole::Fixer: return "Keeps investigators chasing the wrong people. Expensive, and the only one who actually slows the case down.";
		case InsiderRole::Count: break;
		}
		return "";
	}

	const char* SchemeStageName(SchemeStage Stage)
	{
		switch (Stage)
		{
		case SchemeStage::None: return "No scheme";
		case SchemeStage::Planning: return "Planning";
		case SchemeStage::Released: return "Live";
		case SchemeStage::Unraveling: return "Unraveling";
		case SchemeStage::Exposed: return "Exposed";
		case SchemeStage::GotAway: return "Cold case";
		case SchemeStage::Count: break;
		}
		return "";
	}

	bool ConspiracySystem::HasRole(InsiderRole Role) const
	{
		for (const Insider& Member : Crew)
		{
			if (Member.Role == Role && !Member.bFlipped)
			{
				return true;
			}
		}
		return false;
	}

	Money ConspiracySystem::ExpectedTake(double MedicineQuality, Money Treasury)
	{
		// A state-wide treatment contract at a squeezed price, floored so an early scheme is still worth bargaining over.
		const double Doses = 40000.0;
		const double PerDose = 50.0 + 120.0 * Clamp(MedicineQuality, 0.0, 1.0);
		return std::max(Dollars(250000), ToMoney(Doses * PerDose) + Treasury / 20);
	}

	RecruitQuote ConspiracySystem::QuoteInsider(InsiderRole Role, const std::string& Name, double Affinity, double Awareness, int BusinessSkill,
		double MedicineQuality, Money Treasury) const
	{
		RecruitQuote Quote;
		if (Role == InsiderRole::Count)
		{
			Quote.Reason = "Pick a role.";
			return Quote;
		}
		for (const Insider& Member : Crew)
		{
			if (Member.Name == Name)
			{
				Quote.Reason = Name + " is already in.";
				return Quote;
			}
		}
		// Someone who has woken up to what the world is won't help you poison it, and neither will someone who
		// dislikes you. Affinity and awareness are the gate.
		if (Awareness > 0.6)
		{
			Quote.Reason = Name + " sees too much of what's going on to take part in this.";
			return Quote;
		}
		if (Affinity < 0.1)
		{
			Quote.Reason = Name + " doesn't trust you anywhere near enough for a conversation like that.";
			return Quote;
		}
		const detail_conspiracy::RoleTraits Traits = detail_conspiracy::TraitsFor(Role);
		const double Take = ToDollars(ExpectedTake(MedicineQuality, Treasury));
		// The bargain is over what this role is worth to the scheme. The insider's disagreement point is what
		// saying yes risks for them; the player's bargaining power grows with business skill and their loyalty.
		const double RoleValue = Take * Traits.Share * 1.6;
		const double TheirRisk = Take * Traits.Share * (0.8 - 0.3 * Clamp(Affinity, 0.0, 1.0));
		const double Power = Clamp(0.4 + 0.03 * BusinessSkill + 0.2 * Clamp(Affinity, 0.0, 1.0) - 0.3 * HeatValue, 0.15, 0.85);
		const Bargain Deal = NashBargain(std::max(0.0, RoleValue - TheirRisk), 0.0, TheirRisk, Power);
		if (!Deal.bDeal)
		{
			Quote.Reason = Name + " won't take the risk at any price you could pay.";
			return Quote;
		}
		Quote.Cut = Clamp((TheirRisk + Deal.PayoffB) / std::max(1.0, Take), 0.01, 0.4);
		Quote.Retainer = ToMoney(Take * Quote.Cut * 0.08);
		Quote.Loyalty = Clamp(0.3 + 0.4 * Clamp(Affinity, 0.0, 1.0) + 0.4 * Quote.Cut, 0.1, 0.95);
		Quote.FlipRisk = Clamp(Traits.FlipBase * (1.6 - Quote.Loyalty), 0.002, 0.2);
		Quote.bAvailable = true;
		return Quote;
	}

	void ConspiracySystem::Recruit(const std::string& NpcId, const std::string& Name, InsiderRole Role, const RecruitQuote& Terms)
	{
		if (!Terms.bAvailable)
		{
			return;
		}
		Insider Member;
		Member.NpcId = NpcId;
		Member.Name = Name;
		Member.Role = Role;
		Member.Cut = Terms.Cut;
		Member.Retainer = Terms.Retainer;
		Member.Loyalty = Terms.Loyalty;
		Crew.push_back(Member);
		Outlay += Terms.Retainer;
		if (StageValue == SchemeStage::None)
		{
			StageValue = SchemeStage::Planning;
		}
	}

	bool ConspiracySystem::CanRelease(std::string* OutReason) const
	{
		if (StageValue == SchemeStage::Released || StageValue == SchemeStage::Unraveling)
		{
			if (OutReason)
			{
				*OutReason = "It's already out there.";
			}
			return false;
		}
		if (StageValue == SchemeStage::Exposed)
		{
			if (OutReason)
			{
				*OutReason = "Investigators are already on you.";
			}
			return false;
		}
		if (!HasRole(InsiderRole::LabTech))
		{
			if (OutReason)
			{
				*OutReason = "Nobody on your crew can get near a sample. You need someone inside a lab.";
			}
			return false;
		}
		return true;
	}

	void ConspiracySystem::BeginPlanning()
	{
		if (StageValue == SchemeStage::None)
		{
			StageValue = SchemeStage::Planning;
		}
	}

	void ConspiracySystem::Release()
	{
		StageValue = SchemeStage::Released;
		Days = 0;
		bShared = true;
	}

	void ConspiracySystem::SharePayout(Money PlayerProfit, bool bPaidTheCrew)
	{
		Proceeds += PlayerProfit;
		bShared = bPaidTheCrew;
		for (Insider& Member : Crew)
		{
			if (Member.bFlipped)
			{
				continue;
			}
			// Paid as promised, loyalty holds; stiffed, everyone starts thinking about immunity.
			Member.Loyalty = Clamp(Member.Loyalty + (bPaidTheCrew ? 0.15 : -0.35), 0.02, 0.98);
		}
	}

	MatrixGame ConspiracySystem::InsiderDilemma(const Insider& Who, double MedicineQuality, Money Treasury) const
	{
		// Classic prisoner's dilemma in dollars. Both quiet: everyone keeps their cut. One flips: they get immunity
		// and the other takes the fall. Both flip: the case is overwhelming and the money is forfeit either way.
		const double Take = ToDollars(ExpectedTake(MedicineQuality, Treasury));
		const double TheirCut = Take * Who.Cut;
		const double Ruin = -Take * 1.5;
		const double Immunity = TheirCut * 0.25;
		MatrixGame Game = MatrixGame::Make(2, 2,
			// player quiet / player flips first, against them quiet / them flipping
			{Take - TheirCut, Ruin, Take * 0.2, Ruin * 0.6},
			{TheirCut, Immunity, Ruin, Ruin * 0.6});
		Game.RowLabels = {"Keep quiet", "Go to investigators first"};
		Game.ColLabels = {"Keeps quiet", "Goes to investigators"};
		return Game;
	}

	double ConspiracySystem::ResponseDrag() const
	{
		return HasRole(InsiderRole::HealthOfficial) ? 0.35 : 0.0;
	}

	SchemeDay ConspiracySystem::DailyUpdate(bool bOutbreakRunning, double PublicAnger, Rng& Random)
	{
		SchemeDay Day;
		if (!IsLive())
		{
			Day.Heat = HeatValue;
			return Day;
		}
		Days += 1;
		// Investigators work faster with more people to interview, a louder outbreak, and an angry public
		// demanding answers. A fixer pushes back the other way.
		double Rate = 0.001;
		for (const Insider& Member : Crew)
		{
			if (!Member.bFlipped)
			{
				Rate += detail_conspiracy::TraitsFor(Member.Role).HeatPerDay;
			}
		}
		Rate += bOutbreakRunning ? 0.001 : 0.0;
		Rate += 0.006 * Clamp(PublicAnger, 0.0, 1.0);
		HeatValue = Clamp(HeatValue + Rate, 0.0, 1.0);
		if (!bOutbreakRunning)
		{
			HeatValue *= 0.985; // with nobody sick any more, the story slides off the front page
		}

		// Each insider decides afresh: flip now for immunity, or stay quiet and hope. Heat and being stiffed
		// both push toward flipping; so does the knowledge that whoever talks first gets the deal.
		for (Insider& Member : Crew)
		{
			if (Member.bFlipped)
			{
				continue;
			}
			const detail_conspiracy::RoleTraits Traits = detail_conspiracy::TraitsFor(Member.Role);
			const double Risk = Traits.FlipBase * (1.6 - Member.Loyalty) * (0.5 + 2.0 * HeatValue) * (bShared ? 1.0 : 2.5);
			if (Random.Chance(Clamp(Risk, 0.0, 0.5)))
			{
				Member.bFlipped = true;
				Day.bFlipped = true;
				Day.FlippedName = Member.Name;
				HeatValue = Clamp(HeatValue + 0.3, 0.0, 1.0);
				Day.Headline = Member.Name + " cut a deal with investigators";
				break; // the first one to talk is the story of the day
			}
		}
		if (HeatValue > 0.6 && StageValue == SchemeStage::Released)
		{
			StageValue = SchemeStage::Unraveling;
		}
		if (HeatValue >= 1.0)
		{
			Day.bExposed = true;
			Day.Headline = "Investigators say the outbreak was no accident";
		}
		else if (!bOutbreakRunning && HeatValue < 0.25 && Days > 30)
		{
			// The outbreak is over, nobody talked, and the file goes in a drawer.
			StageValue = SchemeStage::GotAway;
			Day.Headline = "Investigation into the outbreak's origin closes without charges";
		}
		Day.Heat = HeatValue;
		return Day;
	}

	std::vector<Crime> ConspiracySystem::Charges() const
	{
		std::vector<Crime> List{Crime::Conspiracy, Crime::Fraud};
		if (!Crew.empty())
		{
			List.push_back(Crime::Bribery);
		}
		return List;
	}

	void ConspiracySystem::MarkExposed()
	{
		StageValue = SchemeStage::Exposed;
		HeatValue = 1.0;
	}

	void ConspiracySystem::Close()
	{
		StageValue = SchemeStage::None;
		Crew.clear();
		HeatValue = 0.0;
		Days = 0;
		Outlay = 0;
		Proceeds = 0;
		bShared = true;
	}

	ConspiracySystem::Snapshot ConspiracySystem::Save() const
	{
		Snapshot Data;
		Data.Stage = StageValue;
		Data.Crew = Crew;
		Data.Heat = HeatValue;
		Data.Days = Days;
		Data.Outlay = Outlay;
		Data.Proceeds = Proceeds;
		Data.bShared = bShared;
		return Data;
	}

	void ConspiracySystem::Load(const Snapshot& Data)
	{
		StageValue = Data.Stage;
		Crew = Data.Crew;
		HeatValue = Data.Heat;
		Days = Data.Days;
		Outlay = Data.Outlay;
		Proceeds = Data.Proceeds;
		bShared = Data.bShared;
	}
}
