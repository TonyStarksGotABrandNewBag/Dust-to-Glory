// Parts, tuning sheet and the full physics setup a car is driven with.
//
//   VehicleModel (catalog)  +  VehicleBuild (installed parts + tuning sheet + paint)
//        --BuildSetup-->  VehicleSetup (engine, gearbox, diff, tires, suspension, aero, brakes, sound)
//
// Parts live in Content/Data/parts.json so the catalog can grow without code changes.
#pragma once

#include "Sim/SimCommon.h"
#include "Sim/Vehicles.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	enum class PartCategory : int
	{
		Intake,
		Exhaust,
		Camshaft,
		EcuTune,
		Internals,
		Turbo,
		Supercharger,
		EngineSwap,
		Flywheel,
		Transmission,
		Clutch,
		Differential,
		Suspension,
		Brakes,
		Tires,
		Wheels,
		WeightReduction,
		Wing,
		FrontBumper,
		RearBumper,
		SideSkirts,
		Hood,
		Widebody,
		Nitrous,
		RollCage,
		Armor,
		SteeringKit,
		Count
	};

	const char* PartCategoryName(PartCategory Value);
	bool ParsePartCategory(const std::string& Text, PartCategory& Out);
	/** "Engine", "Drivetrain", "Chassis", "Aero & Body", "Specialty" - for shop menus. */
	const char* PartGroupName(PartCategory Value);

	enum class DiffType : int
	{
		Open,
		LimitedSlip,
		Locked
	};

	const char* DiffTypeName(DiffType Value);

	/** What a part changes. Every field defaults to "no change". */
	struct PartEffects
	{
		// Engine
		double PowerMul = 1.0;
		double TorqueMul = 1.0;
		double RedlineAdd = 0.0;      // rpm
		double RpmShift = 0.0;        // moves power and torque peaks up (+) or down (-), fraction of rpm
		double CamLope = 0.0;         // lumpy idle, also used by the sound model
		double InertiaMul = 1.0;      // engine rotating inertia (lightweight flywheel < 1)
		double BoostBar = 0.0;        // added boost (turbo/supercharger kits)
		double SpoolSeconds = 0.0;    // turbo lag
		double BoostThresholdRpm = 0.0;
		bool bAddsTurbo = false;
		bool bAddsSupercharger = false;
		bool bAntiLag = false;        // keeps the turbo spooled off-throttle (pops and bangs)
		// Engine swap
		std::string SwapLayout;
		std::string SwapAspiration;
		double SwapLitres = 0.0;
		double SwapPowerKw = 0.0;
		double SwapTorqueNm = 0.0;
		double SwapPowerRpm = 0.0;
		double SwapRedline = 0.0;
		double SwapMassKg = 0.0;
		// Drivetrain
		int GearsOverride = 0;
		bool bCustomRatios = false;   // unlocks individual gear ratio tuning
		bool bAdjustableFinal = false;
		double ShiftTimeMul = 1.0;
		bool bStraightCut = false;    // race gearbox whine
		double ClutchCapacityMul = 1.0;
		bool bSetsDiff = false;
		DiffType Diff = DiffType::Open;
		bool bAdjustableDiff = false;
		// Chassis
		double MassKg = 0.0;
		double MassMul = 1.0;
		double SpringMul = 1.0;
		double RideHeightAddM = 0.0;
		bool bAdjustableSuspension = false;
		double BrakeTorqueMul = 1.0;
		bool bAdjustableBrakeBias = false;
		double TireMu = 0.0;          // compound friction, 0 = keep
		double TireStiffnessMul = 1.0;
		double WheelRadiusAddM = 0.0;
		double WheelInertiaMul = 1.0;
		double TrackAddM = 0.0;
		double ChassisGripMul = 1.0;  // roll cage stiffness, widebody
		double SteerLockAddDeg = 0.0;
		// Aero & body
		double DragMul = 1.0;
		double DownforceFront = 0.0;  // N per (m/s)^2
		double DownforceRear = 0.0;
		bool bAdjustableAero = false;
		double DurabilityMul = 1.0;
		// Nitrous
		double NitroFraction = 0.0;   // extra power as a fraction of engine power
		double NitroSeconds = 0.0;
		// Sound
		double ExhaustLoudness = 0.0; // added loudness
		int ExhaustTone = -1;         // -1 keep, 0 stock, 1 sport, 2 straight pipe
		double IntakeLoudness = 0.0;
		double Pops = 0.0;            // overrun crackle
	};

	struct PartDef
	{
		std::string Id;
		std::string Name;
		std::string Maker;            // fictional aftermarket brand
		std::string Description;
		PartCategory Category = PartCategory::Intake;
		int Tier = 1;                 // 1 street, 2 sport, 3 race
		double PriceBase = 0.0;       // dollars
		double PricePercentOfCar = 0.0;
		bool bIceOnly = false;        // not for electric cars
		bool bEvOnly = false;
		std::vector<BodyStyle> Bodies; // empty = fits everything
		std::string Visual;           // body kit / wheel style id for the renderer
		PartEffects Effects;
	};

	class PartCatalog
	{
	public:
		void Add(PartDef Part);
		const PartDef* Find(const std::string& Id) const;
		const std::vector<PartDef>& All() const { return Parts; }
		std::vector<const PartDef*> ForCategory(PartCategory Category) const;
		bool Fits(const PartDef& Part, const VehicleModel& Model) const;

	private:
		std::vector<PartDef> Parts;
	};

	Money PartPrice(const PartDef& Part, const VehicleModel& Model);

	/** The GT-style tuning sheet. Fields only take effect when the relevant adjustable part is fitted. */
	struct TuningSheet
	{
		double RideHeightFrontMm = 0.0;   // offset from stock, -60..+60
		double RideHeightRearMm = 0.0;
		double SpringFrontHz = 0.0;       // 0 = part default
		double SpringRearHz = 0.0;
		int DamperBumpFront = 5;          // 1..10
		int DamperBumpRear = 5;
		int DamperReboundFront = 5;
		int DamperReboundRear = 5;
		int AntiRollFront = 5;            // 1..10
		int AntiRollRear = 5;
		double CamberFrontDeg = -1.0;     // 0..-5
		double CamberRearDeg = -0.5;
		double ToeFrontDeg = 0.0;         // -0.5..+0.5 (+ = toe-in)
		double ToeRearDeg = 0.1;
		int DownforceFront = 50;          // 0..100 % of the wing's range
		int DownforceRear = 50;
		double BrakeBias = 0.0;           // 0 = stock, otherwise front share 0.50..0.75
		int LsdInitial = 10;              // 5..60
		int LsdAccel = 20;                // 5..60
		int LsdDecel = 10;                // 5..60
		double AwdFrontSplit = 0.0;       // 0 = stock, 0.10..0.60
		std::vector<double> GearRatios;   // empty = stock
		double FinalDrive = 0.0;          // 0 = stock
		int EcuOutputPercent = 100;       // 70..100 (power restrictor)
		double BallastKg = 0.0;           // 0..200
		double BallastPosition = 0.0;     // -1 rear .. +1 front
		int TractionControl = 1;          // 0 off .. 5 max
		bool bAbs = true;
	};

	/** Installed parts, tuning sheet and paint for one owned car. */
	struct VehicleBuild
	{
		std::vector<std::string> Parts;   // at most one per category
		TuningSheet Tuning;
		std::uint32_t PaintRgb = 0xB8B8B8;
		int PaintFinish = 0;              // 0 gloss, 1 metallic, 2 matte, 3 pearl, 4 chrome
		std::uint32_t WheelRgb = 0x303030;

		bool Has(const std::string& PartId) const;
		const PartDef* Installed(const PartCatalog& Catalog, PartCategory Category) const;
		/** Installs a part, replacing the one in the same slot (and turbo <-> supercharger). */
		void Install(const PartCatalog& Catalog, const PartDef& Part);
		void Remove(const PartCatalog& Catalog, PartCategory Category);
	};

	struct EngineSetup
	{
		EngineLayout Layout = EngineLayout::I4;
		Aspiration Induction = Aspiration::Natural;
		int Cylinders = 4;
		double DisplacementL = 2.0;
		double PeakPowerKw = 100.0;
		double PowerRpm = 6000.0;
		double PeakTorqueNm = 200.0;
		double TorqueRpm = 4000.0;
		double RedlineRpm = 6800.0;
		double IdleRpm = 850.0;
		double InertiaKgm2 = 0.18;
		double BoostBar = 0.0;
		double SpoolSeconds = 0.0;
		double BoostThresholdRpm = 0.0;
		bool bAntiLag = false;
		double CamLope = 0.0;
	};

	struct GearboxSetup
	{
		std::vector<double> Ratios;
		double FinalDrive = 3.7;
		double ReverseRatio = 3.3;
		double ShiftSeconds = 0.3;
		double Efficiency = 0.88;
		double ClutchCapacityNm = 600.0;
	};

	struct DiffSetup
	{
		DiffType Type = DiffType::Open;
		double PreloadNm = 0.0;
		double AccelLock = 0.0;  // fraction of input torque that can be transferred on power
		double DecelLock = 0.0;  // ... and off power
	};

	struct TireSetup
	{
		double Mu = 1.0;
		double StiffnessMul = 1.0;
		double RadiusM = 0.32;
		double InertiaKgm2 = 1.2;
	};

	struct SuspensionSetup
	{
		double SpringFront = 30000.0;   // N/m per wheel
		double SpringRear = 30000.0;
		double BumpFront = 2500.0;      // N*s/m
		double BumpRear = 2500.0;
		double ReboundFront = 3800.0;
		double ReboundRear = 3800.0;
		double AntiRollFront = 15000.0; // N/m
		double AntiRollRear = 12000.0;
		double RestLengthFrontM = 0.45; // spring length at zero load; ride height + static sag
		double RestLengthRearM = 0.45;
		double TravelM = 0.20;          // max compression before the bump stop
		double RideHeightFrontM = 0.33;
		double RideHeightRearM = 0.33;
		double CamberFrontDeg = -1.0;
		double CamberRearDeg = -0.5;
		double ToeFrontDeg = 0.0;
		double ToeRearDeg = 0.1;
	};

	struct AeroSetup
	{
		double DragArea = 0.7;          // Cd * A
		double DownforceFront = 0.02;   // N per (m/s)^2
		double DownforceRear = 0.02;
	};

	struct BrakeSetup
	{
		double MaxTorqueFront = 1800.0; // per wheel, N*m
		double MaxTorqueRear = 1000.0;
		double HandbrakeTorque = 2500.0;
		double Bias = 0.62;
	};

	/** Inputs to the procedural engine sound and the drop-in sample banks. */
	struct SoundProfile
	{
		EngineLayout Layout = EngineLayout::I4;
		int Cylinders = 4;
		double DisplacementL = 2.0;
		double ExhaustLoudness = 1.0;
		int ExhaustTone = 0;            // 0 stock, 1 sport, 2 straight pipe
		double IntakeLoudness = 0.3;
		bool bTurbo = false;
		bool bSupercharger = false;
		bool bStraightCutGears = false;
		bool bElectric = false;
		double Pops = 0.1;
		double CamLope = 0.0;
	};

	struct VehicleSetup
	{
		std::string ModelId;
		BodyStyle Body = BodyStyle::Sedan;
		double MassKg = 1400.0;
		double WeightFront = 0.55;
		double WheelbaseM = 2.7;
		double TrackFrontM = 1.55;
		double TrackRearM = 1.55;
		double CgHeightM = 0.5;
		double SteerLockDeg = 36.0;
		Drivetrain Drive = Drivetrain::RWD;
		double AwdFrontSplit = 0.4;
		EngineSetup Engine;
		GearboxSetup Gearbox;
		DiffSetup FrontDiff;
		DiffSetup RearDiff;
		DiffSetup CenterDiff;
		TireSetup FrontTire;
		TireSetup RearTire;
		SuspensionSetup Suspension;
		AeroSetup Aero;
		BrakeSetup Brakes;
		double NitroKw = 0.0;
		double NitroSeconds = 0.0;
		double Durability = 1000.0;
		double ChassisGrip = 1.0;
		int TractionControl = 1;
		bool bAbs = true;
		SoundProfile Sound;
		std::vector<std::string> Visuals; // body kit pieces for the renderer
	};

	/** Combines a catalog model with a build into the setup the physics runs on. */
	VehicleSetup BuildSetup(const VehicleModel& Model, const VehicleBuild& Build, const PartCatalog& Catalog);

	/** Single number for event eligibility, like a performance-points rating. */
	int PerformanceRating(const VehicleSetup& Setup);
}
