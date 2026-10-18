#include "Sim/Tuning.h"

namespace dtg::sim
{
	namespace detail_tuning
	{
		struct NamedCategory
		{
			PartCategory Value;
			const char* Name;
			const char* Group;
		};

		constexpr NamedCategory Categories[] = {
			{PartCategory::Intake, "Intake", "Engine"},
			{PartCategory::Exhaust, "Exhaust", "Engine"},
			{PartCategory::Camshaft, "Camshaft", "Engine"},
			{PartCategory::EcuTune, "EcuTune", "Engine"},
			{PartCategory::Internals, "Internals", "Engine"},
			{PartCategory::Turbo, "Turbo", "Engine"},
			{PartCategory::Supercharger, "Supercharger", "Engine"},
			{PartCategory::EngineSwap, "EngineSwap", "Engine"},
			{PartCategory::Flywheel, "Flywheel", "Drivetrain"},
			{PartCategory::Transmission, "Transmission", "Drivetrain"},
			{PartCategory::Clutch, "Clutch", "Drivetrain"},
			{PartCategory::Differential, "Differential", "Drivetrain"},
			{PartCategory::Suspension, "Suspension", "Chassis"},
			{PartCategory::Brakes, "Brakes", "Chassis"},
			{PartCategory::Tires, "Tires", "Chassis"},
			{PartCategory::Wheels, "Wheels", "Chassis"},
			{PartCategory::WeightReduction, "WeightReduction", "Chassis"},
			{PartCategory::Wing, "Wing", "Aero & Body"},
			{PartCategory::FrontBumper, "FrontBumper", "Aero & Body"},
			{PartCategory::RearBumper, "RearBumper", "Aero & Body"},
			{PartCategory::SideSkirts, "SideSkirts", "Aero & Body"},
			{PartCategory::Hood, "Hood", "Aero & Body"},
			{PartCategory::Widebody, "Widebody", "Aero & Body"},
			{PartCategory::Nitrous, "Nitrous", "Specialty"},
			{PartCategory::RollCage, "RollCage", "Specialty"},
			{PartCategory::Armor, "Armor", "Specialty"},
			{PartCategory::SteeringKit, "SteeringKit", "Specialty"},
		};

		constexpr double Pi = 3.14159265358979;
		constexpr double Gravity = 9.81;

		bool IsRaceBody(BodyStyle Body)
		{
			return Body == BodyStyle::OpenWheel || Body == BodyStyle::Dragster || Body == BodyStyle::MonsterTruck;
		}

		double DefaultIdle(const EngineSetup& Engine, BodyStyle Body)
		{
			if (Engine.Layout == EngineLayout::Electric) return 0.0;
			if (Engine.Layout == EngineLayout::DieselI6) return 650.0;
			if (Body == BodyStyle::OpenWheel) return 4000.0;
			if (Engine.Layout == EngineLayout::Rotary) return 950.0;
			if (Engine.Layout == EngineLayout::V8Crossplane) return 700.0;
			return 850.0;
		}

		double DefaultInertia(const EngineSetup& Engine)
		{
			if (Engine.Layout == EngineLayout::Electric) return 0.05;
			if (Engine.Layout == EngineLayout::DieselI6) return 0.05 + 0.09 * Engine.DisplacementL;
			if (Engine.Layout == EngineLayout::Rotary) return 0.08;
			return 0.05 + 0.03 * Engine.DisplacementL;
		}

		void DefaultBoost(EngineSetup& Engine, int Year)
		{
			switch (Engine.Induction)
			{
			case Aspiration::Turbo:
				Engine.BoostBar = Engine.Layout == EngineLayout::DieselI6 ? 1.6 : 0.9;
				Engine.SpoolSeconds = Year >= 2012 ? 0.45 : 0.75;
				Engine.BoostThresholdRpm = 0.35 * Engine.PowerRpm;
				break;
			case Aspiration::TwinTurbo:
				Engine.BoostBar = 1.1;
				Engine.SpoolSeconds = Year >= 2012 ? 0.4 : 0.6;
				Engine.BoostThresholdRpm = 0.3 * Engine.PowerRpm;
				break;
			case Aspiration::Supercharged:
				Engine.BoostBar = 0.7;
				Engine.SpoolSeconds = 0.05;
				Engine.BoostThresholdRpm = 0.0;
				break;
			default:
				Engine.BoostBar = 0.0;
				Engine.SpoolSeconds = 0.0;
				Engine.BoostThresholdRpm = 0.0;
				break;
			}
		}

