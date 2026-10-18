// In-game hacking. Everything here is a fictional puzzle: a target is a chain of security layers,
// each guarded by a hidden code of glyphs. Probing reveals a glyph, guessing returns match feedback,
// and every action raises a trace meter. It feels like recon -> access -> objective without modelling
// any real-world technique.
#pragma once

#include "Sim/SimCommon.h"

#include <cstddef>
#include <string>
#include <vector>

namespace dtg::sim
{
	enum class HackTargetKind : int
	{
		TrafficSignal,
		SecurityCamera,
		DoorLock,
		Billboard,
		VehicleSystem,
		PowerSubstation,
		CorporateServer,
		Count
	};

	const char* HackTargetKindName(HackTargetKind Kind);

	/** Authorized jobs are paid security tests; unauthorized ones pay more but bring police heat. */
	enum class HackLegality : int
	{
		Authorized,
		Unauthorized
	};

	/** Upgradeable in-game tools bought at the electronics store. Levels 0..3. */
	struct HackToolkit
	{
		int ProbeKit = 0;      // cheaper probes
		int GhostRelay = 0;    // slower trace
		int PatternEngine = 0; // pre-reveals glyphs on each layer
	};

	struct HackConfig
	{
		int Layers = 2;
		int CodeLength = 4;
		int GlyphCount = 6;
		double TracePerGuess = 12.0;
		double TracePerProbe = 8.0;
		double TracePerSecond = 1.0;
		int PreRevealed = 0;
	};

	/** Difficulty scales with the target's tier (1..5); skill and tools push it back down. */
	HackConfig MakeHackConfig(int SecurityTier, int TechSkillLevel, const HackToolkit& Tools);

	struct GuessFeedback
	{
		int Exact = 0;   // right glyph, right position
		int Partial = 0; // right glyph, wrong position
	};

	struct GuessRecord
	{
		std::vector<int> Code;
		GuessFeedback Feedback;
	};

	enum class HackState : int
	{
		Active,
		Breached,
		Traced,
		Aborted
	};

	class HackSession
	{
	public:
		HackSession(const HackConfig& InConfig, std::uint64_t Seed);

		HackState State() const { return CurrentState; }
		int Layer() const { return LayerIndex; }
		int Layers() const { return Config.Layers; }
		int CodeLength() const { return Config.CodeLength; }
		int GlyphCount() const { return Config.GlyphCount; }
		double Trace() const { return TraceValue; }
		const HackConfig& Settings() const { return Config; }

		bool IsRevealed(int Position) const;
		/** Glyph at a revealed position, -1 when hidden. */
		int RevealedGlyph(int Position) const;

		/** Reveals one glyph of the current layer. Returns false if not active or already revealed. */
		bool Probe(int Position);
		/** Submits a full code for the current layer. Wrong guesses add trace. */
		GuessFeedback Guess(const std::vector<int>& Code);
		/** Passive trace over time. */
		void Tick(double Seconds);
		void Abort();

		const std::vector<GuessRecord>& History() const { return LayerHistory; }

		/** Exposed for tests. */
		const std::vector<int>& SecretForLayer(int LayerNumber) const { return Codes[static_cast<std::size_t>(LayerNumber)]; }
		static GuessFeedback Score(const std::vector<int>& Secret, const std::vector<int>& Attempt, int NumGlyphs);

	private:
		void AddTrace(double Amount);
		void EnterLayer(int NewLayer);

		HackConfig Config;
		Rng Random;
		std::vector<std::vector<int>> Codes;
		std::vector<bool> Revealed;
		std::vector<GuessRecord> LayerHistory;
		HackState CurrentState = HackState::Active;
		int LayerIndex = 0;
		double TraceValue = 0.0;
	};

	struct HackContract
	{
		std::string Id;
		std::string Title;
		std::string Client;
		std::string Brief;
		HackTargetKind Target = HackTargetKind::CorporateServer;
		HackLegality Legality = HackLegality::Authorized;
		int Tier = 1;
		Money Payout = 0;
		int HeatStars = 0; // wanted stars added on success (unauthorized only)
	};

	/** Fills the contract board: mostly paid security tests plus a few grey-market jobs. */
	std::vector<HackContract> GenerateHackContracts(int Count, int TechSkillLevel, Rng& Random);

	/** Price of the next tool level, or 0 if maxed. */
	Money HackToolPrice(int CurrentLevel);
}
