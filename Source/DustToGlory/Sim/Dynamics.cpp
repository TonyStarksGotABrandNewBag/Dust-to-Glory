#include "Sim/Dynamics.h"

namespace dtg::sim
{
	namespace detail_dynamics
	{
		constexpr double Pi = 3.14159265358979;
		constexpr double Gravity = 9.81;
		constexpr double AirDensity = 1.225;
		constexpr double SubstepSeconds = 0.002;   // 500 Hz powertrain and wheel integration
		constexpr double SlipVelocityFloor = 3.0;  // m/s, keeps slip ratios sane near standstill
		constexpr double DiffStiffness = 150.0;    // N*m per rad/s of left/right speed difference

		// Default compound: longitudinal peak ~10% slip, lateral peak ~7.4 degrees, ~80-88% grip when sliding.
		constexpr double LongB = 20.0, LongC = 1.5, LongE = 0.3;
		constexpr double LatB = 16.0, LatC = 1.4, LatE = 0.0;

		double RpmToOmega(double Rpm) { return Rpm * 2.0 * Pi / 60.0; }
		double OmegaToRpm(double Omega) { return Omega * 60.0 / (2.0 * Pi); }

		double Sign(double Value) { return Value > 0.0 ? 1.0 : (Value < 0.0 ? -1.0 : 0.0); }

		/** Monotone cubic (Fritsch-Carlson) resampling of key points into a dense table. */
		void Pchip(const std::vector<double>& X, const std::vector<double>& Y, int Samples, std::vector<double>& OutX, std::vector<double>& OutY)
		{
			const std::size_t Count = X.size();
			OutX.clear();
			OutY.clear();
			if (Count < 2)
			{
				OutX = X;
				OutY = Y;
				return;
			}
			std::vector<double> H(Count - 1), Delta(Count - 1), D(Count);
			for (std::size_t K = 0; K + 1 < Count; ++K)
			{
				H[K] = X[K + 1] - X[K];
				Delta[K] = (Y[K + 1] - Y[K]) / H[K];
			}
			D[0] = Delta[0];
			D[Count - 1] = Delta[Count - 2];
			for (std::size_t K = 1; K + 1 < Count; ++K)
			{
				if (Delta[K - 1] * Delta[K] <= 0.0)
				{
					D[K] = 0.0;
				}
				else
				{
					const double W1 = 2.0 * H[K] + H[K - 1];
					const double W2 = H[K] + 2.0 * H[K - 1];
					D[K] = (W1 + W2) / (W1 / Delta[K - 1] + W2 / Delta[K]);
				}
			}
			const double Start = X.front();
			const double End = X.back();
			std::size_t Segment = 0;
			for (int Index = 0; Index < Samples; ++Index)
			{
				const double Rpm = Start + (End - Start) * Index / static_cast<double>(Samples - 1);
				while (Segment + 2 < Count && Rpm > X[Segment + 1])
				{
					++Segment;
				}
				const double T = Clamp((Rpm - X[Segment]) / H[Segment], 0.0, 1.0);
				const double T2 = T * T;
				const double T3 = T2 * T;
				const double H00 = 2.0 * T3 - 3.0 * T2 + 1.0;
				const double H10 = T3 - 2.0 * T2 + T;
				const double H01 = -2.0 * T3 + 3.0 * T2;
				const double H11 = T3 - T2;
				const double Value = H00 * Y[Segment] + H10 * H[Segment] * D[Segment] + H01 * Y[Segment + 1] + H11 * H[Segment] * D[Segment + 1];
				OutX.push_back(Rpm);
				OutY.push_back(std::max(0.0, Value));
			}
		}
	}

	double MagicFormula(double Slip, double B, double C, double E)
	{
		const double Bx = B * Slip;
		return std::sin(C * std::atan(Bx - E * (Bx - std::atan(Bx))));
	}

	TireForce ComputeTireForce(double Mu, double StiffnessMul, double SlipRatio, double SlipAngleRad, double LoadN,
		double NominalLoadN, double GripMulLong, double GripMulLat)
	{
		using namespace detail_dynamics;
		TireForce Result;
		if (LoadN <= 0.0)
		{
			return Result;
		}
		// Load sensitivity: grip per newton falls as the tire is pressed harder.
		const double LoadRatio = NominalLoadN > 1.0 ? LoadN / NominalLoadN : 1.0;
		const double Sensitivity = Clamp(1.0 - 0.1 * (LoadRatio - 1.0), 0.75, 1.15);
		const double PeakLong = Mu * Sensitivity * GripMulLong * LoadN;
		const double PeakLat = Mu * Sensitivity * GripMulLat * LoadN;

		// Combined slip via normalized slip magnitude (friction-ellipse behaviour).
		const double PeakRatio = TirePeakSlipRatio / std::max(0.5, StiffnessMul);
		const double PeakAngle = TirePeakSlipAngle / std::max(0.5, StiffnessMul);
		const double Sx = SlipRatio / PeakRatio;
		const double Sy = SlipAngleRad / PeakAngle;
		const double Rho = std::sqrt(Sx * Sx + Sy * Sy);
		if (Rho < 1e-9)
		{
			return Result;
		}
		const double LongCurve = MagicFormula(Rho * PeakRatio, LongB * StiffnessMul, LongC, LongE);
		const double LatCurve = MagicFormula(Rho * PeakAngle, LatB * StiffnessMul, LatC, LatE);
		Result.Fx = PeakLong * LongCurve * (Sx / Rho);
		Result.Fy = -PeakLat * LatCurve * (Sy / Rho);
		return Result;
	}