		double DefaultShiftSeconds(const VehicleModel& Model)
		{
			if (Model.bElectric) return 0.0;
			switch (Model.Body)
			{
			case BodyStyle::OpenWheel: return 0.05;
			case BodyStyle::Dragster: return 0.10;
			case BodyStyle::Truck: return 0.60;
			case BodyStyle::Bus: return 0.50;
			default: break;
			}
			if (Model.YearEnd >= 2010 && (Model.Body == BodyStyle::Sports || Model.Body == BodyStyle::Supercar)) return 0.12;
			return Model.YearEnd < 1990 ? 0.45 : 0.30;
		}

		DiffSetup LsdFromSheet(const TuningSheet& Sheet)
		{
			DiffSetup Diff;
			Diff.Type = DiffType::LimitedSlip;
			Diff.PreloadNm = 10.0 * Clamp(Sheet.LsdInitial, 5, 60);
			Diff.AccelLock = Clamp(Sheet.LsdAccel, 5, 60) / 60.0 * 0.9;
			Diff.DecelLock = Clamp(Sheet.LsdDecel, 5, 60) / 60.0 * 0.7;
			return Diff;
		}

		DiffSetup DefaultLsd()
		{
			DiffSetup Diff;
			Diff.Type = DiffType::LimitedSlip;
			Diff.PreloadNm = 60.0;
			Diff.AccelLock = 0.35;
			Diff.DecelLock = 0.15;
			return Diff;
		}

		DiffSetup Locked()
		{
			DiffSetup Diff;
			Diff.Type = DiffType::Locked;
			Diff.PreloadNm = 100000.0;
			Diff.AccelLock = 1.0;
			Diff.DecelLock = 1.0;
			return Diff;
		}
	}

	const char* PartCategoryName(PartCategory Value)
	{
		for (const auto& Entry : detail_tuning::Categories)
		{
			if (Entry.Value == Value)
			{
				return Entry.Name;
			}
		}
		return "Part";
	}

	const char* PartGroupName(PartCategory Value)
	{
		for (const auto& Entry : detail_tuning::Categories)
		{
			if (Entry.Value == Value)
			{
				return Entry.Group;
			}
		}
		return "Specialty";
	}

	bool ParsePartCategory(const std::string& Text, PartCategory& Out)
	{
		const std::string Lower = ToLowerAscii(Text);
		for (const auto& Entry : detail_tuning::Categories)
		{
			if (ToLowerAscii(Entry.Name) == Lower)
			{
				Out = Entry.Value;
				return true;
			}
		}
		return false;
	}

	const char* DiffTypeName(DiffType Value)
	{
		switch (Value)
		{
		case DiffType::Open: return "Open";
		case DiffType::LimitedSlip: return "Limited-slip";
		case DiffType::Locked: return "Locked";
		}
		return "Open";
	}

	void PartCatalog::Add(PartDef Part)
	{
		for (PartDef& Existing : Parts)
		{
			if (Existing.Id == Part.Id)
			{
				Existing = std::move(Part);
				return;
			}
		}
		Parts.push_back(std::move(Part));
	}

	const PartDef* PartCatalog::Find(const std::string& Id) const
	{
		for (const PartDef& Part : Parts)
		{
			if (Part.Id == Id)
			{
				return &Part;
			}
		}
		return nullptr;
	}

	std::vector<const PartDef*> PartCatalog::ForCategory(PartCategory Category) const
	{
		std::vector<const PartDef*> Result;
		for (const PartDef& Part : Parts)
		{
			if (Part.Category == Category)
			{
				Result.push_back(&Part);
			}
		}
		return Result;
	}

	bool PartCatalog::Fits(const PartDef& Part, const VehicleModel& Model) const
	{
		if (Part.bIceOnly && Model.bElectric)
		{
			return false;
		}
		if (Part.bEvOnly && !Model.bElectric)
		{
			return false;
		}
		if (Part.Category == PartCategory::Turbo && Model.Layout == EngineLayout::DieselI6)
		{
			return true;
		}
		if (!Part.Bodies.empty())
		{
			bool bMatch = false;
			for (const BodyStyle Body : Part.Bodies)
			{
				bMatch = bMatch || Body == Model.Body;
			}
			return bMatch;
		}
		return true;
	}

