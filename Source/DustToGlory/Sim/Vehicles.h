// Vehicle catalog: fictional brands and nameplates expanded into every generation ever built,
// each with a full engine and chassis description that the tuning and physics code builds on.
#pragma once

#include "Sim/SimCommon.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	enum class BodyStyle : int
	{
		Sedan,
		Coupe,
		Hatchback,
		Wagon,
		Convertible,
		SUV,
		Pickup,
		Van,
		Muscle,
		Sports,
		Supercar,
		Luxury,
		Truck,
		OpenWheel,
		MonsterTruck,
		Dragster,
		Bus,
		Count
	};

	enum class Drivetrain : int
	{
		FWD,
		RWD,
		AWD
	};

	enum class EnginePosition : int
	{
		Front,
		Mid,
		Rear
	};

	/** Engine architecture. Drives the torque curve shape and, above all, how the engine sounds. */
	enum class EngineLayout : int
	{
		I3,
		I4,
		I5,
		I6,
		V6,
		V8Crossplane,
		V8Flatplane,
		V10,
		V12,
		Flat4,
		Flat6,
		Rotary,
		DieselI6,
		Electric,
		Count
	};

	enum class Aspiration : int
	{
		Natural,
		Turbo,
		TwinTurbo,
		Supercharged,
		Electric,
		Count
	};

	const char* BodyStyleName(BodyStyle Value);
	bool ParseBodyStyle(const std::string& Text, BodyStyle& Out);
	const char* DrivetrainName(Drivetrain Value);
	bool ParseDrivetrain(const std::string& Text, Drivetrain& Out);
	const char* EnginePositionName(EnginePosition Value);
	const char* EngineLayoutName(EngineLayout Value);
	bool ParseEngineLayout(const std::string& Text, EngineLayout& Out);
	const char* AspirationName(Aspiration Value);
	bool ParseAspiration(const std::string& Text, Aspiration& Out);
	int CylinderCount(EngineLayout Layout);

	/** A fictional manufacturer. Real makes are licensed, so the game uses parody brands across every era. */
	struct Brand
	{
		std::string Id;
		std::string Name;
		std::string Country;
		std::string Tagline;
		int Founded = 1950;
		int Defunct = 0;               // last production year, 0 when still active
		double Prestige = 0.5;         // 0 budget .. 1 luxury/exotic
		double PerformanceBias = 0.0;  // -1 economy .. +1 performance
		std::uint32_t ColorRgb = 0x8090A0;
		/** Signature engine for performance models ("Flat6", "Rotary", "I6", "V12"...), empty for none. */
		std::string SignatureEngine;
	};

	/** A model line that runs across several generations, e.g. a sedan sold from 1962 to today. */
	struct Nameplate
	{
		std::string BrandId;
		std::string Name;
		BodyStyle Body = BodyStyle::Sedan;
		Drivetrain Drive = Drivetrain::RWD;
		int FirstYear = 1970;
		int LastYear = 0;            // 0 = still in production
		int GenerationYears = 7;
		double Tier = 0.5;           // 0 entry level .. 1 flagship within its segment
		int ElectricFrom = 0;        // generations starting this year or later are electric, 0 = never
		bool bEventOnly = false;     // race machinery: not sold at dealerships
		std::string EngineHint;      // optional layout override, e.g. "V8Flatplane"
		std::string AspirationHint;  // optional: "Turbo", "Supercharged", ...
		std::string PositionHint;    // optional: "Mid", "Rear"
	};

	/** One concrete generation of a nameplate, the unit the dealership sells. */
	struct VehicleModel
	{
		std::string Id;
		std::string BrandId;
		std::string Nameplate;
		std::string DisplayName;     // "Ardent Comet (1984-1990)"
		int Generation = 1;
		int YearStart = 0;
		int YearEnd = 0;
		BodyStyle Body = BodyStyle::Sedan;
		Drivetrain Drive = Drivetrain::RWD;
		EnginePosition Position = EnginePosition::Front;
		bool bElectric = false;
		bool bEventOnly = false;

		// Engine
		EngineLayout Layout = EngineLayout::I4;
		Aspiration Induction = Aspiration::Natural;
		double DisplacementL = 2.0;
		double PowerKw = 100.0;
		double PowerRpm = 6000.0;
		double TorqueNm = 200.0;
		double TorqueRpm = 4200.0;
		double RedlineRpm = 6800.0;
		std::string EngineLabel;     // "5.7L V8"

		// Chassis
		double MassKg = 1400.0;
		double WeightFront = 0.55;   // fraction of mass on the front axle
		double DragCoefficient = 0.32;
		double FrontalAreaM2 = 2.2;
		double LengthM = 4.6;
		double WidthM = 1.8;
		double HeightM = 1.45;
		double TireGrip = 1.0;       // stock tire friction coefficient
		double WheelRadiusM = 0.32;
		int Gears = 6;
		int Seats = 5;

		// Spec-sheet figures (derived)
		double TopSpeedKph = 180.0;
		double ZeroTo100S = 9.0;

		Money Msrp = 0;              // launch price in today's money
		Money MarketPrice = 0;       // what it costs at the dealership now
	};

	/** Brands + nameplates expanded into every generation ever built. */
	class VehicleCatalog
	{
	public:
		void AddBrand(Brand Value);
		void AddNameplate(Nameplate Value);
		/** Expands nameplates into generations. PriceIndex scales prices to the world's era (see EconomyIndex). */
		void Build(int CurrentYearValue, double PriceIndex = 1.0);

		const std::vector<Brand>& Brands() const { return BrandList; }
		const std::vector<Nameplate>& Nameplates() const { return NameplateList; }
		const std::vector<VehicleModel>& Models() const { return ModelList; }
		std::vector<const VehicleModel*> ModelsForBrand(const std::string& BrandId, bool bIncludeEventOnly = false) const;
		const VehicleModel* FindModel(const std::string& Id) const;
		const Brand* FindBrand(const std::string& Id) const;
		/** Newest model of a body style, used for event machinery and NPC traffic. */
		const VehicleModel* NewestOfBody(BodyStyle Body) const;
		int CurrentYear() const { return Year; }

		/** Exposed for tests: the stats a nameplate gets for one generation. */
		static VehicleModel MakeGeneration(const Brand& Maker, const Nameplate& Line, int Generation, int YearStart, int YearEnd, int CurrentYearValue,
			double PriceIndex = 1.0);

	private:
		std::vector<Brand> BrandList;
		std::vector<Nameplate> NameplateList;
		std::vector<VehicleModel> ModelList;
		int Year = 2026;
		double Prices = 1.0;
	};

	/** Chassis defaults per body style used when building a physics setup. */
	struct BodyDefaults
	{
		double SuspensionHz = 1.6;
		double RideHeightM = 0.33;
		double SteerLockDeg = 36.0;
		double TopSpeedCapKph = 260.0;
		double CgHeightM = 0.5;
	};

	const BodyDefaults& GetBodyDefaults(BodyStyle Body);

	/** Stock gear ratios for a model (first to top), before any transmission parts or tuning. */
	std::vector<double> StockGearRatios(const VehicleModel& Model);
	/** Final drive that puts redline in top gear just above the model's top speed. */
	double StockFinalDrive(const VehicleModel& Model, const std::vector<double>& Ratios);
}
