#include "Sim/Hacking.h"

namespace dtg::sim
{
	namespace detail_hacking
	{
		const std::vector<std::string>& Clients()
		{
			static const std::vector<std::string> Names = {
				"Brightwater Logistics", "Halcyon Health", "Meridian Savings", "Northgate Transit Authority",
				"Pinecrest University", "Sable Media Group", "Keystone Utilities", "Orchard Robotics",
				"Lakeside Mall Partners", "Summit Insurance", "Cardinal Freight", "Riverbend Water District"};
			return Names;
		}

		const char* TitleFor(HackTargetKind Kind, HackLegality Legality)
		{
			const bool bAuth = Legality == HackLegality::Authorized;
			switch (Kind)
			{
			case HackTargetKind::TrafficSignal: return bAuth ? "Signal controller audit" : "Green-light corridor";
			case HackTargetKind::SecurityCamera: return bAuth ? "Camera network review" : "Blind the cameras";
			case HackTargetKind::DoorLock: return bAuth ? "Smart-lock assessment" : "After-hours entry";
			case HackTargetKind::Billboard: return bAuth ? "Signage system test" : "Billboard takeover";
			case HackTargetKind::VehicleSystem: return bAuth ? "Fleet telematics test" : "Remote unlock";
			case HackTargetKind::PowerSubstation: return bAuth ? "Grid resilience drill" : "Rolling blackout";
			case HackTargetKind::CorporateServer: return bAuth ? "Server penetration test" : "Data heist";
			case HackTargetKind::Count: break;
			}
			return "Contract";
		}
	}

	const char* HackTargetKindName(HackTargetKind Kind)
	{
		switch (Kind)
		{
		case HackTargetKind::TrafficSignal: return "Traffic signal";
		case HackTargetKind::SecurityCamera: return "Security camera";
		case HackTargetKind::DoorLock: return "Door lock";
		case HackTargetKind::Billboard: return "Billboard";
		case HackTargetKind::VehicleSystem: return "Vehicle system";
		case HackTargetKind::PowerSubstation: return "Power substation";
		case HackTargetKind::CorporateServer: return "Corporate server";
		case HackTargetKind::Count: break;
		}
		return "Device";
	}

	HackConfig MakeHackConfig(int SecurityTier, int TechSkillLevel, const HackToolkit& Tools)
	{
		const int Tier = Clamp(SecurityTier, 1, 5);
		const int SkillLevel = Clamp(TechSkillLevel, 0, 10);
		HackConfig Config;
		Config.Layers = 1 + (Tier + 1) / 2;          // 1,2,2,3,3
		Config.CodeLength = Tier >= 4 ? 5 : 4;
		Config.GlyphCount = 5 + Tier / 2;             // 5,6,6,7,7
		const double SkillFactor = 1.0 - 0.04 * SkillLevel;
		const double Relay = 1.0 - 0.15 * Clamp(Tools.GhostRelay, 0, 3);
		Config.TracePerGuess = (8.0 + 2.0 * Tier) * SkillFactor * Relay;
		Config.TracePerProbe = (6.0 + 1.5 * Tier) * (1.0 - 0.25 * Clamp(Tools.ProbeKit, 0, 3)) * Relay;
		Config.TracePerSecond = (0.4 + 0.3 * Tier) * SkillFactor * Relay;
		Config.PreRevealed = Clamp(Tools.PatternEngine, 0, 3);
		return Config;
	}

	HackSession::HackSession(const HackConfig& InConfig, std::uint64_t Seed) : Config(InConfig), Random(Seed)
	{
		Config.Layers = std::max(1, Config.Layers);
		Config.CodeLength = Clamp(Config.CodeLength, 2, 8);
		Config.GlyphCount = Clamp(Config.GlyphCount, 2, 10);
		for (int LayerNumber = 0; LayerNumber < Config.Layers; ++LayerNumber)
		{
			std::vector<int> Code;
			for (int Position = 0; Position < Config.CodeLength; ++Position)
			{
				Code.push_back(Random.RangeInt(0, Config.GlyphCount - 1));
			}
			Codes.push_back(std::move(Code));
		}
		EnterLayer(0);
	}

	void HackSession::EnterLayer(int NewLayer)
	{
		LayerIndex = NewLayer;
		Revealed.assign(static_cast<std::size_t>(Config.CodeLength), false);
		LayerHistory.clear();
		const int PreReveal = std::min(Config.PreRevealed, Config.CodeLength - 1);
		int Done = 0;
		int Attempts = 0;
		while (Done < PreReveal && Attempts < 32)
		{
			++Attempts;
			const std::size_t Position = Random.Index(Revealed.size());
			if (!Revealed[Position])
			{
				Revealed[Position] = true;
				++Done;
			}
		}
	}

	bool HackSession::IsRevealed(int Position) const
	{
		return Position >= 0 && Position < Config.CodeLength && Revealed[static_cast<std::size_t>(Position)];
	}

	int HackSession::RevealedGlyph(int Position) const
	{
		if (!IsRevealed(Position))
		{
			return -1;
		}
		return Codes[static_cast<std::size_t>(LayerIndex)][static_cast<std::size_t>(Position)];
	}