	TorqueCurve TorqueCurve::FromEngine(const EngineSetup& Engine)
	{
		using namespace detail_dynamics;
		TorqueCurve Curve;
		const double Redline = std::max(Engine.RedlineRpm, 1500.0);
		const double PowerRpm = Clamp(Engine.PowerRpm, 800.0, Redline);
		const double TPower = Engine.PeakPowerKw * 1000.0 / RpmToOmega(PowerRpm);
		const double TPeak = std::max(Engine.PeakTorqueNm, TPower);

		if (Engine.Layout == EngineLayout::Electric)
		{
			// Constant torque up to base speed, then constant power, then a cut at max rpm.
			const double BaseOmega = Engine.PeakPowerKw * 1000.0 / TPeak;
			const double BaseRpm = std::min(OmegaToRpm(BaseOmega), Redline * 0.9);
			const int Samples = 96;
			for (int Index = 0; Index < Samples; ++Index)
			{
				const double Rpm = (Redline + 300.0) * Index / static_cast<double>(Samples - 1);
				double Value = TPeak;
				if (Rpm > BaseRpm)
				{
					Value = Engine.PeakPowerKw * 1000.0 / RpmToOmega(Rpm);
				}
				if (Rpm > Redline)
				{
					Value *= Clamp(1.0 - (Rpm - Redline) / 300.0, 0.0, 1.0);
				}
				Curve.RpmPoints.push_back(Rpm);
				Curve.TorquePoints.push_back(Value);
			}
			return Curve;
		}

		const double Idle = std::max(500.0, Engine.IdleRpm);
		double TorqueRpm = Clamp(Engine.TorqueRpm, Idle + 200.0, PowerRpm * 0.95);
		std::vector<double> X;
		std::vector<double> Y;
		auto AddPoint = [&X, &Y](double Rpm, double Value)
		{
			if (!X.empty() && Rpm <= X.back() + 50.0)
			{
				Rpm = X.back() + 50.0;
			}
			X.push_back(Rpm);
			Y.push_back(Value);
		};

		const bool bTurbo = Engine.Induction == Aspiration::Turbo || Engine.Induction == Aspiration::TwinTurbo;
		if (Engine.Layout == EngineLayout::DieselI6)
		{
			AddPoint(Idle, 0.6 * TPeak);
			AddPoint(TorqueRpm, TPeak);
			AddPoint(std::max(TorqueRpm + 100.0, 0.8 * PowerRpm), 0.97 * TPeak);
			AddPoint(PowerRpm, TPower);
			AddPoint(Redline, 0.7 * TPower);
			AddPoint(Redline + 200.0, 0.4 * TPower);
		}
		else if (bTurbo)
		{
			TorqueRpm = std::max(TorqueRpm, Idle + 800.0);
			AddPoint(Idle, 0.45 * TPeak);
			AddPoint(Idle + 0.85 * (TorqueRpm - Idle), 0.88 * TPeak);
			AddPoint(TorqueRpm, TPeak);
			AddPoint(std::max(TorqueRpm + 100.0, 0.8 * PowerRpm), 0.98 * TPeak);
			AddPoint(PowerRpm, TPower);
			AddPoint(Redline, 0.88 * TPower);
			AddPoint(Redline + 300.0, 0.6 * TPower);
		}
		else if (Engine.Induction == Aspiration::Supercharged)
		{
			AddPoint(Idle, 0.65 * TPeak);
			AddPoint(Idle + 0.6 * (TorqueRpm - Idle), 0.88 * TPeak);
			AddPoint(TorqueRpm, TPeak);
			AddPoint(PowerRpm, TPower);
			AddPoint(Redline, 0.9 * TPower);
			AddPoint(Redline + 300.0, 0.6 * TPower);
		}
		else
		{
			const double Lope = Clamp(Engine.CamLope, 0.0, 1.0);
			AddPoint(Idle, (0.55 - 0.15 * Lope) * TPeak);
			AddPoint(Idle + 0.5 * (TorqueRpm - Idle), (0.82 - 0.1 * Lope) * TPeak);
			AddPoint(TorqueRpm, TPeak);
			AddPoint(PowerRpm, TPower);
			AddPoint(Redline, 0.9 * TPower);
			AddPoint(Redline + 300.0, 0.6 * TPower);
		}

		Pchip(X, Y, 96, Curve.RpmPoints, Curve.TorquePoints);

		// Scale so the curve's peak power matches the rated figure exactly.
		double PeakRpm = 0.0;
		const double CurvePeak = Curve.PeakPowerKw(&PeakRpm);
		if (CurvePeak > 0.0)
		{
			const double Scale = Engine.PeakPowerKw / CurvePeak;
			for (double& Value : Curve.TorquePoints)
			{
				Value *= Scale;
			}
		}
		return Curve;
	}

	double TorqueCurve::TorqueAt(double Rpm) const
	{
		if (RpmPoints.empty())
		{
			return 0.0;
		}
		if (Rpm <= RpmPoints.front())
		{
			return TorquePoints.front();
		}
		if (Rpm >= RpmPoints.back())
		{
			return 0.0;
		}
		// Uniform spacing: direct index.
		const double Span = RpmPoints.back() - RpmPoints.front();
		const double Position = (Rpm - RpmPoints.front()) / Span * static_cast<double>(RpmPoints.size() - 1);
		const std::size_t Index = std::min(static_cast<std::size_t>(Position), RpmPoints.size() - 2);
		const double T = Position - static_cast<double>(Index);
		return Lerp(TorquePoints[Index], TorquePoints[Index + 1], T);
	}

	double TorqueCurve::PowerKwAt(double Rpm) const
	{
		return TorqueAt(Rpm) * detail_dynamics::RpmToOmega(Rpm) / 1000.0;
	}

	double TorqueCurve::PeakPowerKw(double* OutRpm) const
	{
		double Best = 0.0;
		double BestRpm = 0.0;
		for (std::size_t Index = 0; Index < RpmPoints.size(); ++Index)
		{
			const double Power = TorquePoints[Index] * detail_dynamics::RpmToOmega(RpmPoints[Index]) / 1000.0;
			if (Power > Best)
			{
				Best = Power;
				BestRpm = RpmPoints[Index];
			}
		}
		if (OutRpm)
		{
			*OutRpm = BestRpm;
		}
		return Best;
	}

