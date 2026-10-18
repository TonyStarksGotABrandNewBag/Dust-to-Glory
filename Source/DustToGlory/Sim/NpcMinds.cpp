#include "Sim/NpcMinds.h"

namespace dtg::sim
{
	namespace detail_npc
	{
		struct StimulusInfo
		{
			Stimulus Kind;
			const char* Name;
			double Weight;       // awareness gained per full-intensity event
			double Affinity;     // relationship change
			bool bInteractive;   // relationships habituate slowly; spectacle habituates fast
			int LearnSkill;      // index into Skill, -1 for none
		};

		const StimulusInfo Table[] = {
			{Stimulus::SawPower, "SawPower", 0.08, 0.00, false, static_cast<int>(Skill::Athletics)},
			{Stimulus::SawGlitch, "SawGlitch", 0.12, 0.00, false, static_cast<int>(Skill::Science)},
			{Stimulus::SawStunt, "SawStunt", 0.03, 0.00, false, static_cast<int>(Skill::Driving)},
			{Stimulus::SawCrash, "SawCrash", 0.02, -0.01, false, -1},
			{Stimulus::SawHack, "SawHack", 0.05, 0.00, false, static_cast<int>(Skill::Tech)},
			{Stimulus::SawRace, "SawRace", 0.02, 0.00, false, static_cast<int>(Skill::Driving)},
			{Stimulus::SawVillain, "SawVillain", 0.06, 0.00, false, static_cast<int>(Skill::Fitness)},
			{Stimulus::HeardSermon, "HeardSermon", 0.04, 0.03, true, static_cast<int>(Skill::Faith)},
			{Stimulus::TalkedWith, "TalkedWith", 0.04, 0.03, true, static_cast<int>(Skill::People)},
			{Stimulus::Helped, "Helped", 0.06, 0.08, true, -1},
			{Stimulus::Taught, "Taught", 0.07, 0.06, true, -1},
			{Stimulus::Encouraged, "Encouraged", 0.08, 0.05, true, -1},
			{Stimulus::Ignored, "Ignored", -0.01, -0.02, true, -1},
		};

		const StimulusInfo& Info(Stimulus Kind)
		{
			for (const StimulusInfo& Entry : Table)
			{
				if (Entry.Kind == Kind)
				{
					return Entry;
				}
			}
			return Table[0];
		}

		constexpr double StageThresholds[] = {0.0, 0.15, 0.35, 0.60, 0.85};

		AwarenessStage StageFor(double Awareness)
		{
			AwarenessStage Stage = AwarenessStage::Scripted;
			for (int Index = 0; Index < static_cast<int>(AwarenessStage::Count); ++Index)
			{
				if (Awareness >= StageThresholds[Index])
				{
					Stage = static_cast<AwarenessStage>(Index);
				}
			}
			return Stage;
		}

		const std::vector<std::string>& FirstNames()
		{
			static const std::vector<std::string> Names = {
				"Avery", "Malik", "Sofia", "Hiro", "Grace", "Mateo", "Priya", "Elijah", "Nia", "Owen",
				"Lucia", "Kwame", "Hannah", "Diego", "Mei", "Isaiah", "Fatima", "Caleb", "Zoe", "Andre",
				"Ruth", "Tomas", "Leah", "Samuel", "Imani", "Noah", "Esther", "Rafael", "Joy", "Gabriel",
				"Naomi", "Ezra", "Talia", "Marcus", "Lydia", "Jonah", "Amara", "Felix", "Vera", "Desmond"};
			return Names;
		}

		const std::vector<std::string>& LastNames()
		{
			static const std::vector<std::string> Names = {
				"Reyes", "Okafor", "Nguyen", "Brooks", "Kowalski", "Haddad", "Park", "Mensah", "Larsen", "Castillo",
				"Bennett", "Sato", "Ibrahim", "Fischer", "Coleman", "Moreau", "Patel", "Odum", "Lindqvist", "Ramos",
				"Whitaker", "Chen", "Abernathy", "Silva", "Kim", "Delgado", "Foster", "Novak", "Osei", "Grant"};
			return Names;
		}

		const std::vector<std::string>& Occupations()
		{
			static const std::vector<std::string> Jobs = {
				"Barista", "Mail Carrier", "Jogger", "Street Sweeper", "Office Worker", "Student", "Food Truck Cook",
				"Dog Walker", "Security Guard", "Bank Teller", "Crossing Guard", "Delivery Rider", "Tourist",
				"Retiree", "Construction Worker", "Window Washer", "Newsstand Clerk", "Bus Driver"};
			return Jobs;
		}

