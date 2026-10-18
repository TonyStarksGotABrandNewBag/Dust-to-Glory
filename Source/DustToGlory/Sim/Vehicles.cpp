#include "Sim/Vehicles.h"

#include <cstdio>

namespace dtg::sim
{
	namespace detail_vehicles
	{
		struct BodyInfo
		{
			BodyStyle Body;
			const char* Name;
			double BasePowerKw;
			double BaseMassKg;
			double BasePriceUsd;
			double Cd;
			double FrontalArea;
			double Length;
			double Width;
			double Height;
			int Seats;
			double Grip;
			double WheelRadius;
			BodyDefaults Defaults; // suspension Hz, ride height, steer lock, top-speed cap, CG height
		};

		// Baseline figures for a mid-tier model of each body style in 2010.
		const BodyInfo Bodies[] = {
			{BodyStyle::Sedan, "Sedan", 110, 1450, 30000, 0.30, 2.20, 4.80, 1.84, 1.45, 5, 1.00, 0.32, {1.6, 0.33, 36, 260, 0.52}},
			{BodyStyle::Coupe, "Coupe", 125, 1380, 36000, 0.31, 2.05, 4.60, 1.80, 1.36, 4, 1.03, 0.32, {1.7, 0.31, 35, 270, 0.48}},
			{BodyStyle::Hatchback, "Hatchback", 80, 1150, 23000, 0.31, 2.10, 4.15, 1.76, 1.47, 5, 1.00, 0.30, {1.7, 0.32, 37, 230, 0.52}},
			{BodyStyle::Wagon, "Wagon", 105, 1550, 33000, 0.31, 2.25, 4.85, 1.84, 1.50, 5, 1.00, 0.32, {1.5, 0.34, 36, 250, 0.54}},
			{BodyStyle::Convertible, "Convertible", 130, 1500, 45000, 0.34, 2.10, 4.55, 1.82, 1.34, 4, 1.02, 0.32, {1.7, 0.31, 35, 260, 0.48}},
			{BodyStyle::SUV, "SUV", 150, 2050, 42000, 0.36, 2.80, 4.90, 1.95, 1.75, 7, 0.95, 0.38, {1.4, 0.45, 37, 240, 0.68}},
			{BodyStyle::Pickup, "Pickup", 170, 2150, 48000, 0.42, 3.00, 5.80, 2.03, 1.90, 5, 0.90, 0.40, {1.3, 0.48, 38, 200, 0.72}},
			{BodyStyle::Van, "Van", 110, 1950, 40000, 0.38, 3.20, 5.20, 2.00, 2.00, 8, 0.90, 0.35, {1.3, 0.40, 38, 190, 0.75}},
			{BodyStyle::Muscle, "Muscle", 230, 1650, 50000, 0.36, 2.15, 4.85, 1.90, 1.36, 4, 1.00, 0.34, {1.5, 0.32, 34, 280, 0.50}},
			{BodyStyle::Sports, "Sports", 220, 1380, 70000, 0.31, 1.95, 4.40, 1.85, 1.26, 2, 1.10, 0.33, {2.0, 0.28, 34, 310, 0.45}},
			{BodyStyle::Supercar, "Supercar", 480, 1480, 320000, 0.33, 1.85, 4.55, 2.00, 1.18, 2, 1.20, 0.34, {2.4, 0.26, 32, 380, 0.42}},
			{BodyStyle::Luxury, "Luxury", 240, 1950, 110000, 0.28, 2.30, 5.20, 1.92, 1.48, 5, 1.02, 0.35, {1.3, 0.35, 36, 290, 0.54}},
			{BodyStyle::Truck, "Truck", 260, 3600, 80000, 0.55, 5.50, 7.20, 2.45, 2.90, 2, 0.80, 0.50, {1.3, 0.55, 38, 170, 1.10}},
			{BodyStyle::OpenWheel, "Open-Wheel", 750, 800, 4000000, 0.90, 1.30, 5.60, 2.00, 0.95, 1, 1.60, 0.33, {3.5, 0.20, 24, 360, 0.30}},
			{BodyStyle::MonsterTruck, "Monster Truck", 1100, 5400, 300000, 0.80, 5.00, 5.80, 3.70, 3.60, 1, 0.90, 0.84, {0.9, 1.40, 36, 115, 1.60}},
			{BodyStyle::Dragster, "Dragster", 2400, 1150, 250000, 0.60, 1.20, 7.50, 1.40, 1.10, 1, 1.00, 0.45, {2.2, 0.15, 10, 420, 0.40}},
			{BodyStyle::Bus, "Bus", 220, 11500, 400000, 0.60, 7.50, 12.00, 2.55, 3.20, 40, 0.75, 0.52, {1.2, 0.40, 42, 120, 1.20}},
		};

		const BodyInfo& BodyData(BodyStyle Body)
		{
			for (const BodyInfo& Entry : Bodies)
			{
				if (Entry.Body == Body)
				{
					return Entry;
				}
			}
			return Bodies[0];
		}

		struct YearPoint
		{
			double Year;
			double Value;
		};

		template <std::size_t N>
		double Interpolate(const YearPoint (&Points)[N], double Year)
		{
			if (Year <= Points[0].Year)
			{
				return Points[0].Value;
			}
			for (std::size_t Index = 1; Index < N; ++Index)
			{
				if (Year <= Points[Index].Year)
				{
					const double T = (Year - Points[Index - 1].Year) / (Points[Index].Year - Points[Index - 1].Year);
					return Lerp(Points[Index - 1].Value, Points[Index].Value, T);
				}
			}
			return Points[N - 1].Value;
		}