	double TorqueCurve::PeakTorqueNm(double* OutRpm) const
	{
		double Best = 0.0;
		double BestRpm = 0.0;
		for (std::size_t Index = 0; Index < RpmPoints.size(); ++Index)
		{
			if (TorquePoints[Index] > Best)
			{
				Best = TorquePoints[Index];
				BestRpm = RpmPoints[Index];
			}
		}
		if (OutRpm)
		{
			*OutRpm = BestRpm;
		}
		return Best;
	}

	std::vector<DynoPoint> RunDyno(const EngineSetup& Engine, double StepRpm)
	{
		std::vector<DynoPoint> Points;
		const TorqueCurve Curve = TorqueCurve::FromEngine(Engine);
		const double Start = Engine.Layout == EngineLayout::Electric ? 0.0 : std::max(500.0, Engine.IdleRpm);
		const double Step = std::max(50.0, StepRpm);
		for (double Rpm = Start; Rpm <= Engine.RedlineRpm + 1.0; Rpm += Step)
		{
			DynoPoint Point;
			Point.Rpm = Rpm;
			Point.TorqueNm = Curve.TorqueAt(Rpm);
			Point.PowerKw = Curve.PowerKwAt(Rpm);
			Points.push_back(Point);
		}
		return Points;
	}

	VehicleDynamics::VehicleDynamics(const VehicleSetup& InSetup) : Config(InSetup)
	{
		Torque = TorqueCurve::FromEngine(Config.Engine);
		const double Weight = Config.MassKg * detail_dynamics::Gravity;
		StaticLoadFront = 0.5 * Weight * Config.WeightFront;
		StaticLoadRear = 0.5 * Weight * (1.0 - Config.WeightFront);
		Reset();
	}

	void VehicleDynamics::Reset()
	{
		Omega = {0.0, 0.0, 0.0, 0.0};
		EngineOmega = detail_dynamics::RpmToOmega(Config.Engine.IdleRpm);
		BoostState = 0.0;
		Gear = Config.Gearbox.Ratios.empty() ? 0 : 1;
		PendingGear = Gear;
		ShiftTimer = 0.0;
		bClutchLocked = false;
		LimiterTimer = 0.0;
		TcFactor = 1.0;
		NitroLeft = Config.NitroSeconds;
		Telem = DynamicsTelemetry();
		Telem.Gear = Gear;
		Telem.NitroRemaining = NitroLeft;
	}

	double VehicleDynamics::TotalRatio(int GearIndex) const
	{
		const auto& Ratios = Config.Gearbox.Ratios;
		if (GearIndex == 0 || Ratios.empty())
		{
			return 0.0;
		}
		if (GearIndex < 0)
		{
			return -Config.Gearbox.ReverseRatio * Config.Gearbox.FinalDrive;
		}
		const int Index = Clamp(GearIndex, 1, static_cast<int>(Ratios.size())) - 1;
		return Ratios[static_cast<std::size_t>(Index)] * Config.Gearbox.FinalDrive;
	}

	bool VehicleDynamics::IsDriven(int Wheel) const
	{
		switch (Config.Drive)
		{
		case Drivetrain::FWD: return Wheel < 2;
		case Drivetrain::RWD: return Wheel >= 2;
		case Drivetrain::AWD: return true;
		}
		return Wheel >= 2;
	}

	double VehicleDynamics::SteerAngleRad(int Wheel, double SteerInput) const
	{
		using namespace detail_dynamics;
		const double Lock = Config.SteerLockDeg * Pi / 180.0;
		const double ToeFront = Config.Suspension.ToeFrontDeg * Pi / 180.0;
		const double ToeRear = Config.Suspension.ToeRearDeg * Pi / 180.0;
		if (Wheel >= 2)
		{
			// Toe-in: the left wheel points slightly right and vice versa.
			return Wheel == 2 ? ToeRear : -ToeRear;
		}
		const double Base = Clamp(SteerInput, -1.0, 1.0) * Lock;
		double Angle = Base;
		if (std::fabs(Base) > 1e-4)
		{
			// 60% Ackermann: the inside wheel turns tighter.
			const double TurnRadius = Config.WheelbaseM / std::tan(std::fabs(Base));
			const double HalfTrack = 0.5 * Config.TrackFrontM;
			const bool bTurningRight = Base > 0.0;
			const bool bInside = (Wheel == 1) == bTurningRight;
			const double Exact = std::atan(Config.WheelbaseM / (TurnRadius + (bInside ? -HalfTrack : HalfTrack)));
			Angle = Sign(Base) * Lerp(std::fabs(Base), Exact, 0.6);
		}
		return Angle + (Wheel == 0 ? ToeFront : -ToeFront);
	}

