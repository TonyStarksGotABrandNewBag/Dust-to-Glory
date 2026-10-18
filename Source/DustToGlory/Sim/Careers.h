#pragma once

#include "Sim/SimCommon.h"

#include <array>
#include <string>
#include <vector>

namespace dtg::sim
{
	enum class Skill : int
	{
		Driving,
		Fitness,
		Tech,
		Trades,
		Medicine,
		Teaching,
		People,
		Faith,
		Athletics,
		Business,
		Creativity,
		Science,
		Count
	};

	constexpr int SkillCount = static_cast<int>(Skill::Count);
	constexpr int MaxSkillLevel = 10;

	const char* SkillName(Skill Value);
	bool ParseSkill(const std::string& Text, Skill& Out);

	/** Which on-shift minigame a career uses. */
	enum class MinigameKind : int
	{
		SkillCheck, // hit the moving marker inside the zone
		Sprint,     // alternate left/right as fast as possible
		Precision,  // two-stage power + angle meter (throws, kicks, surgery cuts)
		Sequence,   // repeat a shown sequence of inputs
		Count
	};

	const char* MinigameName(MinigameKind Kind);
	bool ParseMinigame(const std::string& Text, MinigameKind& Out);

	struct CareerLevel
	{
		std::string Title;
		Money HourlyPay = 0;
		int XpToNext = 0; // ignored on the last level
	};

	struct CareerDef
	{
		std::string Id;
		std::string Name;
		std::string Category;      // "Trades", "Medical", "Sports", ...
		std::string Description;
		std::string Workplace;     // building role that hosts shifts, e.g. "Hospital"
		std::string Education;     // required education id, empty when none
		bool bCleanRecord = false; // a felony on the record disqualifies (police, law, teaching, medicine...)
		Skill PrimarySkill = Skill::People;
		int RequiredSkillLevel = 0;
		MinigameKind Minigame = MinigameKind::SkillCheck;
		int ShiftHours = 8;
		std::vector<std::string> Tasks; // flavour lines shown during a shift
		std::vector<CareerLevel> Levels;
	};

	struct EducationDef
	{
		std::string Id;
		std::string Name;
		std::string Description;
		Money Tuition = 0;
		int StudyHours = 0;          // classroom hours to graduate
		std::string Prerequisite;    // education id, empty when none
		Skill TrainsSkill = Skill::Science;
	};

	struct CareerProgress
	{
		std::string CareerId;
		int Level = 0;
		int Xp = 0;
		int ShiftsWorked = 0;
		Money TotalEarned = 0;
	};

	struct ShiftResult
	{
		bool bValid = false;
		Money Pay = 0;
		int XpGained = 0;
		int SkillXpGained = 0;
		bool bPromoted = false;
		std::string Title;
		int HoursWorked = 0;
	};

	struct Enrollment
	{
		std::string EducationId;
		int HoursDone = 0;
	};

	/** The player's skills, degrees and current enrollment. */
	class Profile
	{
	public:
		static int LevelForXp(int Xp);
		static int XpForLevel(int Level);

		int SkillXp(Skill Value) const { return Xp[static_cast<std::size_t>(Value)]; }
		int SkillLevel(Skill Value) const { return LevelForXp(SkillXp(Value)); }
		/** Adds XP; returns the number of levels gained. */
		int AddSkillXp(Skill Value, int Amount);

		bool HasEducation(const std::string& Id) const;
		void GrantEducation(const std::string& Id);
		const std::vector<std::string>& Educations() const { return Degrees; }

		bool IsEnrolled() const { return !CurrentEnrollment.EducationId.empty(); }
		const Enrollment& CurrentStudy() const { return CurrentEnrollment; }
		void SetEnrollment(Enrollment Value) { CurrentEnrollment = std::move(Value); }
		void ClearEnrollment() { CurrentEnrollment = Enrollment(); }
		Enrollment& MutableEnrollment() { return CurrentEnrollment; }

		void RestoreXp(const std::array<int, static_cast<std::size_t>(Skill::Count)>& InXp) { Xp = InXp; }
		const std::array<int, static_cast<std::size_t>(Skill::Count)>& AllXp() const { return Xp; }

	private:
		std::array<int, static_cast<std::size_t>(Skill::Count)> Xp{};
		std::vector<std::string> Degrees;
		Enrollment CurrentEnrollment;
	};

	struct ClassResult
	{
		bool bValid = false;
		int HoursAdded = 0;
		int SkillXpGained = 0;
		bool bGraduated = false;
		std::string EducationName;
	};

	/** Career and education definitions plus the rules for applying, working and studying. */
	class CareerBook
	{
	public:
		void AddCareer(CareerDef Def);
		void AddEducation(EducationDef Def);

		const CareerDef* FindCareer(const std::string& Id) const;
		const EducationDef* FindEducation(const std::string& Id) const;
		const std::vector<CareerDef>& Careers() const { return CareerDefs; }
		const std::vector<EducationDef>& Educations() const { return EducationDefs; }

		/** Explains the first unmet requirement in OutReason when the player cannot take the job. */
		bool CanApply(const Profile& Player, const std::string& CareerId, std::string* OutReason) const;
		bool CanEnroll(const Profile& Player, const std::string& EducationId, std::string* OutReason) const;

		/** Performance is 0..1 from the shift minigame. Updates progress and skills; does not pay the wallet. */
		ShiftResult ResolveShift(const CareerDef& Def, CareerProgress& Progress, Profile& Player, double Performance) const;

		/** One class session at the university. Graduates the player when enough hours are banked. */
		ClassResult AttendClass(Profile& Player, double Performance, int Hours) const;

	private:
		std::vector<CareerDef> CareerDefs;
		std::vector<EducationDef> EducationDefs;
	};
}