		// Relative power across eras: the 60s muscle peak, the 70s/80s emissions slump, the modern turbo era.
		constexpr YearPoint PowerByYear[] = {
			{1900, 0.12}, {1930, 0.28}, {1950, 0.45}, {1965, 0.72}, {1971, 0.80}, {1980, 0.55},
			{1990, 0.72}, {2000, 0.88}, {2010, 1.00}, {2020, 1.15}, {2030, 1.25}};

		// Naturally aspirated specific output in kW per litre.
		constexpr YearPoint OutputPerLitre[] = {
			{1900, 8}, {1950, 22}, {1970, 30}, {1980, 32}, {1990, 45}, {2000, 55}, {2010, 62}, {2020, 68}};

		constexpr YearPoint TireGripByYear[] = {{1900, 0.70}, {1950, 0.80}, {1975, 0.90}, {2000, 1.00}, {2020, 1.05}};

		constexpr YearPoint DragByYear[] = {{1900, 1.45}, {1950, 1.45}, {1975, 1.30}, {1990, 1.10}, {2010, 1.00}, {2020, 0.92}};

		double EraPower(int Year) { return Interpolate(PowerByYear, Year); }
		double EraOutput(int Year) { return Interpolate(OutputPerLitre, Year); }
		double EraGrip(int Year) { return Interpolate(TireGripByYear, Year); }
		double EraDrag(int Year) { return Interpolate(DragByYear, Year); }

		double EraMass(int Year, bool bElectric)
		{
			double Factor = 1.0;
			if (Year < 1976)
			{
				Factor = 1.08;
			}
			else if (Year < 1995)
			{
				Factor = 0.92;
			}
			else
			{
				Factor = 0.95 + 0.004 * (Year - 1995);
			}
			return Factor * (bElectric ? 1.2 : 1.0);
		}

		bool IsRaceMachine(BodyStyle Body)
		{
			return Body == BodyStyle::OpenWheel || Body == BodyStyle::MonsterTruck || Body == BodyStyle::Dragster;
		}

		bool IsCollectible(BodyStyle Body)
		{
			return Body == BodyStyle::Muscle || Body == BodyStyle::Sports || Body == BodyStyle::Supercar ||
				Body == BodyStyle::Convertible || Body == BodyStyle::Coupe;
		}

		bool IsPerformanceBody(BodyStyle Body)
		{
			return Body == BodyStyle::Sports || Body == BodyStyle::Supercar || Body == BodyStyle::Coupe || Body == BodyStyle::Convertible;
		}

		struct LayoutInfo
		{
			EngineLayout Layout;
			const char* Name;
			const char* Label;
			int Cylinders;
			double MinLitres;
			double MaxLitres;
			double PowerRpm;
		};

		const LayoutInfo Layouts[] = {
			{EngineLayout::I3, "I3", "I3", 3, 0.8, 1.6, 5800},
			{EngineLayout::I4, "I4", "I4", 4, 1.0, 2.7, 6200},
			{EngineLayout::I5, "I5", "I5", 5, 2.0, 3.2, 6000},
			{EngineLayout::I6, "I6", "I6", 6, 2.0, 4.5, 6200},
			{EngineLayout::V6, "V6", "V6", 6, 2.5, 4.3, 6200},
			{EngineLayout::V8Crossplane, "V8Crossplane", "V8", 8, 3.8, 7.5, 5600},
			{EngineLayout::V8Flatplane, "V8Flatplane", "flat-plane V8", 8, 3.0, 5.5, 7800},
			{EngineLayout::V10, "V10", "V10", 10, 3.0, 8.4, 8000},
			{EngineLayout::V12, "V12", "V12", 12, 3.5, 7.0, 7200},
			{EngineLayout::Flat4, "Flat4", "flat-4", 4, 1.5, 2.6, 6000},
			{EngineLayout::Flat6, "Flat6", "flat-6", 6, 2.0, 4.2, 7200},
			{EngineLayout::Rotary, "Rotary", "twin-rotor rotary", 2, 1.1, 2.0, 8200},
			{EngineLayout::DieselI6, "DieselI6", "turbo-diesel I6", 6, 6.0, 15.0, 2200},
			{EngineLayout::Electric, "Electric", "electric", 0, 0.0, 0.0, 7000},
		};

		const LayoutInfo& LayoutData(EngineLayout Value)
		{
			for (const LayoutInfo& Entry : Layouts)
			{
				if (Entry.Layout == Value)
				{
					return Entry;
				}
			}
			return Layouts[1];
		}

		std::string NormalizeKey(const std::string& Value)
		{
			std::string Out;
			for (const char Character : ToLowerAscii(Value))
			{
				if ((Character >= 'a' && Character <= 'z') || (Character >= '0' && Character <= '9'))
				{
					Out.push_back(Character);
				}
			}
			return Out;
		}

		struct EngineChoice
		{
			EngineLayout Layout = EngineLayout::I4;
			Aspiration Induction = Aspiration::Natural;
			double Litres = 2.0;
			double PowerRpm = 6000.0;
			double TorqueRpm = 4000.0;
			double Redline = 6800.0;
		};