	void VehicleDynamics::UpdateGearbox(double Dt, const DriverInput& Input, double GroundSpeedMs, double& Throttle, double& Brake)
	{
		using namespace detail_dynamics;
		const int Gears = GearCount();
		const double Rpm = OmegaToRpm(EngineOmega);
		auto BeginShift = [this](int Target)
		{
			if (Target == Gear)
			{
				return;
			}
			PendingGear = Target;
			ShiftTimer = Config.Gearbox.ShiftSeconds;
			SinceShift = 0.0;
			bClutchLocked = false;
			if (ShiftTimer <= 0.0)
			{
				Gear = Target;
			}
		};

		SinceShift += Dt;
		const bool bKickdownPressed = Input.Throttle > 0.9 && LastThrottle < 0.6;
		LastThrottle = Input.Throttle;
		if (ShiftTimer > 0.0)
		{
			ShiftTimer -= Dt;
			if (ShiftTimer <= 0.0)
			{
				ShiftTimer = 0.0;
				Gear = PendingGear;
			}
		}
		else if (bAutomatic)
		{
			if (Gear == 0)
			{
				BeginShift(1);
			}
			else if (Gear > 0)
			{
				if (Input.Brake > 0.5 && Input.Throttle < 0.05 && GroundSpeedMs < 0.6)
				{
					BeginShift(-1);
				}
				else if (Gear < Gears && bClutchLocked)
				{
					const double PowerRpm = Config.Engine.PowerRpm;
					const double Redline = Config.Engine.RedlineRpm;
					// Full throttle shifts just past peak power; light throttle short-shifts.
					const double FullUp = std::min(Redline * 0.97, std::max(PowerRpm * 1.05, Redline * 0.9));
					const double UpRpm = Lerp(0.45 * Redline, FullUp, Clamp(Input.Throttle * 1.6, 0.0, 1.0));
					if (Rpm > UpRpm)
					{
						BeginShift(Gear + 1);
					}
				}
				if (ShiftTimer <= 0.0 && Gear > 1 && Gear == PendingGear)
				{
					const double LowerRatio = TotalRatio(Gear - 1);
					const double CurrentRatio = TotalRatio(Gear);
					const double RpmInLower = CurrentRatio != 0.0 ? Rpm * LowerRatio / CurrentRatio : 0.0;
					// Downshift when lugging, or on a fresh kickdown; a short cooldown stops shift hunting.
					const bool bLugging = Rpm < 0.4 * Config.Engine.PowerRpm;
					const bool bKickdown = bKickdownPressed && Rpm < 0.65 * Config.Engine.PowerRpm;
					const bool bCooledDown = SinceShift > 1.0;
					if (((bLugging && bCooledDown) || bKickdown) && RpmInLower < 0.85 * Config.Engine.RedlineRpm)
					{
						BeginShift(Gear - 1);
					}
				}
			}
			else if (Gear < 0 && ((Input.Throttle > 0.1 && GroundSpeedMs > -0.6) || GroundSpeedMs > 1.5))
			{
				// Throttle from a standstill, or rolling forward in reverse: back into first.
				BeginShift(1);
			}
		}
		else if (Input.ShiftRequest != 0)
		{
			const int Target = Clamp(Gear + (Input.ShiftRequest > 0 ? 1 : -1), -1, Gears);
			if (Target >= 0 || std::fabs(GroundSpeedMs) < 1.5)
			{
				BeginShift(Target);
			}
		}

		// Automatic reverse: the brake pedal drives backwards and the throttle brakes (only once stopped or reversing).
		if (bAutomatic && Gear < 0 && GroundSpeedMs < 1.0)
		{
			std::swap(Throttle, Brake);
		}
	}

	double VehicleDynamics::CurrentBoostFactor() const
	{
		const double BoostBar = Config.Engine.BoostBar;
		const double Base = BoostBar > 0.0 ? 1.0 / (1.0 + 0.7 * BoostBar) : 1.0;
		return Base + (1.0 - Base) * BoostState;
	}

	double VehicleDynamics::EngineTorque(double Rpm, double Throttle, double BoostDemand, double Dt)
	{
		const EngineSetup& Engine = Config.Engine;
		// Boost follows the pedal (BoostDemand); traction control trims torque, not boost.
		if (Engine.BoostBar > 0.0)
		{
			if (Engine.Induction == Aspiration::Supercharged)
			{
				BoostState = BoostDemand * Clamp(Rpm / std::max(1.0, 0.6 * Engine.PowerRpm), 0.3, 1.0);
			}
			else
			{
				const double RpmFactor = Engine.BoostThresholdRpm > 0.0 ? std::pow(Clamp(Rpm / Engine.BoostThresholdRpm, 0.0, 1.0), 2.0) : 1.0;
				double Target = BoostDemand * RpmFactor;
				if (Engine.bAntiLag && Rpm > Engine.BoostThresholdRpm)
				{
					Target = std::max(Target, 0.7);
				}
				const double Tau = Target > BoostState ? std::max(0.05, Engine.SpoolSeconds) : 0.15;
				BoostState += (Target - BoostState) * std::min(1.0, Dt / Tau);
			}
			BoostState = Clamp(BoostState, 0.0, 1.0);
		}
		else
		{
			BoostState = 0.0;
		}
		const double BoostFactor = CurrentBoostFactor();
		const double Health = 0.3 + 0.7 * Clamp(EngineHealth, 0.0, 1.0);
		const double Full = Torque.TorqueAt(Rpm) * BoostFactor * Health;
		const bool bElectric = Engine.Layout == EngineLayout::Electric;
		const double Friction = (0.05 + 0.1 * Clamp(Rpm / std::max(1.0, Engine.RedlineRpm), 0.0, 1.2)) * Engine.PeakTorqueNm * (bElectric ? 0.4 : 1.0);
		double Value = Throttle * Full - (1.0 - Throttle) * (Rpm > 50.0 ? Friction : 0.0);
		Telem.EngineLoad = Full > 1.0 ? Clamp(Value / Full, 0.0, 1.0) : 0.0;
		return Value;
	}

	void VehicleDynamics::SplitTorque(double AxleTorque, double OmegaLeft, double OmegaRight, const DiffSetup& Diff, double& OutLeft, double& OutRight) const
	{
		OutLeft = 0.5 * AxleTorque;
		OutRight = 0.5 * AxleTorque;
		if (Diff.Type == DiffType::Open)
		{
			return;
		}
		const double Capacity = Diff.Type == DiffType::Locked ? 1.0e9 :
			Diff.PreloadNm + (AxleTorque >= 0.0 ? Diff.AccelLock : Diff.DecelLock) * std::fabs(AxleTorque);
		// Torque flows from the faster wheel to the slower one, up to the lock capacity.
		const double Transfer = Clamp(detail_dynamics::DiffStiffness * (OmegaRight - OmegaLeft), -Capacity, Capacity);
		OutLeft += 0.5 * Transfer;
		OutRight -= 0.5 * Transfer;
	}