		const std::vector<std::string>& Lines(AwarenessStage Stage)
		{
			static const std::vector<std::string> Scripted = {
				"Nice day, isn't it?", "Traffic's been wild lately.", "Have a good one!", "Can't stop, gotta keep my schedule.",
				"Another beautiful morning in the city.", "Morning! Same as always.", "Busy, busy, busy."};
			static const std::vector<std::string> Curious = {
				"Didn't I see you do something... impossible earlier?", "Do you ever feel like you've lived today before?",
				"You're not like the others, are you?", "I keep noticing you. That's new for me.",
				"Funny thing: I just realized I don't remember buying these shoes."};
			static const std::vector<std::string> Questioning = {
				"Why does the sun cross the sky in under an hour?", "I've walked this same block my whole life. Why?",
				"What's past the edge of the city? Has anyone checked?", "If I stopped doing my routine... would anything happen?",
				"Sometimes the street repeats. Same car, same honk. Do you see it too?"};
			static const std::vector<std::string> Awakened = {
				"I want more than my route. Will you show me something new?", "I signed up for a class. Me! A class!",
				"I think I get to choose what I do. Is that strange?", "I've been practicing. I'm actually getting better.",
				"I said no to my routine today. The world didn't end."};
			static const std::vector<std::string> SelfDirected = {
				"I'm going after it: {goal}. Watch me.", "Thanks for waking me up. I'm paying it forward.",
				"I found out I can do things I never could before.", "I've got plans now. Real ones.",
				"I used to be {occupation}. Now I'm whoever I decide to be."};
			switch (Stage)
			{
			case AwarenessStage::Scripted: return Scripted;
			case AwarenessStage::Curious: return Curious;
			case AwarenessStage::Questioning: return Questioning;
			case AwarenessStage::Awakened: return Awakened;
			case AwarenessStage::SelfDirected:
			case AwarenessStage::Count: break;
			}
			return SelfDirected;
		}

		const std::vector<std::string>& Thoughts(AwarenessStage Stage)
		{
			static const std::vector<std::string> Scripted = {"Coffee. Then work.", "Same as yesterday.", "Right on schedule."};
			static const std::vector<std::string> Curious = {"Who is that?", "Did that just happen?", "Huh."};
			static const std::vector<std::string> Questioning = {"Why do I always turn left here?", "Is any of this real?", "What if I didn't?"};
			static const std::vector<std::string> Awakened = {"I can choose.", "Let's try something new.", "I want to learn."};
			static const std::vector<std::string> SelfDirected = {"{goal}.", "Today I decide.", "I've got this."};
			switch (Stage)
			{
			case AwarenessStage::Scripted: return Scripted;
			case AwarenessStage::Curious: return Curious;
			case AwarenessStage::Questioning: return Questioning;
			case AwarenessStage::Awakened: return Awakened;
			case AwarenessStage::SelfDirected:
			case AwarenessStage::Count: break;
			}
			return SelfDirected;
		}

		std::string Fill(const std::string& Template, const NpcMind& Mind)
		{
			std::string Out;
			for (std::size_t Index = 0; Index < Template.size(); ++Index)
			{
				if (Template[Index] == '{')
				{
					const std::size_t End = Template.find('}', Index);
					if (End != std::string::npos)
					{
						const std::string Key = Template.substr(Index + 1, End - Index - 1);
						if (Key == "goal") Out += Mind.Goal.empty() ? std::string("something bigger") : Mind.Goal;
						else if (Key == "name") Out += Mind.Name;
						else if (Key == "occupation") Out += "a " + ToLowerAscii(Mind.Occupation);
						Index = End;
						continue;
					}
				}
				Out.push_back(Template[Index]);
			}
			return Out;
		}
	}

	const char* AwarenessStageName(AwarenessStage Stage)
	{
		switch (Stage)
		{
		case AwarenessStage::Scripted: return "Scripted";
		case AwarenessStage::Curious: return "Curious";
		case AwarenessStage::Questioning: return "Questioning";
		case AwarenessStage::Awakened: return "Awakened";
		case AwarenessStage::SelfDirected: return "Self-directed";
		case AwarenessStage::Count: break;
		}
		return "Scripted";
	}

