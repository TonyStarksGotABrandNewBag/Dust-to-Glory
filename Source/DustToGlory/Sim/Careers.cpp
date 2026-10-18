#include "Sim/Careers.h"

namespace dtg::sim
{
	namespace detail_careers
	{
		// Total XP needed to reach each skill level 0..10.
		constexpr int LevelThresholds[MaxSkillLevel + 1] = {0, 100, 250, 450, 700, 1000, 1400, 1900, 2500, 3200, 4000};

		struct NamedSkill
		{
			Skill Value;
			const char* Name;
		};

		constexpr NamedSkill SkillNames[] = {
			{Skill::Driving, "Driving"},
			{Skill::Fitness, "Fitness"},
			{Skill::Tech, "Tech"},
			{Skill::Trades, "Trades"},
			{Skill::Medicine, "Medicine"},
			{Skill::Teaching, "Teaching"},
			{Skill::People, "People"},
			{Skill::Faith, "Faith"},
			{Skill::Athletics, "Athletics"},
			{Skill::Business, "Business"},
			{Skill::Creativity, "Creativity"},
			{Skill::Science, "Science"},
		};
	}

	const char* SkillName(Skill Value)
	{
		for (const auto& Entry : detail_careers::SkillNames)
		{
			if (Entry.Value == Value)
			{
				return Entry.Name;
			}
		}
		return "Unknown";
	}

	bool ParseSkill(const std::string& Text, Skill& Out)
	{
		const std::string Lower = ToLowerAscii(Text);
		for (const auto& Entry : detail_careers::SkillNames)
		{
			if (ToLowerAscii(Entry.Name) == Lower)
			{
				Out = Entry.Value;
				return true;
			}
		}
		return false;
	}

	const char* MinigameName(MinigameKind Kind)
	{
		switch (Kind)
		{
		case MinigameKind::SkillCheck: return "SkillCheck";
		case MinigameKind::Sprint: return "Sprint";
		case MinigameKind::Precision: return "Precision";
		case MinigameKind::Sequence: return "Sequence";
		case MinigameKind::Count: break;
		}
		return "SkillCheck";
	}

	bool ParseMinigame(const std::string& Text, MinigameKind& Out)
	{
		const std::string Lower = ToLowerAscii(Text);
		for (int Index = 0; Index < static_cast<int>(MinigameKind::Count); ++Index)
		{
			const MinigameKind Kind = static_cast<MinigameKind>(Index);
			if (ToLowerAscii(MinigameName(Kind)) == Lower)
			{
				Out = Kind;
				return true;
			}
		}
		return false;
	}

	int Profile::LevelForXp(int InXp)
	{
		int Level = 0;
		for (int Candidate = 1; Candidate <= MaxSkillLevel; ++Candidate)
		{
			if (InXp >= detail_careers::LevelThresholds[Candidate])
			{
				Level = Candidate;
			}
		}
		return Level;
	}

	int Profile::XpForLevel(int Level)
	{
		return detail_careers::LevelThresholds[Clamp(Level, 0, MaxSkillLevel)];
	}

	int Profile::AddSkillXp(Skill Value, int Amount)
	{
		if (Amount <= 0 || Value == Skill::Count)
		{
			return 0;
		}
		const int Before = SkillLevel(Value);
		int& Stored = Xp[static_cast<std::size_t>(Value)];
		Stored = Clamp(Stored + Amount, 0, detail_careers::LevelThresholds[MaxSkillLevel]);
		return SkillLevel(Value) - Before;
	}

	bool Profile::HasEducation(const std::string& Id) const
	{
		for (const std::string& Degree : Degrees)
		{
			if (Degree == Id)
			{
				return true;
			}
		}
		return false;
	}

	void Profile::GrantEducation(const std::string& Id)
	{
		if (!Id.empty() && !HasEducation(Id))
		{
			Degrees.push_back(Id);
		}
	}

	void CareerBook::AddCareer(CareerDef Def)
	{
		for (CareerDef& Existing : CareerDefs)
		{
			if (Existing.Id == Def.Id)
			{
				Existing = std::move(Def);
				return;
			}
		}
		CareerDefs.push_back(std::move(Def));
	}

	void CareerBook::AddEducation(EducationDef Def)
	{
		for (EducationDef& Existing : EducationDefs)
		{
			if (Existing.Id == Def.Id)
			{
				Existing = std::move(Def);
				return;
			}
		}
		EducationDefs.push_back(std::move(Def));
	}

	const CareerDef* CareerBook::FindCareer(const std::string& Id) const
	{
		for (const CareerDef& Def : CareerDefs)
		{
			if (Def.Id == Id)
			{
				return &Def;
			}
		}
		return nullptr;
	}

	const EducationDef* CareerBook::FindEducation(const std::string& Id) const
	{
		for (const EducationDef& Def : EducationDefs)
		{
			if (Def.Id == Id)
			{
				return &Def;
			}
		}
		return nullptr;
	}