	Money PartPrice(const PartDef& Part, const VehicleModel& Model)
	{
		const double CarValue = std::max(8000.0, ToDollars(Model.MarketPrice));
		const double Price = Part.PriceBase + Part.PricePercentOfCar * CarValue;
		return ToMoney(std::round(Price / 10.0) * 10.0);
	}

	bool VehicleBuild::Has(const std::string& PartId) const
	{
		for (const std::string& Id : Parts)
		{
			if (Id == PartId)
			{
				return true;
			}
		}
		return false;
	}

	const PartDef* VehicleBuild::Installed(const PartCatalog& Catalog, PartCategory Category) const
	{
		for (const std::string& Id : Parts)
		{
			const PartDef* Part = Catalog.Find(Id);
			if (Part && Part->Category == Category)
			{
				return Part;
			}
		}
		return nullptr;
	}

	void VehicleBuild::Remove(const PartCatalog& Catalog, PartCategory Category)
	{
		std::vector<std::string> Kept;
		for (const std::string& Id : Parts)
		{
			const PartDef* Part = Catalog.Find(Id);
			if (!Part || Part->Category != Category)
			{
				Kept.push_back(Id);
			}
		}
		Parts = std::move(Kept);
	}

	void VehicleBuild::Install(const PartCatalog& Catalog, const PartDef& Part)
	{
		Remove(Catalog, Part.Category);
		if (Part.Category == PartCategory::Turbo)
		{
			Remove(Catalog, PartCategory::Supercharger);
		}
		if (Part.Category == PartCategory::Supercharger)
		{
			Remove(Catalog, PartCategory::Turbo);
		}
		Parts.push_back(Part.Id);
	}