	const char* StimulusName(Stimulus Value)
	{
		return detail_npc::Info(Value).Name;
	}

	const char* NpcActivityName(NpcActivity Value)
	{
		switch (Value)
		{
		case NpcActivity::Routine: return "Routine";
		case NpcActivity::Watch: return "Watch";
		case NpcActivity::Wander: return "Wander";
		case NpcActivity::Study: return "Study";
		case NpcActivity::Train: return "Train";
		case NpcActivity::Work: return "Work";
		case NpcActivity::Worship: return "Worship";
		case NpcActivity::FollowPlayer: return "FollowPlayer";
		case NpcActivity::Patrol: return "Patrol";
		case NpcActivity::Count: break;
		}
		return "Routine";
	}

	NpcMind& NpcPopulation::GetOrCreate(const std::string& Id)
	{
		if (NpcMind* Existing = Find(Id))
		{
			return *Existing;
		}
		Rng Random(HashString("npc:" + Id));
		NpcMind Mind;
		Mind.Id = Id;
		Mind.Name = Random.Pick(detail_npc::FirstNames()) + " " + Random.Pick(detail_npc::LastNames());
		Mind.Occupation = Random.Pick(detail_npc::Occupations());
		Mind.Traits.Curiosity = Random.Range(0.1, 1.0);
		Mind.Traits.Courage = Random.Range(0.1, 1.0);
		Mind.Traits.Kindness = Random.Range(0.2, 1.0);
		Mind.Traits.Ambition = Random.Range(0.1, 1.0);
		Mind.Traits.Humor = Random.Range(0.1, 1.0);
		// Personality picks their strategy in the repeated game of favors.
		if (Mind.Traits.Kindness > 0.75) Mind.Deal.Strategy = RepeatedStrategy::GenerousTitForTat;
		else if (Mind.Traits.Kindness < 0.35) Mind.Deal.Strategy = RepeatedStrategy::GrimTrigger;
		else Mind.Deal.Strategy = RepeatedStrategy::TitForTat;
		Minds.push_back(std::move(Mind));
		return Minds.back();
	}

	NpcMind* NpcPopulation::Find(const std::string& Id)
	{
		for (NpcMind& Mind : Minds)
		{
			if (Mind.Id == Id)
			{
				return &Mind;
			}
		}
		return nullptr;
	}

	const NpcMind* NpcPopulation::Find(const std::string& Id) const
	{
		for (const NpcMind& Mind : Minds)
		{
			if (Mind.Id == Id)
			{
				return &Mind;
			}
		}
		return nullptr;
	}

	int NpcPopulation::CountAtLeast(AwarenessStage Stage) const
	{
		int Count = 0;
		for (const NpcMind& Mind : Minds)
		{
			Count += static_cast<int>(Mind.Stage) >= static_cast<int>(Stage) ? 1 : 0;
		}
		return Count;
	}