		double ForcedInductionGain(Aspiration Induction)
		{
			switch (Induction)
			{
			case Aspiration::Turbo: return 1.45;
			case Aspiration::TwinTurbo: return 1.55;
			case Aspiration::Supercharged: return 1.30;
			default: return 1.0;
			}
		}

		EngineChoice ChooseEngine(const Brand& Maker, const Nameplate& Line, int Year, double PowerKw, bool bElectric, Rng& Random)
		{
			EngineChoice Choice;
			if (bElectric)
			{
				Choice.Layout = EngineLayout::Electric;
				Choice.Induction = Aspiration::Electric;
				Choice.Litres = 0.0;
				Choice.PowerRpm = 7000.0;
				Choice.TorqueRpm = 0.0;
				Choice.Redline = 15000.0;
				return Choice;
			}

			switch (Line.Body)
			{
			case BodyStyle::MonsterTruck:
				Choice = {EngineLayout::V8Crossplane, Aspiration::Supercharged, 8.8, 7000.0, 5200.0, 7600.0};
				return Choice;
			case BodyStyle::Dragster:
				Choice = {EngineLayout::V8Crossplane, Aspiration::Supercharged, 8.2, 7500.0, 6000.0, 8200.0};
				return Choice;
			case BodyStyle::OpenWheel:
				if (Year >= 2014) Choice = {EngineLayout::V6, Aspiration::Turbo, 1.6, 11000.0, 10500.0, 15000.0};
				else if (Year >= 2006) Choice = {EngineLayout::V8Flatplane, Aspiration::Natural, 2.4, 17500.0, 14500.0, 18000.0};
				else if (Year >= 1995) Choice = {EngineLayout::V10, Aspiration::Natural, 3.0, 17000.0, 14000.0, 18500.0};
				else if (Year >= 1989) Choice = {EngineLayout::V12, Aspiration::Natural, 3.5, 13000.0, 10500.0, 14000.0};
				else Choice = {EngineLayout::V8Flatplane, Aspiration::Natural, 3.0, 10500.0, 8500.0, 11000.0};
				return Choice;
			case BodyStyle::Truck:
			case BodyStyle::Bus:
				Choice = {EngineLayout::DieselI6, Aspiration::Turbo, Line.Body == BodyStyle::Bus ? 9.0 : 12.0, 1900.0, 1300.0, 2600.0};
				return Choice;
			default:
				break;
			}

			// Forced induction follows the industry's adoption curve unless the data says otherwise.
			Aspiration Induction = Aspiration::Natural;
			if (!Line.AspirationHint.empty())
			{
				ParseAspiration(Line.AspirationHint, Induction);
			}
			else
			{
				const bool bEveryday = Line.Body == BodyStyle::Sedan || Line.Body == BodyStyle::Hatchback || Line.Body == BodyStyle::Wagon ||
					Line.Body == BodyStyle::SUV || Line.Body == BodyStyle::Luxury || Line.Body == BodyStyle::Coupe || Line.Body == BodyStyle::Convertible;
				if (Year >= 2012 && bEveryday && Random.Chance(0.75))
				{
					Induction = Aspiration::Turbo;
				}
				else if (Year >= 2015 && Line.Body == BodyStyle::Sports && Random.Chance(0.5))
				{
					Induction = Aspiration::Turbo;
				}
				else if (Year >= 2015 && Line.Body == BodyStyle::Supercar && Random.Chance(0.6))
				{
					Induction = Aspiration::TwinTurbo;
				}
				else if (Year >= 1980 && Year < 2012 && (Line.Body == BodyStyle::Sports || Line.Body == BodyStyle::Hatchback || Line.Body == BodyStyle::Coupe) && Random.Chance(0.15))
				{
					Induction = Aspiration::Turbo;
				}
				else if (Year >= 1985 && Line.Body == BodyStyle::Supercar && Random.Chance(0.25))
				{
					Induction = Aspiration::TwinTurbo;
				}
				else if (Year >= 2005 && Line.Body == BodyStyle::Muscle && Line.Tier > 0.6 && Random.Chance(0.4))
				{
					Induction = Aspiration::Supercharged;
				}
				else if (Year >= 2015 && Line.Body == BodyStyle::Pickup && Random.Chance(0.4))
				{
					Induction = Aspiration::TwinTurbo;
				}
			}

			double Litres = PowerKw / (EraOutput(Year) * ForcedInductionGain(Induction));
			EngineLayout Chosen = EngineLayout::I4;
			bool bHaveLayout = false;
			if (!Line.EngineHint.empty())
			{
				bHaveLayout = ParseEngineLayout(Line.EngineHint, Chosen);
			}
			if (!bHaveLayout && !Maker.SignatureEngine.empty() && (IsPerformanceBody(Line.Body) || Line.Body == BodyStyle::Sedan ||
				Line.Body == BodyStyle::Hatchback || Line.Body == BodyStyle::Wagon || Line.Body == BodyStyle::Luxury))
			{
				EngineLayout Signature = EngineLayout::I4;
				if (ParseEngineLayout(Maker.SignatureEngine, Signature))
				{
					const LayoutInfo& SignatureInfo = LayoutData(Signature);
					// Only use the signature engine where its size range can plausibly make the power.
					// A rotor fires every shaft revolution, so a rotary makes about twice the power per litre of a piston engine.
					const double RotaryGain = Signature == EngineLayout::Rotary ? 2.0 : 1.0;
					const double MaxPower = SignatureInfo.MaxLitres * EraOutput(Year) * 1.6 * RotaryGain;
					if (PowerKw <= MaxPower)
					{
						Chosen = Signature;
						bHaveLayout = true;
					}
				}
			}
			if (!bHaveLayout)
			{
				if (Litres < 1.3) Chosen = EngineLayout::I3;
				else if (Litres < 2.4) Chosen = EngineLayout::I4;
				else if (Litres < 2.8) Chosen = Random.Chance(0.2) ? EngineLayout::I5 : EngineLayout::I4;
				else if (Litres < 3.9) Chosen = EngineLayout::V6;
				else
				{
					// Big displacement: exotic makers build V8 flat-planes, V10s and V12s; everyone else builds big V8s.
					const bool bExotic = (Line.Body == BodyStyle::Supercar || Line.Body == BodyStyle::Sports || Line.Body == BodyStyle::Luxury) &&
						Maker.Prestige > 0.55;
					if (!bExotic)
					{
						Chosen = EngineLayout::V8Crossplane;
					}
					else if (Litres < 5.5)
					{
						Chosen = Line.Body == BodyStyle::Luxury ? EngineLayout::V8Crossplane : EngineLayout::V8Flatplane;
					}
					else
					{
						Chosen = (Maker.Prestige > 0.8 || Line.Body == BodyStyle::Luxury) ? EngineLayout::V12 : EngineLayout::V10;
					}
				}
			}

			const LayoutInfo& ChosenInfo = LayoutData(Chosen);
			if (Chosen == EngineLayout::Rotary)
			{
				Litres = PowerKw > 190.0 ? 2.0 : 1.3; // twin- or triple-rotor
			}
			Litres = Clamp(Litres, ChosenInfo.MinLitres, ChosenInfo.MaxLitres);
			// If the block is too small to make the power naturally, it needs a turbo.
			const double SpecificLimit = EraOutput(Year) * (Chosen == EngineLayout::Rotary ? 2.6 : 1.3);
			if (Induction == Aspiration::Natural && Year >= 1975 && PowerKw / Litres > SpecificLimit)
			{
				Induction = PowerKw > 300.0 ? Aspiration::TwinTurbo : Aspiration::Turbo;
			}

			const double EraRpm = 0.8 + 0.2 * Clamp((Year - 1950) / 60.0, 0.0, 1.0);
			double PowerRpm = ChosenInfo.PowerRpm * EraRpm;
			if (Line.Body == BodyStyle::Sports || Line.Body == BodyStyle::Supercar)
			{
				PowerRpm *= 1.08;
			}
			if (Induction == Aspiration::Turbo || Induction == Aspiration::TwinTurbo)
			{
				PowerRpm *= 0.92;
			}

			double TorqueRpm = 0.68 * PowerRpm;
			if (Chosen == EngineLayout::V8Crossplane) TorqueRpm = 0.6 * PowerRpm;
			if (Chosen == EngineLayout::V8Flatplane || Chosen == EngineLayout::Flat6 || Chosen == EngineLayout::V10) TorqueRpm = 0.75 * PowerRpm;
			if (Chosen == EngineLayout::Rotary) TorqueRpm = 0.8 * PowerRpm;
			if (Induction == Aspiration::Turbo || Induction == Aspiration::TwinTurbo) TorqueRpm = std::max(1800.0, 0.4 * PowerRpm);
			if (Induction == Aspiration::Supercharged) TorqueRpm = 0.55 * PowerRpm;

			double Redline = PowerRpm * (Induction == Aspiration::Natural ? 1.1 : 1.08);
			if (Chosen == EngineLayout::V8Flatplane || Chosen == EngineLayout::V10 || Chosen == EngineLayout::Rotary || Chosen == EngineLayout::Flat6)
			{
				Redline = PowerRpm * 1.12;
			}

			Choice.Layout = Chosen;
			Choice.Induction = Induction;
			Choice.Litres = std::round(Litres * 10.0) / 10.0;
			Choice.PowerRpm = std::round(PowerRpm / 50.0) * 50.0;
			Choice.TorqueRpm = std::round(TorqueRpm / 50.0) * 50.0;
			Choice.Redline = std::round(Redline / 100.0) * 100.0;
			return Choice;
		}

