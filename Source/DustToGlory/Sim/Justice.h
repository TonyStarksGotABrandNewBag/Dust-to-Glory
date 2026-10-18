// Courts and sentences. Minor arrests end with a fine; serious ones go to court, where the player takes a plea
// (the Nash bargain between both sides' expected trial outcomes) or goes to trial. A case with a co-defendant is
// a prisoner's dilemma. Time is served in county jail, where good behavior, classes, parole hearings and appeals
// shorten it -- or where the player plans a way out: a tunnel, a bribed guard, a crew on the outside, a medical
// transfer or their own powers. Breaking out makes them a fugitive with a manhunt behind them. A record follows
// the player: some careers and public offices need a clean one.
#pragma once

#include "Sim/Careers.h"
#include "Sim/GameTheory.h"
#include "Sim/SimCommon.h"
#include "Sim/Wanted.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	enum class Severity : int
	{
		Infraction,
		Misdemeanor,
		Felony
	};
	Severity CrimeSeverity(Crime Value);
	/** Sentence in days for one count after a conviction at trial (0 for fine-only offenses). */
	int SentenceDaysFor(Crime Value);

	struct RecordEntry
	{
		Crime Charge = Crime::Speeding;
		std::int64_t Minute = 0;
		int SentenceDays = 0;
		bool bExpunged = false;
	};

	struct CourtCase
	{
		bool bOpen = false;
		std::vector<Crime> Charges;
		double Evidence = 0.6;          // 0..1 strength of the prosecution's case
		std::string CoDefendantId;      // NPC charged alongside the player, if any
		std::string CoDefendantName;
		int TrialDays = 0;              // sentence if convicted at trial
		bool bLawyer = false;
		Money Fine = 0;                 // fine-only matters
	};

	struct Verdict
	{
		bool bDecided = false;
		bool bConvicted = false;
		int SentenceDays = 0;
		std::string Summary;
	};

	enum class JailActivity : int
	{
		KeepHeadDown, // good behavior
		Work,         // a jail job: a little money and good behavior
		Study,        // classes (GED, trades) cut time and train skills
		Exercise,
		Count
	};
	const char* JailActivityName(JailActivity Activity);

	struct JailState
	{
		bool bInJail = false;
		std::int64_t ReleaseMinute = 0;
		int SentenceDays = 0;
		int DaysServed = 0;
		double GoodBehavior = 0.5;     // 0..1
		int ProgramDays = 0;           // class days toward the current program (10 completes one)
		int ProgramsCompleted = 0;
		std::int64_t NextParoleMinute = 0;
	};

	struct JailDayResult
	{
		std::string Summary;
		bool bReleased = false;
		int DaysCut = 0;
		Money Earned = 0;
	};

	/** How the player gets out early the hard way. */
	enum class EscapePlan : int
	{
		None,
		Tunnel,        // dig from the cell over several nights, between shakedowns
		BribeGuard,    // buy a night shift's blind eye
		OutsideCrew,   // pay a crew to stage a breakout at the fence
		MedicalRun,    // fake an illness and slip away during the hospital transfer
		PowerBreak,    // go through the wall with your own powers
		Count
	};
	const char* EscapePlanName(EscapePlan Plan);
	const char* EscapePlanDescription(EscapePlan Plan);

	/** What a plan would take and what it's worth trying, before the player commits to it. */
	struct EscapeOdds
	{
		bool bAvailable = false;
		std::string Reason;      // why not, when unavailable
		Money Cost = 0;          // bribe or crew fee, paid when the plan starts
		int PrepDays = 0;        // nights of preparation before an attempt is worth making
		double Success = 0.0;    // chance an attempt from full preparation works
		double SearchChance = 0.0; // chance per prep day the guards look (inspection-game equilibrium)
		double GuardCut = 0.0;   // bribe only: the guard's share of the Nash surplus
	};

	struct EscapeState
	{
		EscapePlan Plan = EscapePlan::None;
		int PrepDone = 0;
		double Suspicion = 0.0;   // 0..1: how closely the guards are watching
		bool bPaid = false;
		int FailedAttempts = 0;
		bool bFugitive = false;
		int DaysAtLarge = 0;
		int OwedDays = 0;         // sentence left to serve when recaptured
		double Manhunt = 0.0;     // 0..1 how hard police are still looking
	};

	struct EscapeResult
	{
		bool bAttempted = false;
		bool bEscaped = false;
		bool bCaught = false;
		int SolitaryDays = 0;     // days added to the sentence after a failure
		std::string Summary;
	};

	class JusticeSystem
	{
	public:
		const std::vector<RecordEntry>& Record() const { return Entries; }
		int Felonies() const;
		bool HasFelony() const { return Felonies() > 0; }
		int PriorConvictions() const;
		const CourtCase& Case() const { return OpenCase; }
		const JailState& Jail() const { return JailValue; }

		/** Opens a case from the charges. Returns false for fine-only matters (Case().Fine is set). */
		bool Arrest(const std::vector<Crime>& Charges, double Evidence, const std::string& CoDefendantId, const std::string& CoDefendantName,
			Money Balance);
		void HireLawyer() { OpenCase.bLawyer = true; }
		/** Settles a fine-only matter at the window: it is paid, so there is nothing left to answer. */
		void PayFine() { OpenCase = CourtCase(); }
		/** Takes what was collected off what is owed; a fine paid in full leaves nothing behind. */
		void ReduceFine(Money Paid)
		{
			OpenCase.Fine = OpenCase.Fine > Paid ? OpenCase.Fine - Paid : 0;
			if (OpenCase.Fine <= 0)
			{
				OpenCase = CourtCase();
			}
		}
		static Money LawyerFee(int TrialDays);
		double ConvictionChance(int PeopleSkill) const;
		/** The prosecutor's offer: the Nash bargain between the expected trial sentence, its cost and its risk. */
		int PleaOffer(int PeopleSkill) const;
		Verdict TakePlea(int PeopleSkill, std::int64_t Now);
		Verdict GoToTrial(int PeopleSkill, Rng& Random, std::int64_t Now);
		/** Rows: player stays silent / testifies; columns: co-defendant does the same. Payoffs are minus days. */
		static MatrixGame CoDefendantGame(int TrialDays);
		Verdict CoDefendantVerdict(bool bPlayerTestifies, bool bOtherTestifies, std::int64_t Now);

		void BeginSentence(int Days, std::int64_t Now);

		// ------------------------------------------------------------ Breaking out
		const EscapeState& Escape() const { return EscapeValue; }
		bool IsFugitive() const { return EscapeValue.bFugitive; }
		/** What a plan would cost and its odds, given the player's skills, powers and cash. */
		EscapeOdds QuoteEscape(EscapePlan Plan, const Profile& Player, double PowerPotency, Money Balance) const;
		/** Commits to a plan (the caller charges Cost first). Replaces any plan in progress. */
		bool StartEscape(EscapePlan Plan, bool bPaid);
		void AbandonEscape();
		/** A night of preparation. The guards inspect at the game's equilibrium rate; being found means solitary. */
		EscapeResult PrepareEscape(const Profile& Player, double PowerPotency, Money Balance, Rng& Random);
		/** Goes tonight. Success makes the player a fugitive; failure means solitary and a tighter watch. */
		EscapeResult AttemptEscape(const Profile& Player, double PowerPotency, Money Balance, Rng& Random);
		/** A day on the run: the manhunt cools when lying low, heats up when the player is seen. Returns true if police close in. */
		bool AdvanceManhunt(bool bLayingLow, Rng& Random);
		/** Picked up again: the escape goes on the record and the remaining sentence resumes. */
		void Recapture(std::int64_t Now);
		/** Walks into the courthouse voluntarily: the remaining time, plus a smaller escape sentence. */
		void Surrender(std::int64_t Now);
		JailDayResult ServeDay(JailActivity Activity, Profile& Player, std::int64_t Now);
		double ParoleChance() const;
		bool CanRequestParole(std::int64_t Now) const;
		bool ParoleHearing(std::int64_t Now, Rng& Random);
		static Money AppealCost(int SentenceDays);
		/** Overturned convictions are released and removed from the record. */
		bool Appeal(Rng& Random);
		/** Clears felonies older than three years from the record (through a lawyer). */
		bool Expunge(std::int64_t Now);
		void Pardon();
		void Release();

		struct Snapshot
		{
			std::vector<RecordEntry> Entries;
			CourtCase OpenCase;
			JailState Jail;
			EscapeState Escape;
		};
		Snapshot Save() const { return Snapshot{Entries, OpenCase, JailValue, EscapeValue}; }
		void Load(const Snapshot& Data)
		{
			Entries = Data.Entries;
			OpenCase = Data.OpenCase;
			JailValue = Data.Jail;
			EscapeValue = Data.Escape;
		}

	private:
		Verdict Convict(int Days, const std::string& Summary, std::int64_t Now);
		void Solitary(int Days);
		void GoOnTheRun();

		std::vector<RecordEntry> Entries;
		CourtCase OpenCase;
		JailState JailValue;
		EscapeState EscapeValue;
	};
}
