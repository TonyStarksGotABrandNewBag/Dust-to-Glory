#include "Sim/NpcMinds.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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

		struct TroubleSeed
		{
			TroubleKind Kind;
			const char* Text;
			int LowCost;             // dollars, 1990s money
			int HighCost;
		};

		const std::vector<TroubleSeed>& Troubles()
		{
			static const std::vector<TroubleSeed> Seeds = {
				{TroubleKind::Debt, "Rent's due Friday and I'm short. Again.", 180, 650},
				{TroubleKind::Debt, "My sister co-signed for me and now the bank's calling her.", 240, 900},
				{TroubleKind::Job, "My shift manager cut my hours to nothing without saying why.", 0, 0},
				{TroubleKind::Job, "I've applied eleven places. Nobody even calls back.", 0, 0},
				{TroubleKind::Family, "My mother's in Conway on her own and I can't get out there.", 0, 0},
				{TroubleKind::Family, "My boy stopped talking to me in March. I still don't know what I did.", 0, 0},
				{TroubleKind::Car, "My truck's been dead in the yard three weeks. Starter, I think.", 90, 420},
				{TroubleKind::Car, "Transmission's slipping and the shop wants more than the car's worth.", 300, 1100},
				{TroubleKind::Fear, "Somebody's been parked outside my place at night. Same car.", 0, 0},
				{TroubleKind::Fear, "I saw something downtown I can't explain and I haven't slept since.", 0, 0},
				{TroubleKind::Faith, "I keep showing up on Sunday and feeling nothing. Is that a sin?", 0, 0},
				{TroubleKind::Loneliness, "I could go a whole week here without anybody saying my name.", 0, 0},
			};
			return Seeds;
		}

		/** Tips are drawn from what a job puts a person in earshot of. */
		struct TipSeed
		{
			TipKind Kind;
			const char* Text;
			double Value;            // what it is worth to the player, dollars
			double Risk;             // what telling costs them, dollars
		};

		const std::vector<TipSeed>& Tips()
		{
			static const std::vector<TipSeed> Seeds = {
				{TipKind::CarForSale, "There's a clean pickup sitting behind the feed store. Owner's moving and won't haggle hard.", 900.0, 40.0},
				{TipKind::CarForSale, "Widow on Red Bluff has her husband's car under a tarp. Hasn't turned a wheel in nine years.", 1400.0, 60.0},
				{TipKind::Hiring, "The shop on Highway 9 lost two guys last week. They're desperate, not picky.", 400.0, 30.0},
				{TipKind::Hiring, "Hospital's hiring overnight. Nobody wants it, so they'll take anybody with the certificate.", 500.0, 25.0},
				{TipKind::PoliceWatch, "They run the speed trap past the church between four and six. Every weekday.", 700.0, 220.0},
				{TipKind::PoliceWatch, "Night shift sits in the bank lot. If the lot's empty, the whole east side is empty.", 850.0, 300.0},
				{TipKind::BusinessTrouble, "The diner's two months behind its suppliers. Owner's asking around about selling.", 1600.0, 120.0},
				{TipKind::BusinessTrouble, "That tech outfit downtown lost its biggest account and hasn't told anyone.", 2200.0, 260.0},
				{TipKind::Shortcut, "The service road behind the mill cuts ten minutes off the run to town. Gate's never locked.", 300.0, 10.0},
				{TipKind::Person, "Ask for Dale at the salvage yard. Tell him I sent you and the price changes.", 600.0, 50.0},
			};
			return Seeds;
		}

		const std::vector<std::string>& TownLines()
		{
			static const std::vector<std::string> Lines = {
				"It's quiet. Some people call that boring. I call it quiet.",
				"Everybody here knows everybody's business and pretends they don't.",
				"Used to be three places to work. Now there's one, and it's hiring nobody.",
				"My people have been in this county four generations. I'm not going anywhere.",
				"Summer it's all plates from Ohio. Winter it's just us.",
				"The roads out here will eat a car. Watch the shoulder on the curves.",
			};
			return Lines;
		}

		/** What the town says about somebody, strongest first, picked by what they were seen doing. */
		std::string RumorFor(Stimulus Kind, const std::string& Detail)
		{
			switch (Kind)
			{
			case Stimulus::SawPower: return "somebody downtown can do things people can't do";
			case Stimulus::SawGlitch: return "the world went wrong for a second and one person walked through it";
			case Stimulus::SawStunt: return "there's a driver out here who doesn't believe in gravity";
			case Stimulus::SawCrash: return "there was a wreck and somebody walked away from it";
			case Stimulus::SawHack: return "the lights on Main changed by themselves again";
			case Stimulus::SawRace: return "they're racing out past the county line after dark";
			case Stimulus::SawVillain: return "there's somebody dangerous in town and the police know it";
			case Stimulus::HeardSermon: return "that preacher says things that stay with you";
			case Stimulus::Helped: return "somebody stopped and helped a stranger for nothing";
			case Stimulus::Taught: return "there's somebody teaching people things for free";
			default: break;
			}
			return Detail;
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

	const char* TroubleKindName(TroubleKind Value)
	{
		switch (Value)
		{
		case TroubleKind::None: return "None";
		case TroubleKind::Debt: return "Debt";
		case TroubleKind::Job: return "Job";
		case TroubleKind::Family: return "Family";
		case TroubleKind::Car: return "Car";
		case TroubleKind::Fear: return "Fear";
		case TroubleKind::Faith: return "Faith";
		case TroubleKind::Loneliness: return "Loneliness";
		case TroubleKind::Count: break;
		}
		return "None";
	}

	const char* TipKindName(TipKind Value)
	{
		switch (Value)
		{
		case TipKind::None: return "None";
		case TipKind::CarForSale: return "CarForSale";
		case TipKind::Hiring: return "Hiring";
		case TipKind::PoliceWatch: return "PoliceWatch";
		case TipKind::BusinessTrouble: return "BusinessTrouble";
		case TipKind::Shortcut: return "Shortcut";
		case TipKind::Person: return "Person";
		case TipKind::Count: break;
		}
		return "None";
	}

	int NpcMind::TimesDiscussed(const std::string& TopicId) const
	{
		for (const TopicRecord& Record : Topics)
		{
			if (Record.Id == TopicId)
			{
				return Record.Times;
			}
		}
		return 0;
	}

	const Acquaintance* NpcMind::FindKnown(const std::string& OtherId) const
	{
		for (const Acquaintance& Known : Knows)
		{
			if (Known.Id == OtherId)
			{
				return &Known;
			}
		}
		return nullptr;
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
		// Everybody is carrying something and everybody knows something. Both are set from the same
		// seed as the rest of them, so the same stranger is the same person every time the game runs.
		GiveTrouble(Mind, Random);
		GiveTip(Mind, Random);
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

	void NpcPopulation::Place(NpcMind& Mind, const std::string& TownName) const
	{
		if (Mind.Hometown.empty())
		{
			Mind.Hometown = TownName;
		}
	}

	void NpcPopulation::GiveTrouble(NpcMind& Mind, Rng& Random) const
	{
		if (Mind.Burden.Kind != TroubleKind::None)
		{
			return;
		}
		// Not everybody is in trouble. The ones who are, are in trouble in proportion to how little
		// the rest of the simulation has given them.
		if (Random.NextDouble() > 0.72)
		{
			return;
		}
		const std::vector<detail_npc::TroubleSeed>& Seeds = detail_npc::Troubles();
		const detail_npc::TroubleSeed& Seed = Seeds[static_cast<std::size_t>(Random.RangeInt(0, static_cast<int>(Seeds.size()) - 1))];
		Mind.Burden.Kind = Seed.Kind;
		Mind.Burden.Text = Seed.Text;
		Mind.Burden.Cost = Seed.HighCost > Seed.LowCost
			? Dollars(Random.RangeInt(Seed.LowCost, Seed.HighCost))
			: Money(0);
	}

	Money NpcPopulation::HelpCost(const NpcMind& Mind) const
	{
		return Mind.Burden.Cost;
	}

	void NpcPopulation::GiveTip(NpcMind& Mind, Rng& Random) const
	{
		if (Mind.Knowledge.Kind != TipKind::None)
		{
			return;
		}
		const std::vector<detail_npc::TipSeed>& Seeds = detail_npc::Tips();
		const detail_npc::TipSeed& Seed = Seeds[static_cast<std::size_t>(Random.RangeInt(0, static_cast<int>(Seeds.size()) - 1))];
		Mind.Knowledge.Kind = Seed.Kind;
		Mind.Knowledge.Text = Seed.Text;
		Mind.Knowledge.Value = Seed.Value * Random.Range(0.8, 1.25);
		// What they want for it is settled by the bargain below; store the risk in Ask for now.
		Mind.Knowledge.Ask = Dollars(static_cast<std::int64_t>(Seed.Risk));
	}

	Bargain NpcPopulation::PriceOfTip(const NpcMind& Mind) const
	{
		// Telling is worth something to the player and costs the teller something. The gain from the
		// trade is the difference; it gets split by how much each side needs the other. Somebody who
		// likes you hands it over for almost nothing, because walking away costs them a friend.
		const double Worth = Mind.Knowledge.Value;
		const double TheirRisk = ToDollars(Mind.Knowledge.Ask) * (1.0 - 0.5 * Clamp(Mind.Affinity, 0.0, 1.0));
		const double PlayerPower = Clamp(0.5 + 0.35 * Mind.Affinity + 0.1 * Mind.Traits.Kindness, 0.1, 0.95);
		return NashBargain(Worth, 0.0, TheirRisk, PlayerPower);
	}

	void NpcPopulation::Introduce(const std::string& AId, const std::string& BId, double Tie)
	{
		if (AId == BId)
		{
			return;
		}
		NpcMind* A = Find(AId);
		NpcMind* B = Find(BId);
		if (!A || !B)
		{
			return;
		}
		const double Strength = Clamp(Tie, 0.05, 1.0);
		if (!A->FindKnown(BId))
		{
			A->Knows.push_back({BId, B->Name, Strength});
			Thin(*A);
		}
		if (!B->FindKnown(AId))
		{
			B->Knows.push_back({AId, A->Name, Strength});
			Thin(*B);
		}
	}

	void NpcPopulation::Thin(NpcMind& Mind) const
	{
		// A cap, because a town where everybody knows everybody carries a rumour everywhere in one
		// hop, which is not how word gets around. The weakest tie goes, unless the new one is weaker.
		if (Mind.Knows.size() <= MaxAcquaintances)
		{
			return;
		}
		std::size_t Weakest = 0;
		for (std::size_t Index = 1; Index < Mind.Knows.size(); ++Index)
		{
			if (Mind.Knows[Index].Tie < Mind.Knows[Weakest].Tie)
			{
				Weakest = Index;
			}
		}
		Mind.Knows.erase(Mind.Knows.begin() + static_cast<std::ptrdiff_t>(Weakest));
	}

	int NpcPopulation::Mingle(NpcMind& Mind, Rng& Random, int Want)
	{
		if (Minds.size() < 2)
		{
			return 0;
		}
		const std::string Me = Mind.Id;
		int Added = 0;
		for (int Attempt = 0; Attempt < Want * 4 && Added < Want; ++Attempt)
		{
			const std::size_t Pick = static_cast<std::size_t>(Random.RangeInt(0, static_cast<int>(Minds.size()) - 1));
			const std::string Other = Minds[Pick].Id;
			if (Other == Me)
			{
				continue;
			}
			NpcMind* Self = Find(Me);
			if (!Self || Self->FindKnown(Other))
			{
				continue;
			}
			Introduce(Me, Other, Random.Range(0.2, 0.9));
			Added += 1;
		}
		return Added;
	}

	bool NpcPopulation::Tell(NpcMind& Mind, const std::string& Text, const std::string& SourceId, double Weight, int Hops,
		std::int64_t Minute)
	{
		if (Text.empty())
		{
			return false;
		}
		for (Rumor& Known : Mind.Heard)
		{
			if (Known.Text == Text)
			{
				// Hearing it twice makes it true, as far as anybody is concerned.
				Known.Weight = Clamp(Known.Weight + 0.2 * Weight, 0.0, 1.0);
				Known.Minute = Minute;
				return false;
			}
		}
		Rumor Fresh;
		Fresh.Text = Text;
		Fresh.SourceId = SourceId;
		if (const NpcMind* From = Find(SourceId))
		{
			Fresh.SourceName = From->Name;
		}
		Fresh.Weight = Clamp(Weight, 0.0, 1.0);
		Fresh.Hops = Hops;
		Fresh.Minute = Minute;
		Mind.Heard.push_back(Fresh);
		// Nobody carries everything. The oldest thing they heard goes.
		if (Mind.Heard.size() > MaxMemories)
		{
			Mind.Heard.erase(Mind.Heard.begin());
		}
		return true;
	}

	int NpcPopulation::Spread(const std::string& FromId, const std::string& Text, double Weight, std::int64_t Minute, Rng& Random,
		int Hops)
	{
		const NpcMind* From = Find(FromId);
		if (!From || Text.empty())
		{
			return 0;
		}
		// Copy the list: telling people mutates Minds, and a reference into it would not survive that.
		const std::vector<Acquaintance> Circle = From->Knows;
		const double Mouth = 0.4 + 0.6 * From->Traits.Humor; // a talker tells more people
		int Heard = 0;
		for (const Acquaintance& Known : Circle)
		{
			if (Random.NextDouble() > Known.Tie * Mouth)
			{
				continue;
			}
			if (NpcMind* To = Find(Known.Id))
			{
				// Every mouth it passes through takes something off it, and counts.
				Heard += Tell(*To, Text, FromId, Weight * 0.75, Hops + 1, Minute) ? 1 : 0;
			}
		}
		return Heard;
	}

	int NpcPopulation::TownTalks(std::int64_t Minute, Rng& Random)
	{
		// Who is going to repeat what, decided before anybody's list of rumours is touched.
		struct Retelling
		{
			std::string FromId;
			std::string Text;
			double Weight;
			int Hops;
		};
		std::vector<Retelling> Retold;
		for (NpcMind& Mind : Minds)
		{
			for (Rumor& Known : Mind.Heard)
			{
				Known.Weight *= 0.97; // yesterday's news
				// Somebody who saw something impossible yesterday will mention it to somebody today.
				// How far it actually gets is thinned by who they know, below, not by this.
				if (Known.Retold < 3 && Known.Hops < 4 && Known.Weight > 0.35 && Random.NextDouble() < 0.5 * Known.Weight)
				{
					Retold.push_back({Mind.Id, Known.Text, Known.Weight, Known.Hops});
					Known.Retold += 1;
				}
			}
			// Drop what nobody believes any more.
			std::size_t Write = 0;
			for (std::size_t Read = 0; Read < Mind.Heard.size(); ++Read)
			{
				if (Mind.Heard[Read].Weight > 0.08)
				{
					Mind.Heard[Write] = Mind.Heard[Read];
					Write += 1;
				}
			}
			Mind.Heard.resize(Write);
			// Patience comes back overnight.
			Mind.Patience = Clamp(Mind.Patience + 0.35, 0.0, 1.0);
		}
		int Total = 0;
		for (const Retelling& One : Retold)
		{
			Total += Spread(One.FromId, One.Text, One.Weight, Minute, Random, One.Hops);
		}
		return Total;
	}

	const Rumor* NpcPopulation::LoudestRumor(const NpcMind& Mind) const
	{
		const Rumor* Best = nullptr;
		for (const Rumor& Known : Mind.Heard)
		{
			if (!Best || Known.Weight > Best->Weight)
			{
				Best = &Known;
			}
		}
		return Best;
	}

	int NpcPopulation::HowManyHeard(const std::string& Text) const
	{
		int Count = 0;
		for (const NpcMind& Mind : Minds)
		{
			for (const Rumor& Known : Mind.Heard)
			{
				if (Known.Text == Text)
				{
					Count += 1;
					break;
				}
			}
		}
		return Count;
	}

	int NpcPopulation::RaiseTopic(NpcMind& Mind, const std::string& TopicId, std::int64_t Minute) const
	{
		for (TopicRecord& Record : Mind.Topics)
		{
			if (Record.Id == TopicId)
			{
				// Pestering somebody about their job wears thin within the hour, but come back
				// tomorrow and the question is a fresh one again. Without this a topic would be
				// closed for the rest of the game after three asks.
				const bool bStale = Minute - Record.LastMinute >= TopicForgetMinutes;
				const int Before = bStale ? 0 : Record.Times;
				Record.Times = Before + 1;
				Record.LastMinute = Minute;
				return Before;
			}
		}
		Mind.Topics.push_back({TopicId, 1, Minute});
		return 0;
	}

	std::string NpcPopulation::Recount(const NpcMind& Mind) const
	{
		// The strangest thing they ever saw, told the way somebody at their stage would tell it.
		if (!Mind.bSawSomething)
		{
			return "Nothing worth telling. Same streets, same hours.";
		}
		const std::string What = detail_npc::RumorFor(Mind.Witnessed.Kind, Mind.Witnessed.Detail);
		switch (Mind.Stage)
		{
		case AwarenessStage::Scripted:
			return "Saw something odd -- " + What + " -- but it was probably the heat. It was probably the heat.";
		case AwarenessStage::Curious:
			return "I keep coming back to it: " + What + ". I've stopped telling people. They look at me funny.";
		case AwarenessStage::Questioning:
			return "Here's what I know: " + What + ". And nobody in this town will say the word out loud.";
		case AwarenessStage::Awakened:
			return "It was you, wasn't it. " + What + ". I'm not scared. I just want to understand it.";
		case AwarenessStage::SelfDirected:
		case AwarenessStage::Count:
			break;
		}
		return "I've got my own name for it now. " + What + ". And I'm not going back to pretending.";
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

		// Seeing something is the start of saying something. What a witness carries away becomes the line
		// they will repeat to whoever they know, which is how the town comes to have an opinion of the
		// player without ever being told one.
		if (!Entry.bInteractive && Intensity > 0.25)
		{
			const std::string Said = RumorFor(Kind, Detail);
			Tell(Mind, Said, std::string(), Clamp(0.35 + 0.5 * Clamp(Intensity, 0.0, 2.0), 0.0, 1.0), 0, Minute);
			// Kept where talking cannot push it out: the strangest thing wins, and ties go to the newest.
			if (!Mind.bSawSomething || Entry.Weight >= detail_npc::Info(Mind.Witnessed.Kind).Weight)
			{
				Mind.bSawSomething = true;
				Mind.Witnessed = Remembered;
			}
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
		Result.push_back({"chat", "Chat", 0});
		Result.push_back({"ask", "Ask what they want out of life", 0});
		Result.push_back({"town", Mind.Hometown.empty() ? "Ask about around here" : "Ask about " + Mind.Hometown, 0});
		Result.push_back({"work", "Ask how the work is going", 0});

		// They only have something to recount if they witnessed something.
		if (Mind.bSawSomething)
		{
			Result.push_back({"saw", "Ask what they've been seeing", 0});
		}
		if (!Mind.Heard.empty())
		{
			Result.push_back({"gossip", "Ask what people are saying", 0});
		}
		// A problem is only on the table once they trust you enough to have mentioned it.
		if (Mind.Burden.Kind != TroubleKind::None && !Mind.Burden.bHelped)
		{
			if (!Mind.Burden.bShared)
			{
				Result.push_back({"trouble", "Ask if something's wrong", 0});
			}
			else
			{
				const Money Cost = HelpCost(Mind);
				Result.push_back(Cost > 0
					? DialogueOption{"help", "Cover it for them", Cost}
					: DialogueOption{"help", "Tell them you'll see what you can do", 0});
			}
		}
		if (Mind.Knowledge.Kind != TipKind::None && !Mind.Knowledge.bGiven)
		{
			const Bargain Deal = PriceOfTip(Mind);
			const Money Price = Deal.bDeal ? Dollars(static_cast<std::int64_t>(Deal.PayoffB + 0.5)) : Money(0);
			Result.push_back({"tip", "Ask if they know anything worth knowing", Price});
		}
		// Asking after the people they know is how the player learns the town is wired together.
		int Named = 0;
		for (const Acquaintance& Known : Mind.Knows)
		{
			if (Named >= 2)
			{
				break;
			}
			Result.push_back({"about:" + Known.Id, "Ask about " + Known.Name, 0});
			Named += 1;
		}

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
		// The whole outcome is in Say; this keeps the shape callers had before any of it cost money.
		// Nothing is paid for through here -- a caller that cannot see a price cannot agree to one.
		return Say(Mind, OptionId, Minute, Random, Careers, Money(0)).Reply;
	}

	TalkTurn NpcPopulation::Say(NpcMind& Mind, const std::string& OptionId, std::int64_t Minute, Rng& Random,
		const CareerBook& Careers, Money PlayerCash)
	{
		TalkTurn Turn;
		// Going over the same ground wears on people. The third time you ask a man about his work he
		// stops answering, and if you keep at it he leaves.
		static const char* const Repeatable[] = {"chat", "ask", "town", "work", "saw", "gossip"};
		bool bTracked = false;
		for (const char* const One : Repeatable)
		{
			bTracked = bTracked || OptionId == One;
		}
		if (bTracked)
		{
			const int Asked = RaiseTopic(Mind, OptionId, Minute);
			if (Asked >= 2)
			{
				Mind.Patience = Clamp(Mind.Patience - 0.25, 0.0, 1.0);
				if (Mind.Patience <= 0.0)
				{
					Mind.Affinity = Clamp(Mind.Affinity - 0.04, -1.0, 1.0);
					Turn.bEnded = true;
					Turn.Reply = "Look -- I've got somewhere to be. We've been over this.";
					return Turn;
				}
				Turn.Reply = Asked >= 3
					? "Same answer as the last two times."
					: "I already told you about that.";
				return Turn; // deliberately not counted: a brush-off is not a conversation
			}
			if (Asked == 1)
			{
				// Asked once before: they will answer, but it costs them something.
				Mind.Patience = Clamp(Mind.Patience - 0.08, 0.0, 1.0);
			}
		}
		// Everything past here is an answer, so it counts as having talked to them.
		Mind.Conversations += 1;

		if (OptionId == "chat")
		{
			Observe(Mind, Stimulus::TalkedWith, 1.0, Minute, Random, "chatted");
			Turn.Reply = Line(Mind, Random);
			return Turn;
		}
		if (OptionId == "ask")
		{
			Observe(Mind, Stimulus::TalkedWith, 1.2, Minute, Random, "asked about life");
			switch (Mind.Stage)
			{
			case AwarenessStage::Scripted:
				Turn.Reply = "Want? I'm " + std::string("a ") + ToLowerAscii(Mind.Occupation) + ". That's... what I do.";
				return Turn;
			case AwarenessStage::Curious:
				Turn.Reply = "Nobody's ever asked me that. Let me think about it.";
				return Turn;
			case AwarenessStage::Questioning:
				Turn.Reply = "I don't know yet. But I know it isn't this same loop forever.";
				return Turn;
			default: break;
			}
			if (Mind.Goal.empty())
			{
				PickGoal(Mind, Random, &Careers);
			}
			Turn.Reply = "I want to " + ToLowerAscii(Mind.Goal) + ". For real.";
			return Turn;
		}
		if (OptionId == "town")
		{
			Observe(Mind, Stimulus::TalkedWith, 0.8, Minute, Random, "asked about the town");
			const std::string Where = Mind.Hometown.empty() ? std::string("around here") : Mind.Hometown;
			Turn.Reply = Random.Pick(detail_npc::TownLines());
			if (!Mind.Hometown.empty())
			{
				Turn.Reply = Where + "? " + Turn.Reply;
			}
			return Turn;
		}
		if (OptionId == "work")
		{
			Observe(Mind, Stimulus::TalkedWith, 0.9, Minute, Random, "asked about work");
			const std::string Job = ToLowerAscii(Mind.Occupation);
			if (!Mind.CareerId.empty())
			{
				const CareerDef* Def = Careers.FindCareer(Mind.CareerId);
				if (Def)
				{
					Turn.Reply = "Better than it was. I'm a " + ToLowerAscii(Def->Name) + " now. I chose it.";
					return Turn;
				}
			}
			if (Mind.Traits.Ambition > 0.65)
			{
				Turn.Reply = "Being " + ToLowerAscii(Job) + " pays the light bill. It isn't the end of me, though.";
				return Turn;
			}
			Turn.Reply = Mind.Traits.Humor > 0.6
				? "Work's work. They haven't fired me and I haven't quit, so we're even."
				: "It's steady. I've learned not to ask for more than steady.";
			return Turn;
		}
		if (OptionId == "saw")
		{
			Observe(Mind, Stimulus::TalkedWith, 1.0, Minute, Random, "asked what they saw");
			Turn.Reply = Recount(Mind);
			return Turn;
		}
		if (OptionId == "gossip")
		{
			Observe(Mind, Stimulus::TalkedWith, 0.7, Minute, Random, "asked for gossip");
			const Rumor* Loud = LoudestRumor(Mind);
			if (!Loud)
			{
				Turn.Reply = "Nothing's going around. Or nobody tells me.";
				return Turn;
			}
			const int Mouths = HowManyHeard(Loud->Text);
			std::string Who = Loud->SourceName.empty() ? std::string("I saw it myself") : Loud->SourceName + " told me";
			Turn.Reply = Who + ": " + Loud->Text + ".";
			if (Mouths > 2)
			{
				Turn.Reply += " And I'm not the only one saying it -- that's " + std::to_string(Mouths) + " of us now.";
			}
			else if (Loud->Weight < 0.4)
			{
				Turn.Reply += " I don't know that I believe it.";
			}
			return Turn;
		}
		if (OptionId == "trouble")
		{
			RaiseTopic(Mind, "trouble", Minute);
			if (Mind.Burden.Kind == TroubleKind::None)
			{
				Turn.Reply = "I'm alright. Thanks for asking -- people don't.";
				return Turn;
			}
			// Whether they open up is a judgement about you, not a dice roll on its own.
			const double Trust = 0.25 + 0.5 * Mind.Affinity + 0.2 * Mind.Traits.Kindness +
				0.05 * static_cast<double>(Mind.Conversations > 6 ? 6 : Mind.Conversations);
			if (Trust < 0.45)
			{
				Turn.Reply = "Nothing I'd put on a stranger.";
				return Turn;
			}
			Mind.Burden.bShared = true;
			Observe(Mind, Stimulus::TalkedWith, 1.4, Minute, Random, "confided");
			Turn.Reply = Mind.Burden.Text;
			return Turn;
		}
		if (OptionId == "help")
		{
			if (Mind.Burden.Kind == TroubleKind::None || !Mind.Burden.bShared)
			{
				Turn.Reply = "Help with what?";
				return Turn;
			}
			const Money Cost = HelpCost(Mind);
			if (Cost > 0 && PlayerCash < Cost)
			{
				Turn.Reply = "You don't have it either. It's alright. Nobody does.";
				return Turn;
			}
			Mind.Burden.bHelped = true;
			Turn.Cost = Cost;
			Observe(Mind, Stimulus::Helped, 1.6, Minute, Random, "helped with " + std::string(TroubleKindName(Mind.Burden.Kind)));
			Mind.Affinity = Clamp(Mind.Affinity + 0.18, -1.0, 1.0);
			RecordOpponentMove(Mind.Deal, true);
			// This is the kind of thing a person tells everybody they know.
			Turn.bNewRumor = true;
			Turn.Rumor = "somebody stopped and helped " + Mind.Name + " when they didn't have to";
			Tell(Mind, Turn.Rumor, std::string(), 1.0, 0, Minute);
			Turn.Reply = Cost > 0
				? "You're serious. You're actually serious. I won't forget this."
				: "You'd do that? Just -- knowing somebody's trying helps more than you'd think.";
			return Turn;
		}
		if (OptionId == "tip")
		{
			RaiseTopic(Mind, "tip", Minute);
			if (Mind.Knowledge.Kind == TipKind::None || Mind.Knowledge.bGiven)
			{
				Turn.Reply = "I've told you everything I've got.";
				return Turn;
			}
			const Bargain Deal = PriceOfTip(Mind);
			if (!Deal.bDeal)
			{
				Turn.Reply = "There's a thing I know. It isn't worth what it'd cost me to say it.";
				return Turn;
			}
			const Money Price = Dollars(static_cast<std::int64_t>(Deal.PayoffB + 0.5));
			if (Price > 0 && PlayerCash < Price)
			{
				Turn.Reply = "It'll cost you, and today it looks like it costs more than you've got.";
				return Turn;
			}
			Mind.Knowledge.bGiven = true;
			Turn.Cost = Price;
			Observe(Mind, Stimulus::TalkedWith, 1.1, Minute, Random, "traded information");
			Turn.Reply = Price > 0
				? Mind.Knowledge.Text + " (and we're square -- you paid for that.)"
				: Mind.Knowledge.Text + " On the house. You've been decent to me.";
			return Turn;
		}
		if (OptionId.rfind("about:", 0) == 0)
		{
			const std::string Who = OptionId.substr(6);
			const Acquaintance* Known = Mind.FindKnown(Who);
			const NpcMind* Other = Find(Who);
			if (!Known || !Other)
			{
				Turn.Reply = "Don't know anybody by that name.";
				return Turn;
			}
			RaiseTopic(Mind, OptionId, Minute);
			Observe(Mind, Stimulus::TalkedWith, 0.6, Minute, Random, "asked about " + Other->Name);
			const std::string Lead = Known->Tie > 0.7 ? "We're close. " : (Known->Tie < 0.35 ? "I know of them. " : "We talk sometimes. ");
			if (static_cast<int>(Other->Stage) >= static_cast<int>(AwarenessStage::Questioning))
			{
				Turn.Reply = Lead + Other->Name + " has been strange lately. Asking questions nobody asks.";
				if (!Other->Goal.empty())
				{
					Turn.Reply += " Says they're going to " + ToLowerAscii(Other->Goal) + ".";
				}
				return Turn;
			}
			if (Other->Affinity < -0.2)
			{
				Turn.Reply = Lead + "And they don't care for you much, if I'm honest.";
				return Turn;
			}
			Turn.Reply = Lead + Other->Name + "'s " + ToLowerAscii(Other->Occupation) + ". Steady as they come.";
			return Turn;
		}
		if (OptionId.rfind("teach:", 0) == 0)
		{
			Skill Value = Skill::People;
			if (ParseSkill(OptionId.substr(6), Value))
			{
				Observe(Mind, Stimulus::Taught, 1.0, Minute, Random, "learned " + std::string(SkillName(Value)));
				const bool bLevel = Practice(Mind, Value, 60);
				Turn.Reply = bLevel ? std::string("I think I'm getting it! I'm better at ") + SkillName(Value) + " already."
					: std::string("Show me again? ") + SkillName(Value) + " is harder than it looks.";
				if (bLevel)
				{
					Turn.bNewRumor = true;
					Turn.Rumor = "there's somebody teaching people things for free";
					Tell(Mind, Turn.Rumor, std::string(), 0.9, 0, Minute);
				}
				return Turn;
			}
			Turn.Reply = "Huh?";
			return Turn;
		}
		if (OptionId == "encourage")
		{
			if (Mind.Goal.empty())
			{
				PickGoal(Mind, Random, &Careers);
			}
			Observe(Mind, Stimulus::Encouraged, 1.0, Minute, Random, "encouraged");
			Turn.Reply = "You really think I could " + ToLowerAscii(Mind.Goal) + "? Okay. Okay! I'm doing it.";
			return Turn;
		}
		if (OptionId == "invite")
		{
			if (static_cast<int>(Mind.Stage) >= static_cast<int>(AwarenessStage::Awakened) && Mind.Affinity >= 0.15)
			{
				Mind.bCompanion = true;
				Turn.Reply = "Yes! Let's go. Where to first?";
				return Turn;
			}
			Turn.Reply = "Maybe another time. I barely know you yet.";
			return Turn;
		}
		if (OptionId == "favor")
		{
			// They play their repeated-game strategy against your history with them.
			if (DecideCooperate(Mind.Deal, Random))
			{
				Mind.Affinity = Clamp(Mind.Affinity - 0.01, -1.0, 1.0);
				Turn.Reply = "Of course. You've had my back; I've got yours.";
				return Turn;
			}
			Turn.Reply = Mind.Deal.Strategy == RepeatedStrategy::GrimTrigger && Mind.Deal.bTriggered
				? "After last time? No. I don't forget."
				: "Not this time. You know why.";
			return Turn;
		}
		if (OptionId == "promise-keep")
		{
			RecordOpponentMove(Mind.Deal, true);
			Mind.Affinity = Clamp(Mind.Affinity + 0.05, -1.0, 1.0);
			Turn.Reply = "You actually came through. That means a lot.";
			return Turn;
		}
		if (OptionId == "promise-break")
		{
			RecordOpponentMove(Mind.Deal, false);
			Mind.Affinity = Clamp(Mind.Affinity - 0.12, -1.0, 1.0);
			// Being let down is the other thing a person tells everybody they know.
			Turn.bNewRumor = true;
			Turn.Rumor = "somebody gave " + Mind.Name + " their word and broke it";
			Tell(Mind, Turn.Rumor, std::string(), 1.0, 0, Minute);
			Turn.Reply = "Wow. Okay. I'll remember that.";
			return Turn;
		}
		if (OptionId == "dismiss")
		{
			Mind.bCompanion = false;
			Turn.Reply = "I'll be around. I have my own plans now.";
			return Turn;
		}
		if (OptionId == "career")
		{
			const CareerDef* Def = Careers.FindCareer(Mind.GoalCareer);
			if (!Def)
			{
				Turn.Reply = "I'm still figuring out what that job even is.";
				return Turn;
			}
			if (Mind.SkillLevel(Def->PrimarySkill) >= std::max(1, Def->RequiredSkillLevel))
			{
				Mind.CareerId = Def->Id;
				Turn.Reply = "I got the job! I'm officially a " + Def->Name + ". I can't believe it.";
				return Turn;
			}
			Turn.Reply = std::string("They said I need more ") + SkillName(Def->PrimarySkill) + " first. I'll keep practicing.";
			return Turn;
		}
		Turn.Reply = Line(Mind, Random);
		return Turn;
	}
}