		std::string MakeEngineLabel(const EngineChoice& Choice, Drivetrain Drive)
		{
			if (Choice.Layout == EngineLayout::Electric)
			{
				return Drive == Drivetrain::AWD ? "Dual-motor electric" : "Single-motor electric";
			}
			const char* Prefix = "";
			switch (Choice.Induction)
			{
			case Aspiration::Turbo: Prefix = Choice.Layout == EngineLayout::DieselI6 ? "" : "turbo "; break;
			case Aspiration::TwinTurbo: Prefix = "twin-turbo "; break;
			case Aspiration::Supercharged: Prefix = "supercharged "; break;
			default: break;
			}
			char Buffer[96];
			std::snprintf(Buffer, sizeof(Buffer), "%.1fL %s%s", Choice.Litres, Prefix, LayoutData(Choice.Layout).Label);
			return Buffer;
		}

		double TorqueFactor(const EngineChoice& Choice)
		{
			switch (Choice.Induction)
			{
			case Aspiration::Turbo:
			case Aspiration::TwinTurbo: return Choice.Layout == EngineLayout::DieselI6 ? 1.6 : 1.35;
			case Aspiration::Supercharged: return 1.25;
			case Aspiration::Electric: return 1.0;
			default: break;
			}
			if (Choice.Layout == EngineLayout::Rotary) return 1.08;
			if (Choice.Layout == EngineLayout::V8Crossplane) return 1.3;
			return 1.18;
		}

