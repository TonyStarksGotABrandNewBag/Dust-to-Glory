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

	struct NpcMind
	{
		std::string Id;
		std::string Name;
		std::string Occupation;    // scripted job, e.g. "Barista"
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

		int SkillLevel(Skill Value) const { return Profile::LevelForXp(SkillXp[static_cast<std::size_t>(Value)]); }
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
	};

	class NpcPopulation
	{
	public:
		static constexpr std::size_t MaxMemories = 16;

		/** Returns the mind for an id, generating a deterministic new one the first time it is seen. */
		NpcMind& GetOrCreate(const std::string& Id);
		NpcMind* Find(const std::string& Id);
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

		void Restore(std::vector<NpcMind> InMinds) { Minds = std::move(InMinds); }

	private:
		void PickGoal(NpcMind& Mind, Rng& Random, const CareerBook* Careers) const;

		std::vector<NpcMind> Minds;
	};
}
