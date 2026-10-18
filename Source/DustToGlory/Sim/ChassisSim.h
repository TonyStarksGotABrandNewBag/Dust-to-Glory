// Rigid-body chassis: suspension ray casts, tire contact, aero and collisions around VehicleDynamics.
//
// The chassis integrates a 6-DOF rigid body at a fixed 240 Hz. Each sub-step it asks a GroundQuery where
// the road is under every wheel, turns spring compression into tire load, hands the contact-patch
// velocities to VehicleDynamics (powertrain, tires, wheel spin) and applies the resulting forces at the
// contact patches. ADTGVehicle supplies ray casts against the Unreal world and sweeps the body for
// collisions; the unit tests use FlatGround.
#pragma once

#include "Sim/Dynamics.h"
#include "Sim/Math3.h"

#include <array>

namespace dtg::sim
{
	struct GroundHit
	{
		bool bHit = false;
		double Distance = 0.0;   // metres along the ray
		Vec3 Point;
		Vec3 Normal{0.0, 0.0, 1.0};
		double Grip = 1.0;       // surface grip (tarmac 1, wet ~0.7, dirt ~0.6)
	};

	/** Where is the ground? Implemented with line traces in the game and analytically in tests. */
	class GroundQuery
	{
	public:
		virtual ~GroundQuery() = default;
		virtual GroundHit Cast(const Vec3& Origin, const Vec3& Direction, double MaxDistance) const = 0;
	};

	/** An infinite horizontal plane. */
	class FlatGround : public GroundQuery
	{
	public:
		explicit FlatGround(double InHeight = 0.0, double InGrip = 1.0) : Height(InHeight), Grip(InGrip) {}
		GroundHit Cast(const Vec3& Origin, const Vec3& Direction, double MaxDistance) const override;

	private:
		double Height;
		double Grip;
	};

	struct ChassisWheel
	{
		Vec3 MountLocal;           // top of the suspension strut in body space
		double Radius = 0.32;
		double RestLength = 0.45;
		double Travel = 0.2;
		double Compression = 0.0;
		double CompressionVel = 0.0;
		double LoadN = 0.0;
		bool bGrounded = false;
		Vec3 ContactPoint;
		Vec3 ContactNormal{0.0, 0.0, 1.0};
		double SurfaceGrip = 1.0;
		double SpinAngle = 0.0;    // radians, for the renderer
		double SteerAngle = 0.0;   // radians, + = right
		double Extension = 0.45;   // current strut length (mount to wheel centre)

		/** Wheel centre in body space for the renderer. */
		Vec3 CenterLocal() const { return MountLocal - Vec3(0.0, 0.0, Extension); }
	};

	struct ChassisState
	{
		Vec3 Position;              // centre of mass, world metres
		Quat Orientation;
		Vec3 Velocity;              // m/s
		Vec3 AngularVelocity;       // rad/s, world space
	};

	/** Summary of the last airborne stretch, for stunt scoring. */
	struct AirRecord
	{
		double AirTime = 0.0;
		double PitchTurns = 0.0;   // + nose over (front flip), - back flip
		double RollTurns = 0.0;
		int Landings = 0;          // increments on every landing
	};

	class ChassisSim
	{
	public:
		static constexpr double DefaultStepHz = 240.0;

		/** Fixed sub-step rate; distant AI cars can run at 120 Hz to save ray casts. */
		void SetStepRate(double Hz) { StepHz = Hz < 60.0 ? 60.0 : (Hz > 1000.0 ? 1000.0 : Hz); }

		/** Body dimensions are used for the inertia tensor; everything else comes from the setup. */
		void Configure(const VehicleSetup& Setup, double LengthM, double WidthM, double HeightM);
		/** Places the car with its wheels resting on ground at GroundZ, at rest. */
		void Teleport(const Vec3& GroundPoint, double YawRad);
		/** Advances by Dt seconds of game time (internally fixed sub-steps). */
		void Step(double Dt, const DriverInput& Input, const GroundQuery& Ground);

		void ApplyImpulse(const Vec3& WorldPoint, const Vec3& Impulse);
		/**
		 * Collision response against a surface whose normal points out of the obstacle toward the car.
		 * Returns the closing speed (m/s) along the normal, 0 if the car was already separating.
		 */
		double ResolveContact(const Vec3& WorldPoint, const Vec3& Normal, double Restitution, double Friction,
			const Vec3& OtherVelocity = Vec3());
		/** Kills all motion (busted, menus, teleports). */
		void Freeze();

		ChassisState State;

		VehicleDynamics& Dynamics() { return Dyn; }
		const VehicleDynamics& Dynamics() const { return Dyn; }
		const std::array<ChassisWheel, 4>& Wheels() const { return WheelList; }
		double Mass() const { return MassKg; }
		double CgHeight() const { return CgHeightM; }

		Vec3 Forward() const { return State.Orientation.Forward(); }
		Vec3 Right() const { return State.Orientation.Right(); }
		Vec3 Up() const { return State.Orientation.Up(); }
		double ForwardSpeed() const { return Dot(State.Velocity, Forward()); }
		double SpeedKph() const { return State.Velocity.Length() * 3.6; }
		Vec3 PointVelocity(const Vec3& WorldPoint) const;
		int GroundedWheels() const;
		/** Largest slip of any grounded tire, 0..~1, for squeal audio and smoke. */
		double MaxSlip() const;
		/** Seconds since all four wheels left the ground (0 when any wheel touches). */
		double AirTime() const { return CurrentAir; }
		const AirRecord& LastAir() const { return LastAirRecord; }
		/** True when the body is upside down or on its side and nearly stopped. */
		bool IsCapsized() const;

	private:
		void Substep(double Dt, const DriverInput& Input, const GroundQuery& Ground);
		/** Body corners against the ground (rollovers, roof landings, bottoming out). */
		void BodyContacts(const GroundQuery& Ground, Vec3& Force, Vec3& Torque) const;
		Vec3 ApplyInverseInertia(const Vec3& WorldTorque) const;

		VehicleDynamics Dyn;
		SuspensionSetup Suspension;
		AeroSetup Aero;
		std::array<ChassisWheel, 4> WheelList{};
		double MassKg = 1400.0;
		double CgHeightM = 0.5;
		Vec3 InertiaBody{500.0, 2000.0, 2200.0}; // roll, pitch, yaw (kg m^2)
		double FrontAxleX = 1.3;
		double RearAxleX = -1.4;
		Vec3 BodyMin{-2.3, -0.9, -0.3};          // body box relative to the centre of mass
		Vec3 BodyMax{2.3, 0.9, 0.9};
		double StepHz = DefaultStepHz;
		double Accumulator = 0.0;
		double CurrentAir = 0.0;
		double AirPitch = 0.0;
		double AirRoll = 0.0;
		AirRecord LastAirRecord;
	};
}