		int GearCount(BodyStyle Body, int Year, bool bElectric)
		{
			if (bElectric) return 1;
			switch (Body)
			{
			case BodyStyle::Dragster: return 3;
			case BodyStyle::MonsterTruck: return 3;
			case BodyStyle::OpenWheel: return Year < 1990 ? 5 : (Year < 2014 ? 7 : 8);
			case BodyStyle::Bus: return 6;
			case BodyStyle::Truck: return 10;
			default: break;
			}
			if (Year < 1960) return 3;
			if (Year < 1975) return 4;
			if (Year < 1995) return 5;
			if (Year < 2010) return 6;
			if (Body == BodyStyle::Sports || Body == BodyStyle::Supercar) return 7;
			return Year < 2018 ? 6 : 8;
		}

		double WeightFrontFor(EnginePosition Position, Drivetrain Drive, BodyStyle Body, bool bElectric)
		{
			if (Body == BodyStyle::Bus) return 0.35;
			if (Body == BodyStyle::Truck) return 0.55;
			if (Body == BodyStyle::MonsterTruck) return 0.50;
			if (Body == BodyStyle::Dragster) return 0.40;
			if (bElectric) return 0.48;
			switch (Position)
			{
			case EnginePosition::Mid: return 0.42;
			case EnginePosition::Rear: return 0.38;
			case EnginePosition::Front: break;
			}
			if (Drive == Drivetrain::FWD) return 0.61;
			if (Drive == Drivetrain::AWD) return 0.57;
			return Body == BodyStyle::Muscle ? 0.56 : 0.53;
		}
	}

	const char* BodyStyleName(BodyStyle Value)
	{
		return detail_vehicles::BodyData(Value).Name;
	}

	bool ParseBodyStyle(const std::string& Text, BodyStyle& Out)
	{
		const std::string Wanted = detail_vehicles::NormalizeKey(Text);
		for (const auto& Entry : detail_vehicles::Bodies)
		{
			if (detail_vehicles::NormalizeKey(Entry.Name) == Wanted)
			{
				Out = Entry.Body;
				return true;
			}
		}
		return false;
	}

	const char* DrivetrainName(Drivetrain Value)
	{
		switch (Value)
		{
		case Drivetrain::FWD: return "FWD";
		case Drivetrain::RWD: return "RWD";
		case Drivetrain::AWD: return "AWD";
		}
		return "RWD";
	}

	bool ParseDrivetrain(const std::string& Text, Drivetrain& Out)
	{
		const std::string Key = detail_vehicles::NormalizeKey(Text);
		if (Key == "fwd" || Key == "ff") { Out = Drivetrain::FWD; return true; }
		if (Key == "rwd" || Key == "fr" || Key == "mr" || Key == "rr") { Out = Drivetrain::RWD; return true; }
		if (Key == "awd" || Key == "4wd") { Out = Drivetrain::AWD; return true; }
		return false;
	}

	const char* EnginePositionName(EnginePosition Value)
	{
		switch (Value)
		{
		case EnginePosition::Front: return "Front";
		case EnginePosition::Mid: return "Mid";
		case EnginePosition::Rear: return "Rear";
		}
		return "Front";
	}

	const char* EngineLayoutName(EngineLayout Value)
	{
		return detail_vehicles::LayoutData(Value).Name;
	}

	bool ParseEngineLayout(const std::string& Text, EngineLayout& Out)
	{
		const std::string Key = detail_vehicles::NormalizeKey(Text);
		for (const auto& Entry : detail_vehicles::Layouts)
		{
			if (detail_vehicles::NormalizeKey(Entry.Name) == Key)
			{
				Out = Entry.Layout;
				return true;
			}
		}
		if (Key == "v8") { Out = EngineLayout::V8Crossplane; return true; }
		if (Key == "boxer4") { Out = EngineLayout::Flat4; return true; }
		if (Key == "boxer6") { Out = EngineLayout::Flat6; return true; }
		if (Key == "ev") { Out = EngineLayout::Electric; return true; }
		return false;
	}

	const char* AspirationName(Aspiration Value)
	{
		switch (Value)
		{
		case Aspiration::Natural: return "Natural";
		case Aspiration::Turbo: return "Turbo";
		case Aspiration::TwinTurbo: return "TwinTurbo";
		case Aspiration::Supercharged: return "Supercharged";
		case Aspiration::Electric: return "Electric";
		case Aspiration::Count: break;
		}
		return "Natural";
	}

	bool ParseAspiration(const std::string& Text, Aspiration& Out)
	{
		const std::string Key = detail_vehicles::NormalizeKey(Text);
		for (int Index = 0; Index < static_cast<int>(Aspiration::Count); ++Index)
		{
			const Aspiration Value = static_cast<Aspiration>(Index);
			if (detail_vehicles::NormalizeKey(AspirationName(Value)) == Key)
			{
				Out = Value;
				return true;
			}
		}
		if (Key == "na") { Out = Aspiration::Natural; return true; }
		return false;
	}

	int CylinderCount(EngineLayout Value)
	{
		return detail_vehicles::LayoutData(Value).Cylinders;
	}