	VehicleSetup BuildSetup(const VehicleModel& Model, const VehicleBuild& Build, const PartCatalog& Catalog)
	{
		using namespace detail_tuning;
		const TuningSheet& Sheet = Build.Tuning;
		const BodyDefaults& Defaults = GetBodyDefaults(Model.Body);

		std::vector<const PartDef*> Installed;
		for (const std::string& Id : Build.Parts)
		{
			const PartDef* Part = Catalog.Find(Id);
			if (Part && Catalog.Fits(*Part, Model))
			{
				Installed.push_back(Part);
			}
		}

		VehicleSetup Setup;
		Setup.ModelId = Model.Id;
		Setup.Body = Model.Body;
		Setup.Drive = Model.Drive;

		// ---------------------------------------------------------------- Engine
		EngineSetup& Engine = Setup.Engine;
		Engine.Layout = Model.Layout;
		Engine.Induction = Model.Induction;
		Engine.DisplacementL = Model.DisplacementL;
		Engine.PeakPowerKw = Model.PowerKw;
		Engine.PowerRpm = Model.PowerRpm;
		Engine.PeakTorqueNm = Model.TorqueNm;
		Engine.TorqueRpm = Model.TorqueRpm;
		Engine.RedlineRpm = Model.RedlineRpm;
		double SwapMass = 0.0;

		for (const PartDef* Part : Installed)
		{
			const PartEffects& Fx = Part->Effects;
			if (Part->Category == PartCategory::EngineSwap && !Fx.SwapLayout.empty())
			{
				EngineLayout Layout = Engine.Layout;
				if (ParseEngineLayout(Fx.SwapLayout, Layout))
				{
					Engine.Layout = Layout;
				}
				Aspiration Induction = Aspiration::Natural;
				if (!Fx.SwapAspiration.empty() && ParseAspiration(Fx.SwapAspiration, Induction))
				{
					Engine.Induction = Induction;
				}
				else
				{
					Engine.Induction = Aspiration::Natural;
				}
				Engine.DisplacementL = Fx.SwapLitres > 0.0 ? Fx.SwapLitres : Engine.DisplacementL;
				Engine.PeakPowerKw = Fx.SwapPowerKw > 0.0 ? Fx.SwapPowerKw : Engine.PeakPowerKw;
				Engine.PeakTorqueNm = Fx.SwapTorqueNm > 0.0 ? Fx.SwapTorqueNm : Engine.PeakTorqueNm;
				Engine.PowerRpm = Fx.SwapPowerRpm > 0.0 ? Fx.SwapPowerRpm : Engine.PowerRpm;
				Engine.RedlineRpm = Fx.SwapRedline > 0.0 ? Fx.SwapRedline : Engine.PowerRpm * 1.1;
				Engine.TorqueRpm = (Engine.Induction == Aspiration::Turbo || Engine.Induction == Aspiration::TwinTurbo)
					? std::max(2500.0, 0.45 * Engine.PowerRpm) : 0.7 * Engine.PowerRpm;
				SwapMass = Fx.SwapMassKg;
			}
		}
		Engine.Cylinders = CylinderCount(Engine.Layout);
		DefaultBoost(Engine, Model.YearEnd);
		Engine.InertiaKgm2 = DefaultInertia(Engine) * (IsRaceBody(Model.Body) ? 0.45 : 1.0);

		double RpmShift = 0.0;
		for (const PartDef* Part : Installed)
		{
			const PartEffects& Fx = Part->Effects;
			Engine.PeakPowerKw *= Fx.PowerMul;
			Engine.PeakTorqueNm *= Fx.TorqueMul;
			Engine.RedlineRpm += Fx.RedlineAdd;
			Engine.InertiaKgm2 *= Fx.InertiaMul;
			Engine.CamLope += Fx.CamLope;
			Engine.bAntiLag = Engine.bAntiLag || Fx.bAntiLag;
			RpmShift += Fx.RpmShift;
			if (Fx.bAddsTurbo)
			{
				if (Engine.Induction == Aspiration::Natural || Engine.Induction == Aspiration::Supercharged)
				{
					Engine.Induction = Aspiration::Turbo;
					Engine.BoostBar = Fx.BoostBar;
					Engine.TorqueRpm = std::max(2500.0, 0.45 * Engine.PowerRpm);
				}
				else
				{
					Engine.BoostBar += Fx.BoostBar;
				}
				Engine.SpoolSeconds = std::max(Engine.SpoolSeconds, Fx.SpoolSeconds);
				if (Fx.BoostThresholdRpm > 0.0)
				{
					Engine.BoostThresholdRpm = Fx.BoostThresholdRpm;
				}
			}
			if (Fx.bAddsSupercharger)
			{
				Engine.Induction = Aspiration::Supercharged;
				Engine.BoostBar = std::max(Engine.BoostBar, Fx.BoostBar);
				Engine.SpoolSeconds = 0.05;
				Engine.BoostThresholdRpm = 0.0;
				Engine.TorqueRpm = 0.55 * Engine.PowerRpm;
			}
		}
		if (RpmShift != 0.0)
		{
			const double Shift = Clamp(RpmShift, -0.3, 0.3);
			Engine.PowerRpm *= 1.0 + Shift;
			Engine.TorqueRpm *= 1.0 + Shift;
			Engine.RedlineRpm *= 1.0 + 0.8 * Shift;
		}
		Engine.RedlineRpm = std::max(Engine.RedlineRpm, Engine.PowerRpm * 1.02);
		const double Restrictor = Clamp(Sheet.EcuOutputPercent, 70, 100) / 100.0;
		Engine.PeakPowerKw *= Restrictor;
		Engine.PeakTorqueNm *= Restrictor;
		Engine.IdleRpm = DefaultIdle(Engine, Model.Body);
		Engine.CamLope = Clamp(Engine.CamLope, 0.0, 1.0);

		// ---------------------------------------------------------------- Mass and chassis
		double Mass = Model.MassKg + SwapMass;
		double MassMul = 1.0;
		double TrackAdd = 0.0;
		double RideAdd = 0.0;
		Setup.ChassisGrip = 1.0;
		Setup.SteerLockDeg = Defaults.SteerLockDeg;
		Setup.Durability = 800.0 * (Model.MassKg / 1450.0);
		for (const PartDef* Part : Installed)
		{
			const PartEffects& Fx = Part->Effects;
			Mass += Fx.MassKg;
			MassMul *= Fx.MassMul;
			TrackAdd += Fx.TrackAddM;
			RideAdd += Fx.RideHeightAddM;
			Setup.ChassisGrip *= Fx.ChassisGripMul;
			Setup.SteerLockDeg += Fx.SteerLockAddDeg;
			Setup.Durability *= Fx.DurabilityMul;
			if (!Part->Visual.empty())
			{
				Setup.Visuals.push_back(Part->Visual);
			}
		}
		if (Model.Body == BodyStyle::MonsterTruck)
		{
			Setup.Durability *= 2.0;
		}
		Mass = std::max(300.0, Mass * MassMul);
		const double Ballast = Clamp(Sheet.BallastKg, 0.0, 200.0);
		const double BallastFront = 0.5 + 0.5 * Clamp(Sheet.BallastPosition, -1.0, 1.0);
		Setup.WeightFront = (Model.WeightFront * Mass + Ballast * BallastFront) / (Mass + Ballast);
		Setup.MassKg = Mass + Ballast;
		Setup.WheelbaseM = Model.LengthM * (Model.Body == BodyStyle::Bus ? 0.55 : 0.6);
		Setup.TrackFrontM = Model.WidthM * 0.85 + TrackAdd;
		Setup.TrackRearM = Model.WidthM * 0.85 + TrackAdd;

		// ---------------------------------------------------------------- Tires and wheels
		double Mu = Model.TireGrip;
		double Stiffness = 1.0;
		double RadiusAdd = 0.0;
		double WheelInertiaMul = 1.0;
		for (const PartDef* Part : Installed)
		{
			const PartEffects& Fx = Part->Effects;
			if (Fx.TireMu > 0.0)
			{
				Mu = Fx.TireMu;
			}
			Stiffness *= Fx.TireStiffnessMul;
			RadiusAdd += Fx.WheelRadiusAddM;
			WheelInertiaMul *= Fx.WheelInertiaMul;
		}
		const double Radius = std::max(0.2, Model.WheelRadiusM + RadiusAdd);
		const double WheelInertia = (Model.Body == BodyStyle::MonsterTruck ? 30.0 : 1.2 * std::pow(Radius / 0.32, 2.0)) * WheelInertiaMul;
		Setup.FrontTire = TireSetup{Mu, Stiffness, Radius, WheelInertia};
		Setup.RearTire = TireSetup{Mu * (TrackAdd > 0.0 ? 1.03 : 1.0), Stiffness, Radius, WheelInertia};

		// ---------------------------------------------------------------- Gearbox and clutch
		VehicleModel GearModel = Model;
		GearModel.RedlineRpm = Engine.RedlineRpm;
		GearModel.WheelRadiusM = Radius;
		bool bCustomRatios = false;
		bool bAdjustableFinal = false;
		double ShiftMul = 1.0;
		double ClutchMul = 1.0;
		bool bStraightCut = IsRaceBody(Model.Body);
		for (const PartDef* Part : Installed)
		{
			const PartEffects& Fx = Part->Effects;
			if (Fx.GearsOverride > 0 && !Model.bElectric)
			{
				GearModel.Gears = Fx.GearsOverride;
			}
			bCustomRatios = bCustomRatios || Fx.bCustomRatios;
			bAdjustableFinal = bAdjustableFinal || Fx.bAdjustableFinal || Fx.bCustomRatios;
			ShiftMul *= Fx.ShiftTimeMul;
			ClutchMul *= Fx.ClutchCapacityMul;
			bStraightCut = bStraightCut || Fx.bStraightCut;
		}
		GearboxSetup& Gearbox = Setup.Gearbox;
		Gearbox.Ratios = StockGearRatios(GearModel);
		if (bCustomRatios && Sheet.GearRatios.size() == Gearbox.Ratios.size())
		{
			for (std::size_t Index = 0; Index < Gearbox.Ratios.size(); ++Index)
			{
				Gearbox.Ratios[Index] = Clamp(Sheet.GearRatios[Index], 0.4, 6.0);
			}
		}
		Gearbox.FinalDrive = StockFinalDrive(GearModel, StockGearRatios(GearModel));
		if (bAdjustableFinal && Sheet.FinalDrive > 0.0)
		{
			Gearbox.FinalDrive = Clamp(Sheet.FinalDrive, 1.5, Model.Body == BodyStyle::Truck ? 8.0 : 6.5);
		}
		Gearbox.ReverseRatio = Gearbox.Ratios.front() * 0.95;
		Gearbox.ShiftSeconds = DefaultShiftSeconds(Model) * ShiftMul;
		Gearbox.Efficiency = Model.Drive == Drivetrain::AWD ? 0.85 : (Model.Drive == Drivetrain::FWD ? 0.90 : 0.88);
		// The stock clutch is sized for the stock engine: big power without a clutch upgrade will slip.
		Gearbox.ClutchCapacityNm = Model.TorqueNm * (Model.bElectric ? 3.0 : 1.4) * ClutchMul;

		// ---------------------------------------------------------------- Differentials
		const bool bSporty = Model.Body == BodyStyle::Sports || Model.Body == BodyStyle::Supercar || Model.Body == BodyStyle::Muscle ||
			Model.Body == BodyStyle::OpenWheel;
		const PartDef* DiffPart = nullptr;
		for (const PartDef* Part : Installed)
		{
			if (Part->Effects.bSetsDiff)
			{
				DiffPart = Part;
			}
		}
		DiffSetup Driven;
		if (DiffPart)
		{
			if (DiffPart->Effects.Diff == DiffType::Locked)
			{
				Driven = Locked();
			}
			else if (DiffPart->Effects.Diff == DiffType::LimitedSlip)
			{
				Driven = DiffPart->Effects.bAdjustableDiff ? LsdFromSheet(Sheet) : DefaultLsd();
			}
		}
		else if (Model.Body == BodyStyle::Dragster || Model.Body == BodyStyle::MonsterTruck)
		{
			Driven = Locked();
		}
		else if (bSporty && Model.YearEnd >= 1985)
		{
			Driven = DefaultLsd();
		}
		Setup.FrontDiff = DiffSetup();
		Setup.RearDiff = DiffSetup();
		Setup.CenterDiff = DiffSetup();
		switch (Model.Drive)
		{
		case Drivetrain::FWD: Setup.FrontDiff = Driven; break;
		case Drivetrain::RWD: Setup.RearDiff = Driven; break;
		case Drivetrain::AWD:
			Setup.RearDiff = Driven;
			if (DiffPart)
			{
				Setup.FrontDiff = Driven;
			}
			Setup.CenterDiff = DefaultLsd();
			break;
		}
		if (Model.Body == BodyStyle::MonsterTruck)
		{
			Setup.FrontDiff = Locked();
			Setup.RearDiff = Locked();
		}
		Setup.AwdFrontSplit = Sheet.AwdFrontSplit > 0.0 ? Clamp(Sheet.AwdFrontSplit, 0.1, 0.6) : (bSporty ? 0.35 : 0.45);

		// ---------------------------------------------------------------- Suspension
		bool bAdjustableSuspension = false;
		double SpringMul = 1.0;
		for (const PartDef* Part : Installed)
		{
			bAdjustableSuspension = bAdjustableSuspension || Part->Effects.bAdjustableSuspension;
			SpringMul *= Part->Effects.SpringMul;
		}
		SuspensionSetup& Susp = Setup.Suspension;
		double HzFront = Defaults.SuspensionHz * SpringMul;
		double HzRear = Defaults.SuspensionHz * SpringMul * 1.05;
		int BumpF = 5, BumpR = 5, RebF = 5, RebR = 5, ArbF = 5, ArbR = 5;
		double RideFront = Defaults.RideHeightM + RideAdd;
		double RideRear = Defaults.RideHeightM + RideAdd;
		if (bAdjustableSuspension)
		{
			if (Sheet.SpringFrontHz > 0.0) HzFront = Clamp(Sheet.SpringFrontHz, 0.8, 5.0);
			if (Sheet.SpringRearHz > 0.0) HzRear = Clamp(Sheet.SpringRearHz, 0.8, 5.0);
			BumpF = Clamp(Sheet.DamperBumpFront, 1, 10);
			BumpR = Clamp(Sheet.DamperBumpRear, 1, 10);
			RebF = Clamp(Sheet.DamperReboundFront, 1, 10);
			RebR = Clamp(Sheet.DamperReboundRear, 1, 10);
			ArbF = Clamp(Sheet.AntiRollFront, 1, 10);
			ArbR = Clamp(Sheet.AntiRollRear, 1, 10);
			RideFront += Clamp(Sheet.RideHeightFrontMm, -60.0, 60.0) / 1000.0;
			RideRear += Clamp(Sheet.RideHeightRearMm, -60.0, 60.0) / 1000.0;
			Susp.CamberFrontDeg = Clamp(Sheet.CamberFrontDeg, -5.0, 0.0);
			Susp.CamberRearDeg = Clamp(Sheet.CamberRearDeg, -5.0, 0.0);
			Susp.ToeFrontDeg = Clamp(Sheet.ToeFrontDeg, -0.5, 0.5);
			Susp.ToeRearDeg = Clamp(Sheet.ToeRearDeg, -0.5, 0.5);
		}
		RideFront = std::max(0.06, RideFront);
		RideRear = std::max(0.06, RideRear);
		const double CornerFront = Setup.MassKg * Setup.WeightFront * 0.5;
		const double CornerRear = Setup.MassKg * (1.0 - Setup.WeightFront) * 0.5;
		Susp.SpringFront = CornerFront * std::pow(2.0 * Pi * HzFront, 2.0);
		Susp.SpringRear = CornerRear * std::pow(2.0 * Pi * HzRear, 2.0);
		const double CritFront = 2.0 * std::sqrt(Susp.SpringFront * CornerFront);
		const double CritRear = 2.0 * std::sqrt(Susp.SpringRear * CornerRear);
		Susp.BumpFront = CritFront * (0.15 + 0.06 * BumpF);
		Susp.BumpRear = CritRear * (0.15 + 0.06 * BumpR);
		Susp.ReboundFront = CritFront * (0.25 + 0.08 * RebF);
		Susp.ReboundRear = CritRear * (0.25 + 0.08 * RebR);
		const double AverageSpring = 0.5 * (Susp.SpringFront + Susp.SpringRear);
		Susp.AntiRollFront = AverageSpring * (0.15 + 0.13 * ArbF);
		Susp.AntiRollRear = AverageSpring * (0.15 + 0.11 * ArbR);
		const double SagFront = CornerFront * Gravity / Susp.SpringFront;
		const double SagRear = CornerRear * Gravity / Susp.SpringRear;
		Susp.RideHeightFrontM = RideFront;
		Susp.RideHeightRearM = RideRear;
		Susp.RestLengthFrontM = RideFront + SagFront;
		Susp.RestLengthRearM = RideRear + SagRear;
		Susp.TravelM = std::min(0.25, std::max(0.08, 0.6 * std::min(Susp.RestLengthFrontM, Susp.RestLengthRearM)));
		Setup.CgHeightM = std::max(0.2, Defaults.CgHeightM + 0.8 * (0.5 * (RideFront + RideRear) - Defaults.RideHeightM));

		// ---------------------------------------------------------------- Aero
		double DragMul = 1.0;
		double PartFront = 0.0;
		double PartRear = 0.0;
		bool bAdjustableAero = false;
		for (const PartDef* Part : Installed)
		{
			DragMul *= Part->Effects.DragMul;
			PartFront += Part->Effects.DownforceFront;
			PartRear += Part->Effects.DownforceRear;
			bAdjustableAero = bAdjustableAero || Part->Effects.bAdjustableAero;
		}
		if (bAdjustableAero)
		{
			PartFront *= 0.5 + 0.5 * Clamp(Sheet.DownforceFront, 0, 100) / 100.0;
			PartRear *= 0.5 + 0.5 * Clamp(Sheet.DownforceRear, 0, 100) / 100.0;
		}
		double BaseDownforce = 0.04;
		switch (Model.Body)
		{
		case BodyStyle::OpenWheel: BaseDownforce = Model.YearEnd < 1968 ? 0.1 : 3.2; break;
		case BodyStyle::Supercar: BaseDownforce = 0.35; break;
		case BodyStyle::Sports: BaseDownforce = 0.15; break;
		case BodyStyle::Dragster: BaseDownforce = 0.6; break;
		default: break;
		}
		Setup.Aero.DownforceFront = 0.45 * BaseDownforce + PartFront;
		Setup.Aero.DownforceRear = 0.55 * BaseDownforce + PartRear;
		// Induced drag: every bit of added downforce costs some top speed.
		Setup.Aero.DragArea = Model.DragCoefficient * Model.FrontalAreaM2 * DragMul + 0.004 * (PartFront + PartRear);

		// ---------------------------------------------------------------- Brakes
		double BrakeMul = 1.0;
		bool bAdjustableBias = false;
		for (const PartDef* Part : Installed)
		{
			BrakeMul *= Part->Effects.BrakeTorqueMul;
			bAdjustableBias = bAdjustableBias || Part->Effects.bAdjustableBrakeBias;
		}
		double Bias = 0.62;
		if (Model.Drive == Drivetrain::FWD) Bias = 0.66;
		if (Model.Position == EnginePosition::Mid) Bias = 0.58;
		if (Model.Position == EnginePosition::Rear) Bias = 0.55;
		if (bAdjustableBias && Sheet.BrakeBias > 0.0)
		{
			Bias = Clamp(Sheet.BrakeBias, 0.45, 0.8);
		}
		const double BrakeForce = 1.25 * Setup.MassKg * Gravity * BrakeMul;
		Setup.Brakes.Bias = Bias;
		Setup.Brakes.MaxTorqueFront = BrakeForce * Bias * 0.5 * Radius;
		Setup.Brakes.MaxTorqueRear = BrakeForce * (1.0 - Bias) * 0.5 * Radius;
		Setup.Brakes.HandbrakeTorque = 0.6 * Setup.MassKg * Gravity * 0.5 * Radius;

		// ---------------------------------------------------------------- Nitrous and assists
		double NitroFraction = 0.0;
		double NitroSeconds = 0.0;
		for (const PartDef* Part : Installed)
		{
			NitroFraction += Part->Effects.NitroFraction;
			NitroSeconds = std::max(NitroSeconds, Part->Effects.NitroSeconds);
		}
		Setup.NitroKw = Engine.PeakPowerKw * NitroFraction;
		Setup.NitroSeconds = NitroFraction > 0.0 ? NitroSeconds : 0.0;
		Setup.TractionControl = Clamp(Sheet.TractionControl, 0, 5);
		Setup.bAbs = Sheet.bAbs;

		// ---------------------------------------------------------------- Sound
		SoundProfile& Sound = Setup.Sound;
		Sound.Layout = Engine.Layout;
		Sound.Cylinders = Engine.Cylinders;
		Sound.DisplacementL = Engine.DisplacementL;
		Sound.bElectric = Engine.Layout == EngineLayout::Electric;
		Sound.bTurbo = Engine.Induction == Aspiration::Turbo || Engine.Induction == Aspiration::TwinTurbo;
		Sound.bSupercharger = Engine.Induction == Aspiration::Supercharged;
		Sound.bStraightCutGears = bStraightCut;
		Sound.ExhaustTone = IsRaceBody(Model.Body) ? 2 : ((Model.Body == BodyStyle::Sports || Model.Body == BodyStyle::Supercar || Model.Body == BodyStyle::Muscle) ? 1 : 0);
		Sound.ExhaustLoudness = 1.0;
		Sound.IntakeLoudness = 0.3;
		Sound.Pops = Sound.ExhaustTone >= 1 ? 0.3 : 0.1;
		Sound.CamLope = Engine.CamLope;
		for (const PartDef* Part : Installed)
		{
			const PartEffects& Fx = Part->Effects;
			Sound.ExhaustLoudness += Fx.ExhaustLoudness;
			Sound.IntakeLoudness += Fx.IntakeLoudness;
			Sound.Pops += Fx.Pops;
			if (Fx.ExhaustTone >= 0)
			{
				Sound.ExhaustTone = Fx.ExhaustTone;
			}
		}
		if (Engine.bAntiLag)
		{
			Sound.Pops += 0.5;
		}
		Sound.Pops = Clamp(Sound.Pops, 0.0, 1.5);
		return Setup;
	}

	int PerformanceRating(const VehicleSetup& Setup)
	{
		const double PowerToWeight = (Setup.Engine.PeakPowerKw + 0.5 * Setup.NitroKw) / (Setup.MassKg / 1000.0);
		const double Grip = 0.5 * (Setup.FrontTire.Mu + Setup.RearTire.Mu) * Setup.ChassisGrip;
		const double Downforce = Setup.Aero.DownforceFront + Setup.Aero.DownforceRear;
		const double Rating = 55.0 * std::pow(std::max(1.0, PowerToWeight), 0.4) * std::pow(Grip, 0.6) * (1.0 + 0.25 * std::min(1.0, Downforce));
		return static_cast<int>(std::lround(Rating));
	}
}
