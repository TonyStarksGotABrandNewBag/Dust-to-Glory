// Courts and sentences. Minor arrests end with a fine; serious ones go to court, where the player takes a plea
// (the Nash bargain between both sides' expected trial outcomes) or goes to trial. A case with a co-defendant is
// a prisoner's dilemma. Time is served in county jail, where good behavior, classes, parole hearings and appeals
// shorten it. A record follows the player: some careers and public offices need a clean one.
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
		};
		Snapshot Save() const { return Snapshot{Entries, OpenCase, JailValue}; }
		void Load(const Snapshot& Data)
		{
			Entries = Data.Entries;
			OpenCase = Data.OpenCase;
			JailValue = Data.Jail;
		}

	private:
		Verdict Convict(int Days, const std::string& Summary, std::int64_t Now);

		std::vector<RecordEntry> Entries;
		CourtCase OpenCase;
		JailState JailValue;
	};
}