	const BodyDefaults& GetBodyDefaults(BodyStyle Body)
	{
		return detail_vehicles::BodyData(Body).Defaults;
	}

	void VehicleCatalog::AddBrand(Brand Value)
	{
		BrandList.push_back(std::move(Value));
	}

	void VehicleCatalog::AddNameplate(Nameplate Value)
	{
		NameplateList.push_back(std::move(Value));
	}

	std::vector<double> StockGearRatios(const VehicleModel& Model)
	{
		std::vector<double> Ratios;
		const int Gears = std::max(1, Model.Gears);
		if (Gears == 1)
		{
			Ratios.push_back(1.0);
			return Ratios;
		}
		// Typical first/top ratios by gearbox size, from old 3-speeds to modern 8-speed automatics.
		double First = 3.6;
		double Top = 0.75;
		switch (Gears)
		{
		case 2: First = 1.8; Top = 1.0; break;
		case 3: First = 2.5; Top = 1.0; break;
		case 4: First = 2.7; Top = 0.9; break;
		case 5: First = 3.3; Top = 0.8; break;
		case 6: First = 3.6; Top = 0.75; break;
		case 7: First = 3.8; Top = 0.7; break;
		default: First = 4.7; Top = 0.67; break;
		}
		if (Model.Layout == EngineLayout::DieselI6) { First = Model.Body == BodyStyle::Truck ? 12.0 : 5.0; Top = 0.75; }
		if (Model.Body == BodyStyle::Dragster) { First = 2.0; Top = 1.0; }
		if (Model.Body == BodyStyle::MonsterTruck) { First = 2.5; Top = 1.0; }
		if (Model.Body == BodyStyle::OpenWheel) { First = 3.2; Top = 0.68; }
		for (int Index = 0; Index < Gears; ++Index)
		{
			const double T = static_cast<double>(Index) / static_cast<double>(Gears - 1);
			// Slightly progressive spacing: lower gears further apart, like real gearboxes.
			const double Shaped = std::pow(T, 0.85);
			Ratios.push_back(std::round(First * std::pow(Top / First, Shaped) * 1000.0) / 1000.0);
		}
		return Ratios;
	}

	double StockFinalDrive(const VehicleModel& Model, const std::vector<double>& Ratios)
	{
		// Modern gearboxes reach top speed a gear or two before the last; the top gears are overdrives for cruising.
		std::size_t SpeedGear = Ratios.empty() ? 0 : Ratios.size() - 1;
		if (Ratios.size() >= 7)
		{
			SpeedGear = Ratios.size() - 3;
		}
		else if (Ratios.size() == 6)
		{
			SpeedGear = Ratios.size() - 2;
		}
		const double TopRatio = Ratios.empty() ? 1.0 : Ratios[SpeedGear];
		const double TopSpeedMs = std::max(10.0, Model.TopSpeedKph / 3.6);
		const double RedlineOmega = Model.RedlineRpm * 2.0 * 3.14159265358979 / 60.0;
		return std::round(RedlineOmega * Model.WheelRadiusM / (TopRatio * TopSpeedMs * 1.04) * 100.0) / 100.0;
	}