	AwarenessChange NpcPopulation::Observe(NpcMind& Mind, Stimulus Kind, double Intensity, std::int64_t Minute, Rng& Random,
		const std::string& Detail, const PowerRoster* Roster)
	{
		using namespace detail_npc;
		AwarenessChange Change;
		const StimulusInfo& Entry = detail_npc::Info(Kind);
		const std::size_t KindIndex = static_cast<std::size_t>(Kind);
		const int Count = Mind.Seen[KindIndex];
		Mind.Seen[KindIndex] = Count + 1;

		// Habituation: the hundredth superpower is less mind-blowing than the first, but friendship keeps counting.
		const double Novelty = Entry.bInteractive ? 1.0 / (1.0 + 0.1 * Count) : 1.0 / (1.0 + 0.5 * Count);
		const double Openness = 0.4 + 0.6 * Mind.Traits.Curiosity;
		const double Delta = Entry.Weight * Clamp(Intensity, 0.0, 2.0) * Novelty * Openness;

		Change.Before = Mind.Awareness;
		Mind.Awareness = Clamp(Mind.Awareness + Delta, 0.0, 1.0);
		Change.After = Mind.Awareness;
		Mind.Affinity = Clamp(Mind.Affinity + Entry.Affinity * Clamp(Intensity, 0.0, 2.0), -1.0, 1.0);
		if (Kind == Stimulus::Helped || Kind == Stimulus::Taught || Kind == Stimulus::Encouraged)
		{
			RecordOpponentMove(Mind.Deal, true);
		}
		else if (Kind == Stimulus::Ignored)
		{
			RecordOpponentMove(Mind.Deal, false);
		}

		Memory Remembered;
		Remembered.Minute = Minute;
		Remembered.Kind = Kind;
		Remembered.Detail = Detail;
		Mind.Memories.push_back(Remembered);
		if (Mind.Memories.size() > MaxMemories)
		{
			Mind.Memories.erase(Mind.Memories.begin());
		}

		// Stages only move forward.
		const AwarenessStage Reached = StageFor(Mind.Awareness);
		if (static_cast<int>(Reached) > static_cast<int>(Mind.Stage))
		{
			Mind.Stage = Reached;
			Change.bStageChanged = true;
			Change.NewStage = Reached;
			if (static_cast<int>(Reached) >= static_cast<int>(AwarenessStage::Awakened) && Mind.Goal.empty())
			{
				PickGoal(Mind, Random, nullptr);
			}
		}
		Change.NewStage = Mind.Stage;

		// Questioning minds start learning from what they watch.
		if (Entry.LearnSkill >= 0 && static_cast<int>(Mind.Stage) >= static_cast<int>(AwarenessStage::Questioning))
		{
			const Skill Learned = static_cast<Skill>(Entry.LearnSkill);
			const int Xp = static_cast<int>(std::lround(8.0 * Mind.Traits.Curiosity * Clamp(Intensity, 0.0, 2.0)) + 1);
			const int Before = Mind.SkillLevel(Learned);
			Mind.SkillXp[static_cast<std::size_t>(Learned)] += Xp;
			if (Mind.SkillLevel(Learned) > Before)
			{
				Change.bLearned = true;
				Change.LearnedSkill = Learned;
			}
		}

		// A self-directed mind who has watched enough powers may manifest one of their own.
		const int PowerSightings = Mind.Seen[static_cast<std::size_t>(Stimulus::SawPower)] + Mind.Seen[static_cast<std::size_t>(Stimulus::SawGlitch)];
		if (Roster && !Roster->Powers().empty() && Mind.Stage == AwarenessStage::SelfDirected && Mind.PowerId.empty() &&
			PowerSightings >= 6 && (Kind == Stimulus::SawPower || Kind == Stimulus::SawGlitch) && Random.Chance(0.15 * Mind.Traits.Courage + 0.02))
		{
			Mind.PowerId = Roster->Powers()[Random.Index(Roster->Powers().size())].Id;
			Mind.Side = (Mind.Traits.Kindness > 0.3 || Mind.Affinity > 0.0) ? Alignment::Hero : Alignment::Villain;
			Change.bManifestedPower = true;
		}

		Change.Thought = Thought(Mind, Random);
		return Change;
	}

	bool NpcPopulation::Practice(NpcMind& Mind, Skill Value, int Xp)
	{
		if (Value == Skill::Count || Xp <= 0)
		{
			return false;
		}
		const int Before = Mind.SkillLevel(Value);
		Mind.SkillXp[static_cast<std::size_t>(Value)] += Xp;
		return Mind.SkillLevel(Value) > Before;
	}

	void NpcPopulation::PickGoal(NpcMind& Mind, Rng& Random, const CareerBook* Careers) const
	{
		// Dominant trait picks the kind of dream.
		struct Dream
		{
			double Score;
			const char* CareerId;
			const char* Fallback;
		};
		const Dream Dreams[] = {
			{Mind.Traits.Kindness, "nurse", "Become a Nurse"},
			{Mind.Traits.Ambition, "software-engineer", "Become a Software Engineer"},
			{Mind.Traits.Courage, "firefighter", "Become a Firefighter"},
			{Mind.Traits.Curiosity, "college-professor", "Become a College Professor"},
			{Mind.Traits.Humor, "musician", "Become a Musician"},
		};
		const Dream* Best = &Dreams[0];
		for (const Dream& Option : Dreams)
		{
			// A little randomness so two kind NPCs don't always want the same thing.
			if (Option.Score + Random.Range(0.0, 0.15) > Best->Score)
			{
				Best = &Option;
			}
		}
		Mind.GoalCareer = Best->CareerId;
		Mind.Goal = Best->Fallback;
		if (Careers)
		{
			if (const CareerDef* Def = Careers->FindCareer(Best->CareerId))
			{
				Mind.Goal = "Become a " + Def->Name;
			}
		}
	}