	std::array<WheelOutput, 4> VehicleDynamics::Step(double Dt, const DriverInput& Input, const std::array<WheelContact, 4>& Contacts)
	{
		using namespace detail_dynamics;
		std::array<WheelOutput, 4> Result{};
		if (Dt <= 0.0)
		{
			return Result;
		}

		double GroundSpeed = 0.0;
		int GroundedCount = 0;
		for (const WheelContact& Contact : Contacts)
		{
			if (Contact.bGrounded)
			{
				GroundSpeed += Contact.VelLongMs;
				++GroundedCount;
			}
		}
		GroundSpeed = GroundedCount > 0 ? GroundSpeed / GroundedCount : 0.0;

		double Throttle = Clamp(Input.Throttle, 0.0, 1.0);
		double Brake = Clamp(Input.Brake, 0.0, 1.0);
		UpdateGearbox(Dt, Input, GroundSpeed, Throttle, Brake);

		const bool bElectric = Config.Engine.Layout == EngineLayout::Electric;
		const int Substeps = std::max(1, static_cast<int>(std::ceil(Dt / SubstepSeconds)));
		const double H = Dt / Substeps;
		const double EngineInertia = std::max(0.02, Config.Engine.InertiaKgm2);
		const double Idle = Config.Engine.IdleRpm;
		int DrivenCount = 0;
		for (int Wheel = 0; Wheel < 4; ++Wheel)
		{
			DrivenCount += IsDriven(Wheel) ? 1 : 0;
		}

		Telem.bAbsActive = false;
		Telem.bTractionControlActive = false;
		Telem.bNitroActive = false;
		Telem.bRevLimiter = false;
		std::array<double, 4> Fx{};
		std::array<double, 4> Fy{};

		for (int Sub = 0; Sub < Substeps; ++Sub)
		{
			const double Ratio = ShiftTimer > 0.0 ? 0.0 : TotalRatio(Gear);
			const bool bEngaged = Ratio != 0.0;
			double DrivenOmega = 0.0;
			for (int Wheel = 0; Wheel < 4; ++Wheel)
			{
				if (IsDriven(Wheel))
				{
					DrivenOmega += Omega[static_cast<std::size_t>(Wheel)];
				}
			}
			DrivenOmega /= std::max(1, DrivenCount);
			const double OmegaIn = DrivenOmega * Ratio;

			// What the driven tires can carry, as torque at the wheels.
			double WheelTraction = 0.0;
			for (int Wheel = 0; Wheel < 4; ++Wheel)
			{
				const std::size_t W = static_cast<std::size_t>(Wheel);
				if (IsDriven(Wheel) && Contacts[W].bGrounded)
				{
					const TireSetup& Tire = Wheel < 2 ? Config.FrontTire : Config.RearTire;
					WheelTraction += Tire.Mu * Contacts[W].SurfaceGrip * Config.ChassisGrip * Contacts[W].LoadN * Tire.RadiusM;
				}
			}
			// Launch: the automatic clutch is still slipping. Like launch control, hold the engine near the launch rpm
			// and let the clutch (not the throttle) meter torque to what the tires can take.
			const bool bLaunching = bAutomatic && bEngaged && !bClutchLocked && !bElectric && std::fabs(GroundSpeed) < 25.0;
			const double LaunchRpm = Idle + std::min(500.0 + 2500.0 * Throttle, std::max(600.0, 0.6 * Config.Engine.RedlineRpm - Idle));

			// Traction control, feed-forward part: never ask for more torque than the driven tires can carry.
			double TcLimit = 1.0;
			if (Config.TractionControl > 0 && Ratio != 0.0 && !bLaunching)
			{
				// Headroom covers the torque spent spinning up the engine and wheels (large in low gears);
				// lower settings allow a little more slip for faster launches.
				const double Traction = WheelTraction * 1.5 * (1.0 + 0.08 * (5 - Config.TractionControl));
				const double EngineLimit = Traction / (std::fabs(Ratio) * Config.Gearbox.Efficiency);
				const double Available = Torque.TorqueAt(OmegaToRpm(EngineOmega)) * CurrentBoostFactor() + 1.0;
				TcLimit = Clamp(EngineLimit / Available, 0.15, 1.0);
			}

			// Rev limiter and traction control shape the throttle the engine actually sees.
			// Feed-forward caps the throttle at what the tires can take; feedback trims below that when they still spin.
			double EffectiveThrottle = std::min(Throttle, TcLimit) * (bLaunching ? 1.0 : TcFactor);
			if (bLaunching && Throttle > 0.05)
			{
				// Rpm governor (ignition cut) above the launch rpm.
				EffectiveThrottle *= Clamp((LaunchRpm + 400.0 - OmegaToRpm(EngineOmega)) / 400.0, 0.0, 1.0);
			}
			if (TcLimit < Throttle)
			{
				Telem.bTractionControlActive = true;
			}
			if (LimiterTimer > 0.0)
			{
				LimiterTimer -= H;
				EffectiveThrottle = 0.0;
				Telem.bRevLimiter = true;
			}

			double EngineRpm = OmegaToRpm(EngineOmega);
			const double BoostDemand = LimiterTimer > 0.0 && !Config.Engine.bAntiLag ? 0.0 : Throttle;
			double EngineT = EngineTorque(EngineRpm, EffectiveThrottle, BoostDemand, H);

			// Nitrous: extra torque equivalent to its rated power while the bottle lasts.
			if (Input.bNitro && NitroLeft > 0.0 && bEngaged && EffectiveThrottle > 0.5)
			{
				EngineT += Config.NitroKw * 1000.0 / std::max(EngineOmega, 150.0);
				NitroLeft -= H;
				Telem.bNitroActive = true;
			}

			double ClutchTorque = 0.0;
			double Reflected = 0.0;
			if (!bEngaged && ShiftTimer > 0.0 && PendingGear != 0)
			{
				// Mid-shift: the gearbox blips/cuts to rev-match the gear being selected.
				const double Target = std::max(RpmToOmega(Idle), std::fabs(DrivenOmega * TotalRatio(PendingGear)));
				const double Rate = std::min(1.0, H / (0.3 * Config.Gearbox.ShiftSeconds + 0.01));
				EngineOmega += (Target - EngineOmega) * Rate;
				bClutchLocked = false;
			}
			else if (!bEngaged)
			{
				// Neutral: the engine spins freely; an idle controller keeps it running.
				double IdleAssist = 0.0;
				if (!bElectric && EngineRpm < Idle)
				{
					IdleAssist = Clamp((Idle - EngineRpm) / 200.0, 0.0, 1.0) * 0.3 * Config.Engine.PeakTorqueNm;
				}
				EngineOmega += H * (EngineT + IdleAssist) / EngineInertia;
				bClutchLocked = false;
			}
			else if (bElectric)
			{
				EngineOmega = std::fabs(OmegaIn);
				ClutchTorque = EngineT;
				Reflected = EngineInertia * Ratio * Ratio;
				bClutchLocked = true;
			}
			else
			{
				// Automatic launch clutch: it bites progressively as the engine revs toward the launch rpm,
				// and is fully engaged once the driveline itself is turning that fast.
				const double DrivelineRpm = OmegaToRpm(OmegaIn);
				double Engagement = 1.0;
				if (bAutomatic && DrivelineRpm < LaunchRpm)
				{
					const double ByEngine = Clamp((EngineRpm - Idle) / std::max(100.0, LaunchRpm - Idle), 0.05, 1.0);
					const double ByDriveline = Clamp(DrivelineRpm / LaunchRpm, 0.0, 1.0);
					Engagement = std::max(ByEngine, ByDriveline);
					// Off the throttle at crawling speed the automatic clutch opens: the car rolls, it doesn't creep.
					if (Throttle < 0.02)
					{
						Engagement = 0.0;
					}
				}
				const double Capacity = Config.Gearbox.ClutchCapacityNm;
				if (bClutchLocked)
				{
					const bool bOverpowered = std::fabs(EngineT) > Capacity;
					const bool bStalling = bAutomatic && OmegaToRpm(OmegaIn) < 0.95 * Idle;
					if (bOverpowered || bStalling)
					{
						bClutchLocked = false;
					}
				}
				if (bClutchLocked)
				{
					EngineOmega = OmegaIn;
					ClutchTorque = EngineT;
					Reflected = EngineInertia * Ratio * Ratio;
				}
				else
				{
					// A slipping clutch transmits its (bounded) friction torque in the direction of the slip, but never
					// more than it takes to bring both sides to the same speed this substep (avoids chatter).
					const double Slip = EngineOmega - OmegaIn;
					double Cap = Capacity * Engagement;
					if (bLaunching && Config.TractionControl > 0)
					{
						// Launch control: slip the clutch so the tires sit just past their grip peak.
						// TcFactor (slip feedback) backs the clutch off further when the tires still break loose.
						const double LaunchTraction = WheelTraction * (0.98 + 0.03 * (5 - Config.TractionControl)) * TcFactor;
						Cap = std::min(Cap, LaunchTraction / std::max(1e-6, std::fabs(Ratio) * Config.Gearbox.Efficiency));
						if (Cap < Capacity * Engagement)
						{
							Telem.bTractionControlActive = true;
						}
					}
					double DrivenInertia = 0.0;
					for (int Wheel = 0; Wheel < 4; ++Wheel)
					{
						if (IsDriven(Wheel))
						{
							DrivenInertia += (Wheel < 2 ? Config.FrontTire : Config.RearTire).InertiaKgm2;
						}
					}
					const double DrivelineInertia = DrivenInertia / std::max(1e-6, Ratio * Ratio * Config.Gearbox.Efficiency);
					const double StickTorque = std::fabs(Slip) / (H * (1.0 / EngineInertia + 1.0 / DrivelineInertia));
					ClutchTorque = Sign(Slip) * std::min(Cap, StickTorque);
					double IdleAssist = 0.0;
					if (EngineRpm < Idle)
					{
						IdleAssist = Clamp((Idle - EngineRpm) / 200.0, 0.0, 1.0) * 0.3 * Config.Engine.PeakTorqueNm;
					}
					EngineOmega += H * (EngineT + IdleAssist - ClutchTorque) / EngineInertia;
					const double NewSlip = EngineOmega - OmegaIn;
					if (Engagement > 0.2 && std::fabs(EngineT) <= Capacity && DrivelineRpm > 0.95 * Idle &&
						(Sign(NewSlip) != Sign(Slip) || std::fabs(NewSlip) < 3.0))
					{
						EngineOmega = OmegaIn;
						bClutchLocked = true;
					}
				}
			}
			EngineOmega = std::max(0.0, EngineOmega);
			EngineRpm = OmegaToRpm(EngineOmega);
			if (EngineRpm >= Config.Engine.RedlineRpm && LimiterTimer <= 0.0)
			{
				LimiterTimer = 0.06;
			}

			// Torque at the wheels, through the differentials.
			const double AxleTorqueTotal = bEngaged ? ClutchTorque * Ratio * Config.Gearbox.Efficiency : 0.0;
			std::array<double, 4> DriveT{};
			switch (Config.Drive)
			{
			case Drivetrain::FWD:
				SplitTorque(AxleTorqueTotal, Omega[0], Omega[1], Config.FrontDiff, DriveT[0], DriveT[1]);
				break;
			case Drivetrain::RWD:
				SplitTorque(AxleTorqueTotal, Omega[2], Omega[3], Config.RearDiff, DriveT[2], DriveT[3]);
				break;
			case Drivetrain::AWD:
			{
				const double FrontOmega = 0.5 * (Omega[0] + Omega[1]);
				const double RearOmega = 0.5 * (Omega[2] + Omega[3]);
				double FrontT = AxleTorqueTotal * Config.AwdFrontSplit;
				double RearT = AxleTorqueTotal * (1.0 - Config.AwdFrontSplit);
				const DiffSetup& Center = Config.CenterDiff;
				if (Center.Type != DiffType::Open)
				{
					const double Capacity = Center.Type == DiffType::Locked ? 1.0e9 :
						Center.PreloadNm + Center.AccelLock * std::fabs(AxleTorqueTotal);
					const double Transfer = Clamp(DiffStiffness * (RearOmega - FrontOmega), -Capacity, Capacity);
					FrontT += 0.5 * Transfer;
					RearT -= 0.5 * Transfer;
				}
				SplitTorque(FrontT, Omega[0], Omega[1], Config.FrontDiff, DriveT[0], DriveT[1]);
				SplitTorque(RearT, Omega[2], Omega[3], Config.RearDiff, DriveT[2], DriveT[3]);
				break;
			}
			}

			// Wheels: tire forces and spin.
			double MaxDrivenSlip = 0.0;
			for (int Wheel = 0; Wheel < 4; ++Wheel)
			{
				const std::size_t W = static_cast<std::size_t>(Wheel);
				const WheelContact& Contact = Contacts[W];
				const bool bFront = Wheel < 2;
				const TireSetup& Tire = bFront ? Config.FrontTire : Config.RearTire;
				const double Radius = Tire.RadiusM;
				const double Inertia = Tire.InertiaKgm2 + (IsDriven(Wheel) && Reflected > 0.0 ? Reflected / DrivenCount : 0.0);

				double BrakeT = Brake * (bFront ? Config.Brakes.MaxTorqueFront : Config.Brakes.MaxTorqueRear);
				if (!bFront)
				{
					BrakeT += Clamp(Input.Handbrake, 0.0, 1.0) * Config.Brakes.HandbrakeTorque;
				}

				double WheelFx = 0.0;
				double WheelFy = 0.0;
				double Stiffness = 0.0;
				if (Contact.bGrounded && Contact.LoadN > 0.0)
				{
					const double Denominator = std::max(std::fabs(Contact.VelLongMs), SlipVelocityFloor);
					const double SlipRatio = (Omega[W] * Radius - Contact.VelLongMs) / Denominator;
					const double SlipAngle = std::atan2(Contact.VelLatMs, Denominator);

					// ABS releases a wheel that is about to lock.
					if (Config.bAbs && Brake > 0.05 && SlipRatio < -0.15 && std::fabs(Contact.VelLongMs) > 2.0)
					{
						BrakeT *= 0.3;
						Telem.bAbsActive = true;
					}

					const double Camber = std::fabs(bFront ? Config.Suspension.CamberFrontDeg : Config.Suspension.CamberRearDeg);
					const double CamberLat = 1.0 + 0.05 * std::min(Camber, 2.5) - 0.04 * std::max(0.0, Camber - 2.5);
					const double CamberLong = 1.0 - 0.015 * Camber;
					const double Grip = Contact.SurfaceGrip * Config.ChassisGrip;
					const double Nominal = bFront ? StaticLoadFront : StaticLoadRear;
					const TireForce Force = ComputeTireForce(Tire.Mu, Tire.StiffnessMul, SlipRatio, SlipAngle, Contact.LoadN, Nominal,
						Grip * CamberLong, Grip * CamberLat);
					WheelFx = Force.Fx;
					WheelFy = Force.Fy;

					// Stability: the tire can at most cancel the patch's sliding velocity within one frame.
					const double CornerMass = Contact.LoadN / Gravity;
					const double MaxFy = CornerMass * std::fabs(Contact.VelLatMs) / Dt * 0.9;
					WheelFy = Clamp(WheelFy, -MaxFy, MaxFy);
					const double SlipVelocity = Omega[W] * Radius - Contact.VelLongMs;
					const double MaxFx = CornerMass * std::fabs(SlipVelocity) / Dt * 0.9 + 1.0;
					WheelFx = Clamp(WheelFx, -MaxFx, MaxFx);

					// Linearized tire stiffness for the semi-implicit wheel update.
					const TireForce Nudged = ComputeTireForce(Tire.Mu, Tire.StiffnessMul, SlipRatio + 1e-3, SlipAngle, Contact.LoadN, Nominal,
						Grip * CamberLong, Grip * CamberLat);
					Stiffness = std::fabs(Nudged.Fx - Force.Fx) / 1e-3 * Radius / Denominator;

					Telem.SlipRatio[W] = SlipRatio;
					Telem.SlipAngle[W] = SlipAngle;
					if (IsDriven(Wheel))
					{
						MaxDrivenSlip = std::max(MaxDrivenSlip, SlipRatio * Sign(Contact.VelLongMs >= 0.0 ? 1.0 : -1.0));
					}
				}
				else
				{
					Telem.SlipRatio[W] = 0.0;
					Telem.SlipAngle[W] = 0.0;
				}
				Telem.TireLoad[W] = Contact.bGrounded ? Contact.LoadN : 0.0;

				const double Effective = Inertia + H * Stiffness * Radius;
				const double Net = DriveT[W] - WheelFx * Radius;
				double NewOmega = Omega[W] + H * Net / Effective;
				const double BrakeStep = H * BrakeT / Effective;
				if (std::fabs(NewOmega) <= BrakeStep)
				{
					NewOmega = 0.0;
				}
				else
				{
					NewOmega -= Sign(NewOmega) * BrakeStep;
				}
				Omega[W] = NewOmega;
				Fx[W] += WheelFx * H;
				Fy[W] += WheelFy * H;
			}

			// Locked diffs force both wheels on the axle to turn together.
			auto LockAxle = [this](std::size_t Left, std::size_t Right)
			{
				const double Average = 0.5 * (Omega[Left] + Omega[Right]);
				Omega[Left] = Average;
				Omega[Right] = Average;
			};
			if (Config.FrontDiff.Type == DiffType::Locked && (Config.Drive != Drivetrain::RWD))
			{
				LockAxle(0, 1);
			}
			if (Config.RearDiff.Type == DiffType::Locked && (Config.Drive != Drivetrain::FWD))
			{
				LockAxle(2, 3);
			}

			// Traction control: a proportional controller that holds driven-wheel slip near its target.
			if (Config.TractionControl > 0)
			{
				// Feedback part: gently trims what the feed-forward limit misses (bumps, surface changes).
				const double Target = 0.30 - 0.035 * Config.TractionControl;
				const double Excess = MaxDrivenSlip - Target;
				if (Excess > 0.0 && Throttle > 0.1)
				{
					TcFactor = std::max(0.1, TcFactor - Excess * 2.5 * H);
					Telem.bTractionControlActive = true;
				}
				else
				{
					TcFactor = std::min(1.0, TcFactor + 1.5 * H);
				}
			}
			else
			{
				TcFactor = 1.0;
			}
		}

		for (std::size_t W = 0; W < 4; ++W)
		{
			Result[W].Fx = Fx[W] / Dt;
			Result[W].Fy = Fy[W] / Dt;
			Telem.WheelOmega[W] = Omega[W];
		}
		Telem.EngineRpm = OmegaToRpm(EngineOmega);
		Telem.Throttle = Throttle * TcFactor;
		Telem.Boost = BoostState;
		Telem.BoostBar = BoostState * Config.Engine.BoostBar;
		Telem.Gear = ShiftTimer > 0.0 ? PendingGear : Gear;
		Telem.bShifting = ShiftTimer > 0.0;
		Telem.bClutchSlipping = !bClutchLocked && Gear != 0 && !bElectric;
		Telem.NitroRemaining = std::max(0.0, NitroLeft);
		double DriveSum = 0.0;
		const double Ratio = TotalRatio(Gear);
		DriveSum = Ratio != 0.0 && bClutchLocked ? Torque.TorqueAt(Telem.EngineRpm) * Telem.Throttle * Ratio * Config.Gearbox.Efficiency : 0.0;
		Telem.DriveTorqueNm = DriveSum;
		return Result;
	}

