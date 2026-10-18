// Vehicle dynamics: torque curves, tire model, clutch/gearbox/differentials and wheel spin.
//
// The engine-side vehicle (ADTGVehicle) owns the rigid body and the suspension ray casts. Each frame it
// hands this class the load and contact-patch velocity of every wheel; VehicleDynamics sub-steps the
// powertrain and wheel rotation at ~500 Hz and returns the tire forces to apply. Because it has no engine
// dependencies the same code drives the unit tests and the dyno/acceleration estimates in the garage.
#pragma once

#include "Sim/Tuning.h"

#include <array>
#include <vector>

namespace dtg::sim
{
	/** Pacejka "Magic Formula": normalized force (-1..1) for a slip value. */
	double MagicFormula(double Slip, double B, double C, double E);

	struct TireForce
	{
		double Fx = 0.0; // along the wheel's rolling direction
		double Fy = 0.0; // to the wheel's right
	};

	/** Peak slip values of the default tire curve (used to normalize combined slip). */
	constexpr double TirePeakSlipRatio = 0.10;
	constexpr double TirePeakSlipAngle = 0.13; // radians, ~7.4 degrees

	/**
	 * Combined-slip tire force. SlipRatio uses (wheel speed - ground speed) / |ground speed|, SlipAngle is in
	 * radians with positive meaning the contact patch slides to the right. GripMul folds in surface, camber
	 * and chassis effects.
	 */
	TireForce ComputeTireForce(double Mu, double StiffnessMul, double SlipRatio, double SlipAngleRad, double LoadN,
		double NominalLoadN, double GripMulLong, double GripMulLat);

	/** Full-throttle, full-boost torque curve. */
	class TorqueCurve
	{
	public:
		static TorqueCurve FromEngine(const EngineSetup& Engine);

		double TorqueAt(double Rpm) const;
		double PowerKwAt(double Rpm) const;
		double MaxRpm() const { return RpmPoints.empty() ? 0.0 : RpmPoints.back(); }
		/** Highest power on the curve and the rpm where it occurs. */
		double PeakPowerKw(double* OutRpm = nullptr) const;
		double PeakTorqueNm(double* OutRpm = nullptr) const;

		const std::vector<double>& Rpms() const { return RpmPoints; }
		const std::vector<double>& Torques() const { return TorquePoints; }

	private:
		std::vector<double> RpmPoints;
		std::vector<double> TorquePoints;
	};

	struct DynoPoint
	{
		double Rpm = 0.0;
		double TorqueNm = 0.0;
		double PowerKw = 0.0;
	};

	/** Samples the curve every StepRpm from idle to redline for the garage dyno screen. */
	std::vector<DynoPoint> RunDyno(const EngineSetup& Engine, double StepRpm = 250.0);

	struct DriverInput
	{
		double Throttle = 0.0;  // 0..1
		double Brake = 0.0;     // 0..1
		double Steer = 0.0;     // -1 left .. +1 right
		double Handbrake = 0.0; // 0..1
		bool bNitro = false;
		int ShiftRequest = 0;   // +1 up, -1 down (manual gearbox)
	};

	/** Wheel order used everywhere: 0 front-left, 1 front-right, 2 rear-left, 3 rear-right. */
	struct WheelContact
	{
		bool bGrounded = false;
		double LoadN = 0.0;       // suspension force pressing the tire into the ground
		double VelLongMs = 0.0;   // contact-patch velocity along the (steered) wheel's forward axis
		double VelLatMs = 0.0;    // ... and along its right axis
		double SurfaceGrip = 1.0; // 1 tarmac, ~0.6 wet, ~0.5 grass/dirt
	};

	struct WheelOutput
	{
		double Fx = 0.0; // N along the wheel's forward axis
		double Fy = 0.0; // N along the wheel's right axis
	};