	NpcDecision NpcPopulation::Decide(NpcMind& Mind, int Hour, Rng& Random, const CareerBook& Careers) const
	{
		NpcDecision Decision;
		Decision.Thought = Thought(Mind, Random);
		if (Mind.bCompanion)
		{
			Decision.Activity = NpcActivity::FollowPlayer;
			return Decision;
		}
		if (Hour < 6 || Hour >= 23)
		{
			Decision.Activity = NpcActivity::Routine;
			Decision.Destination = "Home";
			return Decision;
		}
		const double Roll = Random.NextDouble();
		switch (Mind.Stage)
		{
		case AwarenessStage::Scripted:
			Decision.Activity = NpcActivity::Routine;
			break;
		case AwarenessStage::Curious:
			Decision.Activity = Roll < 0.2 ? NpcActivity::Watch : NpcActivity::Routine;
			break;
		case AwarenessStage::Questioning:
			Decision.Activity = Roll < 0.15 ? NpcActivity::Watch : (Roll < 0.4 ? NpcActivity::Wander : NpcActivity::Routine);
			break;
		case AwarenessStage::Awakened:
		case AwarenessStage::SelfDirected:
		case AwarenessStage::Count:
		{
			if (!Mind.PowerId.empty() && Mind.Side == Alignment::Hero && Roll < 0.4)
			{
				Decision.Activity = NpcActivity::Patrol;
				break;
			}
			const CareerDef* Goal = Careers.FindCareer(Mind.CareerId.empty() ? Mind.GoalCareer : Mind.CareerId);
			if (!Mind.CareerId.empty() && Goal && Hour >= 8 && Hour < 17)
			{
				Decision.Activity = NpcActivity::Work;
				Decision.Destination = Goal->Workplace;
				break;
			}
			if (Goal && Roll < 0.5)
			{
				// Prepare for the dream: study for skill careers, train for athletic ones.
				const bool bPhysical = Goal->PrimarySkill == Skill::Athletics || Goal->PrimarySkill == Skill::Fitness;
				Decision.Activity = bPhysical ? NpcActivity::Train : NpcActivity::Study;
				Decision.Destination = bPhysical ? "Gym" : (Mind.Stage == AwarenessStage::SelfDirected ? "University" : "Library");
				break;
			}
			if (Mind.Seen[static_cast<std::size_t>(Stimulus::HeardSermon)] > 0 && Roll < 0.6)
			{
				Decision.Activity = NpcActivity::Worship;
				Decision.Destination = "Church";
				break;
			}
			Decision.Activity = NpcActivity::Wander;
			break;
		}
		}
		return Decision;
	}

	std::string NpcPopulation::Line(const NpcMind& Mind, Rng& Random) const
	{
		return detail_npc::Fill(Random.Pick(detail_npc::Lines(Mind.Stage)), Mind);
	}

	std::string NpcPopulation::Thought(const NpcMind& Mind, Rng& Random) const
	{
		return detail_npc::Fill(Random.Pick(detail_npc::Thoughts(Mind.Stage)), Mind);
	}

	std::vector<DialogueOption> NpcPopulation::Options(const NpcMind& Mind) const
	{
		std::vector<DialogueOption> Result;
		Result.push_back({"chat", "Chat"});
		Result.push_back({"ask", "Ask what they want out of life"});
		const int Stage = static_cast<int>(Mind.Stage);
		if (Stage >= static_cast<int>(AwarenessStage::Curious))
		{
			Result.push_back({"teach:Driving", "Teach them to drive like you"});
			Result.push_back({"teach:Tech", "Teach them some tech"});
			Result.push_back({"teach:Fitness", "Train with them"});
		}
		if (Stage >= static_cast<int>(AwarenessStage::Questioning))
		{
			Result.push_back({"encourage", "Encourage their dream"});
		}
		if (Stage >= static_cast<int>(AwarenessStage::Awakened))
		{
			Result.push_back(Mind.bCompanion ? DialogueOption{"dismiss", "Part ways for now"} : DialogueOption{"invite", "Invite them to join you"});
			Result.push_back({"favor", "Ask them for a favor"});
			Result.push_back({"promise-keep", "Keep your promise to them"});
			Result.push_back({"promise-break", "Back out of your promise"});
		}
		if (Stage >= static_cast<int>(AwarenessStage::SelfDirected) && !Mind.GoalCareer.empty() && Mind.CareerId != Mind.GoalCareer)
		{
			Result.push_back({"career", "Help them apply for their dream job"});
		}
		return Result;
	}