	void HackSession::AddTrace(double Amount)
	{
		if (CurrentState != HackState::Active)
		{
			return;
		}
		TraceValue = std::min(100.0, TraceValue + std::max(0.0, Amount));
		if (TraceValue >= 100.0)
		{
			CurrentState = HackState::Traced;
		}
	}

	bool HackSession::Probe(int Position)
	{
		if (CurrentState != HackState::Active || Position < 0 || Position >= Config.CodeLength || IsRevealed(Position))
		{
			return false;
		}
		Revealed[static_cast<std::size_t>(Position)] = true;
		AddTrace(Config.TracePerProbe);
		return true;
	}

	GuessFeedback HackSession::Score(const std::vector<int>& Secret, const std::vector<int>& Attempt, int NumGlyphs)
	{
		GuessFeedback Feedback;
		std::vector<int> SecretCounts(static_cast<std::size_t>(std::max(NumGlyphs, 1)), 0);
		std::vector<int> GuessCounts(static_cast<std::size_t>(std::max(NumGlyphs, 1)), 0);
		const std::size_t Length = std::min(Secret.size(), Attempt.size());
		for (std::size_t Index = 0; Index < Length; ++Index)
		{
			if (Secret[Index] == Attempt[Index])
			{
				++Feedback.Exact;
			}
			else
			{
				if (Secret[Index] >= 0 && Secret[Index] < NumGlyphs)
				{
					++SecretCounts[static_cast<std::size_t>(Secret[Index])];
				}
				if (Attempt[Index] >= 0 && Attempt[Index] < NumGlyphs)
				{
					++GuessCounts[static_cast<std::size_t>(Attempt[Index])];
				}
			}
		}
		for (std::size_t Glyph = 0; Glyph < SecretCounts.size(); ++Glyph)
		{
			Feedback.Partial += std::min(SecretCounts[Glyph], GuessCounts[Glyph]);
		}
		return Feedback;
	}

	GuessFeedback HackSession::Guess(const std::vector<int>& Code)
	{
		GuessFeedback Feedback;
		if (CurrentState != HackState::Active || static_cast<int>(Code.size()) != Config.CodeLength)
		{
			return Feedback;
		}
		Feedback = Score(Codes[static_cast<std::size_t>(LayerIndex)], Code, Config.GlyphCount);
		LayerHistory.push_back(GuessRecord{Code, Feedback});
		if (Feedback.Exact == Config.CodeLength)
		{
			if (LayerIndex + 1 >= Config.Layers)
			{
				CurrentState = HackState::Breached;
			}
			else
			{
				EnterLayer(LayerIndex + 1);
			}
		}
		else
		{
			AddTrace(Config.TracePerGuess);
		}
		return Feedback;
	}

	void HackSession::Tick(double Seconds)
	{
		if (Seconds > 0.0)
		{
			AddTrace(Config.TracePerSecond * Seconds);
		}
	}

	void HackSession::Abort()
	{
		if (CurrentState == HackState::Active)
		{
			CurrentState = HackState::Aborted;
		}
	}

	std::vector<HackContract> GenerateHackContracts(int Count, int TechSkillLevel, Rng& Random)
	{
		std::vector<HackContract> Result;
		const int SkillLevel = Clamp(TechSkillLevel, 0, 10);
		for (int Index = 0; Index < Count; ++Index)
		{
			HackContract Contract;
			Contract.Target = static_cast<HackTargetKind>(Random.RangeInt(0, static_cast<int>(HackTargetKind::Count) - 1));
			// Roughly one in four jobs is grey-market.
			Contract.Legality = Random.Chance(0.25) ? HackLegality::Unauthorized : HackLegality::Authorized;
			const int MaxTier = Clamp(1 + SkillLevel / 2, 1, 5);
			Contract.Tier = Random.RangeInt(1, MaxTier);
			Contract.Client = Random.Pick(detail_hacking::Clients());
			Contract.Title = detail_hacking::TitleFor(Contract.Target, Contract.Legality);
			const double Base = 800.0 * std::pow(static_cast<double>(Contract.Tier), 1.6);
			if (Contract.Legality == HackLegality::Authorized)
			{
				Contract.Payout = ToMoney(std::round(Base * Random.Range(0.9, 1.2) / 50.0) * 50.0);
				Contract.Brief = Contract.Client + " hired you to test their " + HackTargetKindName(Contract.Target) +
					" defenses and report what you find.";
			}
			else
			{
				Contract.Payout = ToMoney(std::round(Base * 2.2 * Random.Range(0.9, 1.3) / 50.0) * 50.0);
				Contract.HeatStars = Clamp(1 + Contract.Tier / 2, 1, 3);
				Contract.Brief = "An anonymous buyer wants into a " + std::string(HackTargetKindName(Contract.Target)) +
					" run by " + Contract.Client + ". Police will come looking.";
			}
			Contract.Id = Slugify(Contract.Client + " " + Contract.Title) + "-" + std::to_string(Random.NextU32() % 10000u);
			Result.push_back(Contract);
		}
		return Result;
	}

	Money HackToolPrice(int CurrentLevel)
	{
		switch (CurrentLevel)
		{
		case 0: return Dollars(1500);
		case 1: return Dollars(6000);
		case 2: return Dollars(18000);
		default: return 0;
		}
	}
}