	VehicleModel VehicleCatalog::MakeGeneration(const Brand& Maker, const Nameplate& Line, int Generation, int YearStart, int YearEnd, int CurrentYearValue,
		double PriceIndex)
	{
		using namespace detail_vehicles;
		const BodyInfo& Body = BodyData(Line.Body);
		const std::uint64_t Seed = HashString(Maker.Id + "|" + Line.Name + "|" + std::to_string(Generation));
		Rng Random(Seed);

		VehicleModel Model;
		Model.BrandId = Maker.Id;
		Model.Nameplate = Line.Name;
		Model.Generation = Generation;
		Model.YearStart = YearStart;
		Model.YearEnd = YearEnd;
		Model.Body = Line.Body;
		Model.Drive = Line.Drive;
		Model.bEventOnly = Line.bEventOnly || IsRaceMachine(Line.Body);
		Model.bElectric = Line.ElectricFrom > 0 && YearStart >= Line.ElectricFrom;
		// Year in the id keeps revived nameplates (same name, new era) distinct.
		Model.Id = Slugify(Maker.Id + " " + Line.Name + " " + std::to_string(YearStart));

		char NameBuffer[200];
		const std::string EndText = YearEnd >= CurrentYearValue ? std::string("present") : std::to_string(YearEnd);
		std::snprintf(NameBuffer, sizeof(NameBuffer), "%s %s (%d-%s)", Maker.Name.c_str(), Line.Name.c_str(), YearStart, EndText.c_str());
		Model.DisplayName = NameBuffer;

		const int MidYear = (YearStart + YearEnd) / 2;
		const double Jitter = Random.Range(0.92, 1.08);
		// Race machinery is purpose-built, so era swings are muted.
		const double Era = IsRaceMachine(Line.Body) ? std::sqrt(EraPower(MidYear)) : EraPower(MidYear);
		double Power = Body.BasePowerKw * Era * (0.75 + 0.6 * Line.Tier) * (1.0 + 0.25 * Maker.PerformanceBias) * Jitter;
		if (Model.bElectric)
		{
			Power *= 1.15;
		}
		Model.PowerKw = std::round(Power);

		// Engine
		const EngineChoice Engine = ChooseEngine(Maker, Line, MidYear, Model.PowerKw, Model.bElectric, Random);
		Model.Layout = Engine.Layout;
		Model.Induction = Engine.Induction;
		Model.DisplacementL = Engine.Litres;
		Model.PowerRpm = Engine.PowerRpm;
		Model.TorqueRpm = Engine.TorqueRpm;
		Model.RedlineRpm = Engine.Redline;
		Model.EngineLabel = MakeEngineLabel(Engine, Line.Drive);
		if (Model.bElectric)
		{
			// Electric motors make their peak torque from standstill up to the base speed (~40% of max-power rpm).
			Model.TorqueNm = std::round(Model.PowerKw * 1000.0 / (0.4 * Engine.PowerRpm * 2.0 * 3.14159265358979 / 60.0));
		}
		else
		{
			const double PowerOmega = Engine.PowerRpm * 2.0 * 3.14159265358979 / 60.0;
			Model.TorqueNm = std::round(Model.PowerKw * 1000.0 / PowerOmega * TorqueFactor(Engine));
		}

		// Position
		Model.Position = EnginePosition::Front;
		if (!Line.PositionHint.empty())
		{
			const std::string Hint = NormalizeKey(Line.PositionHint);
			if (Hint == "mid") Model.Position = EnginePosition::Mid;
			if (Hint == "rear") Model.Position = EnginePosition::Rear;
		}
		else if (Line.Body == BodyStyle::Supercar || Line.Body == BodyStyle::OpenWheel)
		{
			Model.Position = EnginePosition::Mid;
		}
		else if (Line.Body == BodyStyle::Bus)
		{
			Model.Position = EnginePosition::Rear;
		}
		else if (Engine.Layout == EngineLayout::Flat6 && Line.Body == BodyStyle::Sports)
		{
			Model.Position = EnginePosition::Rear;
		}

		// Chassis
		Model.MassKg = std::round(Body.BaseMassKg * EraMass(MidYear, Model.bElectric) * (0.9 + 0.2 * Line.Tier) * Random.Range(0.96, 1.04));
		Model.WeightFront = WeightFrontFor(Model.Position, Line.Drive, Line.Body, Model.bElectric);
		Model.DragCoefficient = std::round(Body.Cd * EraDrag(MidYear) * 1000.0) / 1000.0;
		Model.FrontalAreaM2 = Body.FrontalArea;
		Model.LengthM = std::round(Body.Length * Random.Range(0.97, 1.03) * 100.0) / 100.0;
		Model.WidthM = Body.Width;
		Model.HeightM = Body.Height;
		Model.Seats = Body.Seats;
		double Grip = Body.Grip * EraGrip(MidYear);
		if ((IsPerformanceBody(Line.Body) || Line.Body == BodyStyle::Muscle) && MidYear >= 1985)
		{
			Grip *= 1.0 + 0.12 * Line.Tier; // performance trims ship on stickier, wider tires
		}
		Model.TireGrip = std::round(Grip * 100.0) / 100.0;
		Model.WheelRadiusM = Body.WheelRadius;
		Model.Gears = GearCount(Line.Body, MidYear, Model.bElectric);

		const double DragArea = Model.DragCoefficient * Model.FrontalAreaM2;
		double TopKph = std::cbrt(2.0 * 0.88 * Model.PowerKw * 1000.0 / (1.225 * DragArea)) * 3.6;
		TopKph = std::min(TopKph, Body.Defaults.TopSpeedCapKph);
		if (MidYear < 1960)
		{
			TopKph = std::min(TopKph, 185.0);
		}
		if (Model.bElectric && Line.Body != BodyStyle::Supercar)
		{
			TopKph = std::min(TopKph, 250.0);
		}
		Model.TopSpeedKph = std::round(TopKph);

		// Spec-sheet 0-100: the slower of the power limit and the traction limit.
		const double V100 = 27.78;
		const double PowerLimited = 0.9 + Model.MassKg * V100 * V100 / (2.0 * 0.58 * Model.PowerKw * 1000.0);
		// Share of the car's weight on the driven tires during a launch (static share plus weight transfer).
		double DriveShare = std::max(0.4, Model.WeightFront - 0.08);
		if (Line.Drive == Drivetrain::RWD) DriveShare = std::min(0.95, 1.0 - Model.WeightFront + 0.12);
		if (Line.Drive == Drivetrain::AWD) DriveShare = 1.0;
		const double TractionLimited = V100 / (Model.TireGrip * 9.81 * DriveShare) + 0.3;
		Model.ZeroTo100S = std::round(std::max(PowerLimited, TractionLimited) * 10.0) / 10.0;

		const double Msrp = Body.BasePriceUsd * (0.7 + 0.9 * Line.Tier) * (1.0 + 0.8 * Maker.Prestige) * Random.Range(0.95, 1.05) * PriceIndex;
		Model.Msrp = ToMoney(std::round(Msrp / 100.0) * 100.0);

		// Market value: depreciation to a floor, then collector appreciation for desirable classics. Msrp is the
		// launch price in today's money, so a desirable classic ending up worth more than it is real appreciation.
		const int Age = std::max(0, CurrentYearValue - YearEnd);
		Model.AgeYears = Age;
		double Value = Msrp * std::max(std::pow(0.86, Age), 0.06);
		double Desire = 0.0;
		double RampStart = 20.0;
		double RampYears = 25.0;
		if (IsCollectible(Line.Body) || Line.Body == BodyStyle::Muscle)
		{
			Desire = (0.25 + 1.2 * Line.Tier) * (0.6 + 0.8 * Maker.Prestige) * (1.0 + 0.5 * std::max(0.0, Maker.PerformanceBias));
		}
		else if (Line.Body == BodyStyle::Pickup || Line.Body == BodyStyle::SUV || Line.Body == BodyStyle::Wagon || Line.Body == BodyStyle::Luxury)
		{
			// Vintage trucks, 4x4s, wood-panel wagons and land yachts have their own collectors.
			Desire = 0.5 * (0.25 + 1.2 * Line.Tier) * (0.6 + 0.8 * Maker.Prestige);
			RampStart = 25.0;
			RampYears = 30.0;
		}
		if (Age > 45)
		{
			Desire = std::max(Desire, 0.3 + Line.Tier); // anything this old is a survivor worth preserving
		}
		if (Maker.Country == "USA" && YearStart >= 1953 && YearStart <= 1972)
		{
			Desire *= 1.4; // golden-age Americana: tailfins, chrome and big V8s
		}
		if (YearEnd - YearStart <= 1 && Age > 40)
		{
			Desire *= 3.0; // built for a year or two: genuinely rare
		}
		if (Maker.Defunct > 0 && Age > 30)
		{
			Desire *= 1.2; // orphan makes have devoted followings
		}
		if (Desire > 0.0 && Age > RampStart)
		{
			const double Ramp = std::min(1.0, (Age - RampStart) / RampYears);
			Value += (Msrp * Desire - Value) * Ramp * (Msrp * Desire > Value ? 1.0 : 0.0);
		}
		Value = std::max(Value, 1500.0 * PriceIndex);
		Model.MarketPrice = ToMoney(std::round(Value / 50.0) * 50.0);
		return Model;
	}

