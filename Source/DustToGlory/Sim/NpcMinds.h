// NPC minds: background characters who can "wake up".
//
// Every pedestrian has a persistent mind: personality traits, memories of what they witnessed, skills
// and an awareness level. Unusual events (seeing powers, impossible stunts, the world glitching when
// time slows) and genuine interaction with the player (talking, helping, teaching) raise awareness. As it
// grows an NPC moves from scripted routine to curiosity, questioning, awakening and finally choosing its
// own path: learning skills, changing careers, joining the player, or even manifesting powers.
#pragma once

#include "Sim/Careers.h"
#include "Sim/GameTheory.h"
#include "Sim/Powers.h"
#include "Sim/SimCommon.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace dtg::sim
{
	enum class AwarenessStage : int
	{
		Scripted,
		Curious,
		Questioning,
		Awakened,
		SelfDirected,
		Count
	};

	const char* AwarenessStageName(AwarenessStage Stage);

	enum class Stimulus : int
	{
		SawPower,       // watched the player use a superpower
		SawGlitch,      // time slowed, teleport, phasing: the world "glitched"
		SawStunt,       // big jump, near miss, drift
		SawCrash,
		SawHack,        // a traffic light or billboard changed by itself
		SawRace,
		SawVillain,
		HeardSermon,
		TalkedWith,
		Helped,         // the player helped them (rescue, gift, favour)
		Taught,         // the player taught them a skill
		Encouraged,     // the player encouraged their dream
		Ignored,
		Count
	};

	const char* StimulusName(Stimulus Value);

	struct Personality
	{
		double Curiosity = 0.5;
		double Courage = 0.5;
		double Kindness = 0.5;
		double Ambition = 0.5;
		double Humor = 0.5;
	};

	struct Memory
	{
		std::int64_t Minute = 0;
		Stimulus Kind = Stimulus::TalkedWith;
		std::string Detail;
	};

	/** What an NPC decides to do with its next stretch of time. */
	enum class NpcActivity : int
	{
		Routine,        // the scripted loop: walk the block, commute, sip coffee
		Watch,          // stop and watch the player
		Wander,         // explore somewhere new
		Study,          // library / university
		Train,          // gym / track
		Work,           // go to a chosen career's workplace
		Worship,        // church
		FollowPlayer,   // companion
		Patrol,         // a self-made hero looking for trouble
		Count
	};

	const char* NpcActivityName(NpcActivity Value);

	struct NpcDecision
	{
		NpcActivity Activity = NpcActivity::Routine;
		std::string Destination; // building role, e.g. "Library", "Gym", "Hospital"
		std::string Thought;     // shown above their head
	};

	/** Something the player has raised with them, and how often. Asking twice is not the same as asking once. */
	struct TopicRecord
	{
		std::string Id;
		int Times = 0;
		std::int64_t LastMinute = 0;
	};

	/** Somebody else in town they know, which is how word travels. */
	struct Acquaintance
	{
		std::string Id;
		std::string Name;
		double Tie = 0.5;          // 0..1 how close they are
	};

	/** Something being said about the player, and who said it. */
	struct Rumor
	{
		std::string Text;
		std::string SourceId;      // who told them; empty means they saw it themselves
		std::string SourceName;
		double Weight = 0.5;       // 0..1 how much of it they believe
		int Hops = 0;              // how many mouths it passed through to reach them
		int Retold = 0;            // how many times they have passed it on; nobody repeats it forever
		std::int64_t Minute = 0;
	};

	/** What is wrong in their life. The reason a conversation is worth having twice. */
	enum class TroubleKind : int
	{
		None,
		Debt,
		Job,
		Family,
		Car,
		Fear,
		Faith,
		Loneliness,
		Count
	};

	const char* TroubleKindName(TroubleKind Value);

	struct Trouble
	{
		TroubleKind Kind = TroubleKind::None;
		std::string Text;
		Money Cost = 0;            // what it takes to put right, when money puts it right
		bool bShared = false;      // they have told the player about it
		bool bHelped = false;      // the player did something about it
	};

	/** What they know that the player might want to know. */
	enum class TipKind : int
	{
		None,
		CarForSale,
		Hiring,
		PoliceWatch,
		BusinessTrouble,
		Shortcut,
		Person,
		Count
	};

	const char* TipKindName(TipKind Value);

	struct Tip
	{
		TipKind Kind = TipKind::None;
		std::string Text;
		Money Ask = 0;             // what they want for it; 0 when they will just say
		double Value = 0.0;        // what it is worth to the player, in dollars
		bool bGiven = false;
	};

	struct NpcMind
	{
		std::string Id;
		std::string Name;
		std::string Occupation;    // scripted job, e.g. "Barista"
		std::string Hometown;      // where they are from; empty until somebody asks around
		Personality Traits;
		double Awareness = 0.0;    // 0..1
		AwarenessStage Stage = AwarenessStage::Scripted;
		double Affinity = 0.0;     // -1..1 toward the player
		std::vector<Memory> Memories;
		std::array<int, static_cast<std::size_t>(Skill::Count)> SkillXp{};
		std::array<int, static_cast<std::size_t>(Stimulus::Count)> Seen{};
		std::string Goal;          // set once awakened, e.g. "Become a Nurse"
		std::string GoalCareer;    // career id the goal points at
		std::string CareerId;      // career they actually hold now
		bool bCompanion = false;
		std::string PowerId;       // empty until they manifest a power
		Alignment Side = Alignment::Hero;
		int Conversations = 0;
		/** Favors and promises between this NPC and the player are a repeated prisoner's dilemma. */
		RepeatedGameMemory Deal;

		// -------------------------------------------------------------- Conversation
		/**
		 * The strangest thing they ever witnessed, kept outside the memory list. Memories are a short
		 * FIFO and talking to somebody fills it, so without this a long conversation would make a
		 * witness forget what they saw -- which is exactly backwards.
		 */
		bool bSawSomething = false;
		Memory Witnessed;
		std::vector<TopicRecord> Topics;
		std::vector<Acquaintance> Knows;
		std::vector<Rumor> Heard;
		Trouble Burden;
		Tip Knowledge;
		double Patience = 1.0;     // 0..1; falls when the player goes over the same ground

		int SkillLevel(Skill Value) const { return Profile::LevelForXp(SkillXp[static_cast<std::size_t>(Value)]); }
		/** How many times the player has raised a topic. */
		int TimesDiscussed(const std::string& TopicId) const;
		const Acquaintance* FindKnown(const std::string& OtherId) const;
	};

	struct AwarenessChange
	{
		double Before = 0.0;
		double After = 0.0;
		bool bStageChanged = false;
		AwarenessStage NewStage = AwarenessStage::Scripted;
		bool bLearned = false;
		Skill LearnedSkill = Skill::People;
		bool bManifestedPower = false;
		std::string Thought;
	};

	struct DialogueOption
	{
		std::string Id;     // "chat", "ask", "teach:Driving", "encourage", "invite", "dismiss"
		std::string Label;
		Money Cost = 0;     // what choosing it will cost the player, when it costs anything
	};

	/** The outcome of one line of conversation. */
	struct TalkTurn
	{
		std::string Reply;
		Money Cost = 0;           // money the player owes for what they just got
		bool bEnded = false;      // they walked off
		bool bNewRumor = false;   // the town now has something to say
		std::string Rumor;
	};

	class NpcPopulation
	{
	public:
		static constexpr std::size_t MaxMemories = 16;
		/** How long before a question counts as a fresh one again (a game day). */
		static constexpr std::int64_t TopicForgetMinutes = 1440;
		/** Nobody knows the whole town. Past this, meeting somebody new pushes out a weak tie. */
		static constexpr std::size_t MaxAcquaintances = 8;

		/** Returns the mind for an id, generating a deterministic new one the first time it is seen. */
		NpcMind& GetOrCreate(const std::string& Id);
		NpcMind* Find(const std::string& Id);
		const NpcMind* Find(const std::string& Id) const;
		const std::vector<NpcMind>& All() const { return Minds; }
		int CountAtLeast(AwarenessStage Stage) const;

		/** Records something the NPC experienced and updates awareness, skills and stage. */
		AwarenessChange Observe(NpcMind& Mind, Stimulus Kind, double Intensity, std::int64_t Minute, Rng& Random,
			const std::string& Detail = std::string(), const PowerRoster* Roster = nullptr);

		/** Practising a skill on their own (gym, library, workplace). */
		bool Practice(NpcMind& Mind, Skill Value, int Xp);

		/** What they do next. Scripted NPCs keep their routine; awakened ones make choices. */
		NpcDecision Decide(NpcMind& Mind, int Hour, Rng& Random, const CareerBook& Careers) const;

		/** A spoken line that fits their stage and personality. */
		std::string Line(const NpcMind& Mind, Rng& Random) const;
		/** A short inner thought for the bubble above their head. */
		std::string Thought(const NpcMind& Mind, Rng& Random) const;

		std::vector<DialogueOption> Options(const NpcMind& Mind) const;
		/** Applies a dialogue choice; returns their reply. */
		std::string Respond(NpcMind& Mind, const std::string& OptionId, std::int64_t Minute, Rng& Random, const CareerBook& Careers);
		/** The same, with everything the choice did: what it costs, whether they walked off, what the town now knows. */
		TalkTurn Say(NpcMind& Mind, const std::string& OptionId, std::int64_t Minute, Rng& Random, const CareerBook& Careers,
			Money PlayerCash = 0);

		// ------------------------------------------------------------------ A town that talks
		/** Links two minds, so word can travel between them. Both ends remember. */
		void Introduce(const std::string& AId, const std::string& BId, double Tie);
		/** Gives a mind a handful of acquaintances drawn from whoever else exists. */
		int Mingle(NpcMind& Mind, Rng& Random, int Want = 3);
		/** Puts a line about the player into one mind's ears. */
		bool Tell(NpcMind& Mind, const std::string& Text, const std::string& SourceId, double Weight, int Hops, std::int64_t Minute);
		/**
		 * Carries a line from one mind to the people it knows. Returns how many heard it. Hops is how
		 * far along the chain the teller's own copy already was, so the listeners' copies count from there.
		 */
		int Spread(const std::string& FromId, const std::string& Text, double Weight, std::int64_t Minute, Rng& Random,
			int Hops = 0);
		/** Once a game day: rumours fade, and the strongest ones travel one more hop. */
		int TownTalks(std::int64_t Minute, Rng& Random);
		/** The rumour they would repeat if asked, or null. */
		const Rumor* LoudestRumor(const NpcMind& Mind) const;
		/** How many minds are carrying a line that mentions this text. */
		int HowManyHeard(const std::string& Text) const;

		// ------------------------------------------------------------------ Troubles and tips
		/** Gives them a problem to carry, if they do not already have one. */
		void GiveTrouble(NpcMind& Mind, Rng& Random) const;
		/** What it would take from the player to put their trouble right. */
		Money HelpCost(const NpcMind& Mind) const;
		/** Something they know, generated from what they do and what they have seen. */
		void GiveTip(NpcMind& Mind, Rng& Random) const;
		/** What they will settle for, splitting the gain from telling against the risk of telling. */
		Bargain PriceOfTip(const NpcMind& Mind) const;

		/** Where they are from, which gives them something to say about the place. */
		void Place(NpcMind& Mind, const std::string& TownName) const;

		void Restore(std::vector<NpcMind> InMinds) { Minds = std::move(InMinds); }

	private:
		void PickGoal(NpcMind& Mind, Rng& Random, const CareerBook* Careers) const;
		/** Keeps an acquaintance list down to a plausible size by dropping the weakest tie. */
		void Thin(NpcMind& Mind) const;
		/** Records that a topic was raised and returns how many times it had been raised before. */
		int RaiseTopic(NpcMind& Mind, const std::string& TopicId, std::int64_t Minute) const;
		/** Their account of the strangest thing they have witnessed. */
		std::string Recount(const NpcMind& Mind) const;

		std::vector<NpcMind> Minds;
	};
}