	void VehicleDynamics::AeroForces(const AeroSetup& Aero, double SpeedMs, double& OutDragN, double& OutFrontN, double& OutRearN)
	{
		const double V2 = SpeedMs * SpeedMs;
		OutDragN = 0.5 * detail_dynamics::AirDensity * Aero.DragArea * V2;
		OutFrontN = Aero.DownforceFront * V2;
		OutRearN = Aero.DownforceRear * V2;
	}

	std::array<double, 4> VehicleDynamics::SuspensionForces(const SuspensionSetup& Susp, const std::array<double, 4>& CompressionM,
		const std::array<double, 4>& CompressionVelMs, const std::array<bool, 4>& Grounded)
	{
		std::array<double, 4> Forces{};
		for (std::size_t W = 0; W < 4; ++W)
		{
			if (!Grounded[W])
			{
				continue;
			}
			const bool bFront = W < 2;
			const double Spring = bFront ? Susp.SpringFront : Susp.SpringRear;
			const double Velocity = CompressionVelMs[W];
			const double Damper = Velocity > 0.0 ? (bFront ? Susp.BumpFront : Susp.BumpRear) : (bFront ? Susp.ReboundFront : Susp.ReboundRear);
			const double Compression = std::max(0.0, CompressionM[W]);
			double Force = Spring * Compression + Damper * Velocity;
			// Bump stop beyond the travel limit.
			if (Compression > Susp.TravelM)
			{
				Force += Spring * 10.0 * (Compression - Susp.TravelM);
			}
			Forces[W] = std::max(0.0, Force);
		}
		auto AntiRoll = [&Forces, &CompressionM, &Grounded](std::size_t Left, std::size_t Right, double Stiffness)
		{
			if (Grounded[Left] && Grounded[Right])
			{
				const double Twist = Stiffness * (CompressionM[Left] - CompressionM[Right]);
				Forces[Left] = std::max(0.0, Forces[Left] + Twist);
				Forces[Right] = std::max(0.0, Forces[Right] - Twist);
			}
		};
		AntiRoll(0, 1, Susp.AntiRollFront);
		AntiRoll(2, 3, Susp.AntiRollRear);
		return Forces;
	}