	void VehicleCatalog::Build(int CurrentYearValue, double PriceIndex)
	{
		Year = CurrentYearValue;
		Prices = PriceIndex > 0.0 ? PriceIndex : 1.0;
		ModelList.clear();
		for (const Nameplate& Line : NameplateList)
		{
			const Brand* Maker = FindBrand(Line.BrandId);
			if (!Maker)
			{
				continue;
			}
			int Last = Line.LastYear > 0 ? Line.LastYear : CurrentYearValue;
			if (Maker->Defunct > 0)
			{
				Last = std::min(Last, Maker->Defunct);
			}
			Last = std::min(Last, CurrentYearValue);
			const int First = std::max(Line.FirstYear, Maker->Founded);
			if (First > Last)
			{
				continue;
			}
			if (!Line.GenerationStarts.empty())
			{
				// Known launch years: each generation runs until the next one starts.
				int Generation = 1;
				for (std::size_t Index = 0; Index < Line.GenerationStarts.size(); ++Index)
				{
					const int Start = std::max(First, Line.GenerationStarts[Index]);
					int Next = Index + 1 < Line.GenerationStarts.size() ? Line.GenerationStarts[Index + 1] - 1 : Last;
					if (Index < Line.GenerationEnds.size() && Line.GenerationEnds[Index] > 0)
					{
						Next = std::min(Next, Line.GenerationEnds[Index]);
					}
					const int End = std::min(Last, Next);
					if (Start > End)
					{
						continue;
					}
					ModelList.push_back(MakeGeneration(*Maker, Line, Generation, Start, End, CurrentYearValue, Prices));
					++Generation;
				}
				continue;
			}
			Rng Random(HashString(Line.BrandId + "/" + Line.Name + "/" + std::to_string(Line.FirstYear)));
			const int BaseLength = std::max(2, Line.GenerationYears);
			int Start = First;
			int Generation = 1;
			while (Start <= Last)
			{
				const int Length = std::max(2, BaseLength + Random.RangeInt(-1, 1));
				int End = std::min(Last, Start + Length - 1);
				// Avoid a stub final generation of one or two years: fold it into this one.
				if (Last - End <= 1)
				{
					End = Last;
				}
				ModelList.push_back(MakeGeneration(*Maker, Line, Generation, Start, End, CurrentYearValue, Prices));
				Start = End + 1;
				++Generation;
			}
		}
	}

	std::vector<const VehicleModel*> VehicleCatalog::ModelsForBrand(const std::string& BrandId, bool bIncludeEventOnly) const
	{
		std::vector<const VehicleModel*> Result;
		for (const VehicleModel& Model : ModelList)
		{
			if (Model.BrandId == BrandId && (bIncludeEventOnly || !Model.bEventOnly))
			{
				Result.push_back(&Model);
			}
		}
		return Result;
	}

	const VehicleModel* VehicleCatalog::FindModel(const std::string& Id) const
	{
		for (const VehicleModel& Model : ModelList)
		{
			if (Model.Id == Id)
			{
				return &Model;
			}
		}
		return nullptr;
	}

	const Brand* VehicleCatalog::FindBrand(const std::string& Id) const
	{
		for (const Brand& Value : BrandList)
		{
			if (Value.Id == Id)
			{
				return &Value;
			}
		}
		return nullptr;
	}

	const VehicleModel* VehicleCatalog::NewestOfBody(BodyStyle Body) const
	{
		const VehicleModel* Best = nullptr;
		for (const VehicleModel& Model : ModelList)
		{
			if (Model.Body == Body && (!Best || Model.YearEnd > Best->YearEnd))
			{
				Best = &Model;
			}
		}
		return Best;
	}
}