	struct DynamicsTelemetry
	{
		double EngineRpm = 0.0;
		double Throttle = 0.0;     // after traction control and the rev limiter
		double EngineLoad = 0.0;   // 0..1 share of available torque being produced
		double Boost = 0.0;        // 0..1 of target boost
		double BoostBar = 0.0;
		int Gear = 1;              // -1 reverse, 0 neutral, 1..n
		bool bShifting = false;
		bool bRevLimiter = false;
		bool bClutchSlipping = false;
		bool bNitroActive = false;
		double NitroRemaining = 0.0; // seconds
		bool bTractionControlActive = false;
		bool bAbsActive = false;
		double DriveTorqueNm = 0.0;  // at the wheels
		std::array<double, 4> WheelOmega{};
		std::array<double, 4> SlipRatio{};
		std::array<double, 4> SlipAngle{};
		std::array<double, 4> TireLoad{};
	};

	class VehicleDynamics
	{
	public:
		VehicleDynamics() : VehicleDynamics(VehicleSetup()) {}
		explicit VehicleDynamics(const VehicleSetup& InSetup);

		void Reset();
		/** Advances by Dt seconds and returns the average tire force per wheel over the step. */
		std::array<WheelOutput, 4> Step(double Dt, const DriverInput& Input, const std::array<WheelContact, 4>& Contacts);

		/** Steering angle (radians, + = right) including Ackermann geometry and toe. */
		double SteerAngleRad(int Wheel, double SteerInput) const;
		const DynamicsTelemetry& Telemetry() const { return Telem; }
		const VehicleSetup& Setup() const { return Config; }
		const TorqueCurve& Curve() const { return Torque; }
		int GearCount() const { return static_cast<int>(Config.Gearbox.Ratios.size()); }

		bool bAutomatic = true;
		/** Damage-driven power loss, 0..1 (demolition derby). */
		double EngineHealth = 1.0;

		/** Aero drag (opposing motion) and downforce per axle for a forward speed in m/s. */
		static void AeroForces(const AeroSetup& Aero, double SpeedMs, double& OutDragN, double& OutFrontN, double& OutRearN);
		/** Spring + damper + anti-roll bar force per wheel (N, pushing the body up). */
		static std::array<double, 4> SuspensionForces(const SuspensionSetup& Susp, const std::array<double, 4>& CompressionM,
			const std::array<double, 4>& CompressionVelMs, const std::array<bool, 4>& Grounded);

	private:
		double TotalRatio(int Gear) const;
		bool IsDriven(int Wheel) const;
		void UpdateGearbox(double Dt, const DriverInput& Input, double GroundSpeedMs, double& Throttle, double& Brake);
		double EngineTorque(double Rpm, double Throttle, double BoostDemand, double Dt);
		double CurrentBoostFactor() const;
		void SplitTorque(double AxleTorque, double OmegaLeft, double OmegaRight, const DiffSetup& Diff, double& OutLeft, double& OutRight) const;

		VehicleSetup Config;
		TorqueCurve Torque;
		DynamicsTelemetry Telem;
		std::array<double, 4> Omega{};
		double EngineOmega = 0.0;
		double BoostState = 0.0;
		int Gear = 1;
		int PendingGear = 1;
		double ShiftTimer = 0.0;
		bool bClutchLocked = false;
		double LimiterTimer = 0.0;
		double TcFactor = 1.0;
		double LastThrottle = 0.0;
		double SinceShift = 10.0;
		double NitroLeft = 0.0;
		double StaticLoadFront = 3000.0;
		double StaticLoadRear = 3000.0;
	};

	struct LaunchResult
	{
		double ZeroTo100S = 0.0;    // 0 when not reached
		double ZeroTo200S = 0.0;
		double QuarterMileS = 0.0;
		double QuarterMileTrapKph = 0.0;
		double TopSpeedKph = 0.0;   // highest speed reached in the time limit
	};

	/** Straight-line run on a flat strip (with weight transfer, aero and rolling resistance). */
	LaunchResult SimulateLaunch(const VehicleSetup& Setup, double MaxSeconds = 40.0);
}