	LaunchResult SimulateLaunch(const VehicleSetup& Setup, double MaxSeconds)
	{
		using namespace detail_dynamics;
		LaunchResult Result;
		VehicleDynamics Dyn(Setup);
		Dyn.bAutomatic = true;
		const double Dt = 1.0 / 240.0;
		const double Mass = Setup.MassKg;
		double Speed = 0.0;
		double Distance = 0.0;
		double Accel = 0.0;
		DriverInput Input;
		Input.Throttle = 1.0;
		for (double Time = 0.0; Time < MaxSeconds; Time += Dt)
		{
			double Drag = 0.0;
			double DownFront = 0.0;
			double DownRear = 0.0;
			VehicleDynamics::AeroForces(Setup.Aero, Speed, Drag, DownFront, DownRear);
			const double Transfer = Mass * Accel * Setup.CgHeightM / std::max(1.0, Setup.WheelbaseM);
			const double FrontAxle = std::max(0.0, Mass * Gravity * Setup.WeightFront + DownFront - Transfer);
			const double RearAxle = std::max(0.0, Mass * Gravity * (1.0 - Setup.WeightFront) + DownRear + Transfer);
			std::array<WheelContact, 4> Contacts{};
			for (std::size_t W = 0; W < 4; ++W)
			{
				Contacts[W].bGrounded = true;
				Contacts[W].LoadN = 0.5 * (W < 2 ? FrontAxle : RearAxle);
				Contacts[W].VelLongMs = Speed;
				Contacts[W].VelLatMs = 0.0;
			}
			const std::array<WheelOutput, 4> Out = Dyn.Step(Dt, Input, Contacts);
			double Push = 0.0;
			for (const WheelOutput& Wheel : Out)
			{
				Push += Wheel.Fx;
			}
			const double Rolling = Speed > 0.1 ? 0.012 * Mass * Gravity : 0.0;
			Accel = (Push - Drag - Rolling) / Mass;
			const double Before = Speed;
			Speed = std::max(0.0, Speed + Accel * Dt);
			Distance += 0.5 * (Before + Speed) * Dt;
			const double Kph = Speed * 3.6;
			if (Result.ZeroTo100S == 0.0 && Kph >= 100.0) Result.ZeroTo100S = Time + Dt;
			if (Result.ZeroTo200S == 0.0 && Kph >= 200.0) Result.ZeroTo200S = Time + Dt;
			if (Result.QuarterMileS == 0.0 && Distance >= 402.336)
			{
				Result.QuarterMileS = Time + Dt;
				Result.QuarterMileTrapKph = Kph;
			}
			Result.TopSpeedKph = std::max(Result.TopSpeedKph, Kph);
		}
		return Result;
	}
}