	bool CareerBook::CanApply(const Profile& Player, const std::string& CareerId, std::string* OutReason) const
	{
		const CareerDef* Def = FindCareer(CareerId);
		if (!Def)
		{
			if (OutReason)
			{
				*OutReason = "Unknown career.";
			}
			return false;
		}
		if (!Def->Education.empty() && !Player.HasEducation(Def->Education))
		{
			if (OutReason)
			{
				const EducationDef* Edu = FindEducation(Def->Education);
				*OutReason = "Requires " + (Edu ? Edu->Name : Def->Education) + ".";
			}
			return false;
		}
		if (Player.SkillLevel(Def->PrimarySkill) < Def->RequiredSkillLevel)
		{
			if (OutReason)
			{
				*OutReason = std::string("Requires ") + SkillName(Def->PrimarySkill) + " level " + std::to_string(Def->RequiredSkillLevel) + ".";
			}
			return false;
		}
		return true;
	}

	bool CareerBook::CanEnroll(const Profile& Player, const std::string& EducationId, std::string* OutReason) const
	{
		const EducationDef* Def = FindEducation(EducationId);
		if (!Def)
		{
			if (OutReason)
			{
				*OutReason = "Unknown program.";
			}
			return false;
		}
		if (Player.HasEducation(EducationId))
		{
			if (OutReason)
			{
				*OutReason = "Already completed.";
			}
			return false;
		}
		if (Player.IsEnrolled())
		{
			if (OutReason)
			{
				*OutReason = "Finish your current program first.";
			}
			return false;
		}
		if (!Def->Prerequisite.empty() && !Player.HasEducation(Def->Prerequisite))
		{
			if (OutReason)
			{
				const EducationDef* Pre = FindEducation(Def->Prerequisite);
				*OutReason = "Requires " + (Pre ? Pre->Name : Def->Prerequisite) + ".";
			}
			return false;
		}
		return true;
	}

	ShiftResult CareerBook::ResolveShift(const CareerDef& Def, CareerProgress& Progress, Profile& Player, double Performance) const
	{
		ShiftResult Result;
		if (Def.Levels.empty())
		{
			return Result;
		}
		const double Perf = Clamp(Performance, 0.0, 1.0);
		const int LevelIndex = Clamp(Progress.Level, 0, static_cast<int>(Def.Levels.size()) - 1);
		const CareerLevel& Level = Def.Levels[static_cast<std::size_t>(LevelIndex)];
		const int Hours = Def.ShiftHours > 0 ? Def.ShiftHours : 8;

		// A poor shift still pays 60% of base; a perfect one pays a 20% bonus.
		const double PayFactor = 0.6 + 0.6 * Perf;
		Result.Pay = static_cast<Money>(std::llround(static_cast<double>(Level.HourlyPay) * Hours * PayFactor));
		Result.XpGained = static_cast<int>(std::lround(10.0 * Hours * (0.5 + Perf)));
		Result.SkillXpGained = static_cast<int>(std::lround(4.0 * Hours * (0.25 + Perf)));
		Result.HoursWorked = Hours;

		Progress.CareerId = Def.Id;
		Progress.ShiftsWorked += 1;
		Progress.TotalEarned += Result.Pay;
		Progress.Xp += Result.XpGained;
		Player.AddSkillXp(Def.PrimarySkill, Result.SkillXpGained);

		const bool bCanPromote = LevelIndex + 1 < static_cast<int>(Def.Levels.size());
		if (bCanPromote && Level.XpToNext > 0 && Progress.Xp >= Level.XpToNext)
		{
			Progress.Xp -= Level.XpToNext;
			Progress.Level = LevelIndex + 1;
			Result.bPromoted = true;
		}
		Result.Title = Def.Levels[static_cast<std::size_t>(Clamp(Progress.Level, 0, static_cast<int>(Def.Levels.size()) - 1))].Title;
		Result.bValid = true;
		return Result;
	}

	ClassResult CareerBook::AttendClass(Profile& Player, double Performance, int Hours) const
	{
		ClassResult Result;
		if (!Player.IsEnrolled() || Hours <= 0)
		{
			return Result;
		}
		const EducationDef* Def = FindEducation(Player.CurrentStudy().EducationId);
		if (!Def)
		{
			Player.ClearEnrollment();
			return Result;
		}
		const double Perf = Clamp(Performance, 0.0, 1.0);
		// Strong performance counts for extra study hours, a weak session for fewer.
		const int Credited = std::max(1, static_cast<int>(std::lround(Hours * (0.5 + Perf))));
		Enrollment& Study = Player.MutableEnrollment();
		Study.HoursDone += Credited;
		Result.HoursAdded = Credited;
		Result.SkillXpGained = static_cast<int>(std::lround(6.0 * Hours * (0.25 + Perf)));
		Player.AddSkillXp(Def->TrainsSkill, Result.SkillXpGained);
		Result.EducationName = Def->Name;
		if (Study.HoursDone >= Def->StudyHours)
		{
			Player.GrantEducation(Def->Id);
			Player.ClearEnrollment();
			Result.bGraduated = true;
		}
		Result.bValid = true;
		return Result;
	}
}