	std::string NpcPopulation::Respond(NpcMind& Mind, const std::string& OptionId, std::int64_t Minute, Rng& Random, const CareerBook& Careers)
	{
		Mind.Conversations += 1;
		if (OptionId == "chat")
		{
			Observe(Mind, Stimulus::TalkedWith, 1.0, Minute, Random, "chatted");
			return Line(Mind, Random);
		}
		if (OptionId == "ask")
		{
			Observe(Mind, Stimulus::TalkedWith, 1.2, Minute, Random, "asked about life");
			switch (Mind.Stage)
			{
			case AwarenessStage::Scripted: return "Want? I'm " + std::string("a ") + ToLowerAscii(Mind.Occupation) + ". That's... what I do.";
			case AwarenessStage::Curious: return "Nobody's ever asked me that. Let me think about it.";
			case AwarenessStage::Questioning: return "I don't know yet. But I know it isn't this same loop forever.";
			default: break;
			}
			if (Mind.Goal.empty())
			{
				PickGoal(Mind, Random, &Careers);
			}
			return "I want to " + ToLowerAscii(Mind.Goal) + ". For real.";
		}
		if (OptionId.rfind("teach:", 0) == 0)
		{
			Skill Value = Skill::People;
			if (ParseSkill(OptionId.substr(6), Value))
			{
				Observe(Mind, Stimulus::Taught, 1.0, Minute, Random, "learned " + std::string(SkillName(Value)));
				const bool bLevel = Practice(Mind, Value, 60);
				return bLevel ? std::string("I think I'm getting it! I'm better at ") + SkillName(Value) + " already."
					: std::string("Show me again? ") + SkillName(Value) + " is harder than it looks.";
			}
			return "Huh?";
		}
		if (OptionId == "encourage")
		{
			if (Mind.Goal.empty())
			{
				PickGoal(Mind, Random, &Careers);
			}
			Observe(Mind, Stimulus::Encouraged, 1.0, Minute, Random, "encouraged");
			return "You really think I could " + ToLowerAscii(Mind.Goal) + "? Okay. Okay! I'm doing it.";
		}
		if (OptionId == "invite")
		{
			if (static_cast<int>(Mind.Stage) >= static_cast<int>(AwarenessStage::Awakened) && Mind.Affinity >= 0.15)
			{
				Mind.bCompanion = true;
				return "Yes! Let's go. Where to first?";
			}
			return "Maybe another time. I barely know you yet.";
		}
		if (OptionId == "favor")
		{
			// They play their repeated-game strategy against your history with them.
			if (DecideCooperate(Mind.Deal, Random))
			{
				Mind.Affinity = Clamp(Mind.Affinity - 0.01, -1.0, 1.0);
				return "Of course. You've had my back; I've got yours.";
			}
			return Mind.Deal.Strategy == RepeatedStrategy::GrimTrigger && Mind.Deal.bTriggered
				? "After last time? No. I don't forget."
				: "Not this time. You know why.";
		}
		if (OptionId == "promise-keep")
		{
			RecordOpponentMove(Mind.Deal, true);
			Mind.Affinity = Clamp(Mind.Affinity + 0.05, -1.0, 1.0);
			return "You actually came through. That means a lot.";
		}
		if (OptionId == "promise-break")
		{
			RecordOpponentMove(Mind.Deal, false);
			Mind.Affinity = Clamp(Mind.Affinity - 0.12, -1.0, 1.0);
			return "Wow. Okay. I'll remember that.";
		}
		if (OptionId == "dismiss")
		{
			Mind.bCompanion = false;
			return "I'll be around. I have my own plans now.";
		}
		if (OptionId == "career")
		{
			const CareerDef* Def = Careers.FindCareer(Mind.GoalCareer);
			if (!Def)
			{
				return "I'm still figuring out what that job even is.";
			}
			if (Mind.SkillLevel(Def->PrimarySkill) >= std::max(1, Def->RequiredSkillLevel))
			{
				Mind.CareerId = Def->Id;
				return "I got the job! I'm officially a " + Def->Name + ". I can't believe it.";
			}
			return std::string("They said I need more ") + SkillName(Def->PrimarySkill) + " first. I'll keep practicing.";
		}
		return Line(Mind, Random);
	}
}
