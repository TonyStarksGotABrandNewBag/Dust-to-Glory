#include "Sim/ChassisSim.h"

#include <algorithm>

namespace dtg::sim
{
	namespace detail_chassis
	{
		constexpr double Gravity = 9.81;
		constexpr double TwoPi = 6.283185307179586;
		constexpr double RollingResistance = 0.012;
	}

	GroundHit FlatGround::Cast(const Vec3& Origin, const Vec3& Direction, double MaxDistance) const
	{
		GroundHit Hit;
		if (Direction.Z > -1e-6)
		{
			return Hit; // parallel or pointing up
		}
		const double Distance = (Origin.Z - Height) / -Direction.Z;
		if (Distance < 0.0 || Distance > MaxDistance)
		{
			return Hit;
		}
		Hit.bHit = true;
		Hit.Distance = Distance;
		Hit.Point = Origin + Direction * Distance;
		Hit.Normal = Vec3(0.0, 0.0, 1.0);
		Hit.Grip = Grip;
		return Hit;
	}

	void ChassisSim::Configure(const VehicleSetup& Setup, double LengthM, double WidthM, double HeightM)
	{
		Dyn = VehicleDynamics(Setup);
		Suspension = Setup.Suspension;
		Aero = Setup.Aero;
		MassKg = std::max(100.0, Setup.MassKg);
		CgHeightM = std::max(0.15, Setup.CgHeightM);

		// Box inertia, trimmed because a car's mass is concentrated nearer the middle than a uniform box.
		const double L = std::max(1.0, LengthM);
		const double W = std::max(0.8, WidthM);
		const double H = std::max(0.6, HeightM);
		InertiaBody = Vec3(MassKg / 12.0 * (W * W + H * H), MassKg / 12.0 * (L * L + H * H), MassKg / 12.0 * (L * L + W * W)) * 0.85;

		const double Wheelbase = std::max(1.0, Setup.WheelbaseM);
		FrontAxleX = Wheelbase * (1.0 - Setup.WeightFront);
		RearAxleX = -Wheelbase * Setup.WeightFront;
		const double Clearance = std::min(Setup.Suspension.RideHeightFrontM, Setup.Suspension.RideHeightRearM);
		const double MidX = 0.5 * (FrontAxleX + RearAxleX);
		BodyMin = Vec3(MidX - 0.5 * L, -0.5 * W, Clearance - CgHeightM);
		BodyMax = Vec3(MidX + 0.5 * L, 0.5 * W, std::max(Clearance + 0.3, H) - CgHeightM);
		for (std::size_t Index = 0; Index < 4; ++Index)
		{
			const bool bFront = Index < 2;
			const bool bLeft = Index % 2 == 0;
			ChassisWheel& Wheel = WheelList[Index];
			const TireSetup& Tire = bFront ? Setup.FrontTire : Setup.RearTire;
			const double Track = bFront ? Setup.TrackFrontM : Setup.TrackRearM;
			const double RideHeight = bFront ? Suspension.RideHeightFrontM : Suspension.RideHeightRearM;
			Wheel = ChassisWheel();
			Wheel.Radius = std::max(0.15, Tire.RadiusM);
			Wheel.RestLength = std::max(0.1, bFront ? Suspension.RestLengthFrontM : Suspension.RestLengthRearM);
			Wheel.Travel = std::max(0.05, Suspension.TravelM);
			Wheel.Extension = Wheel.RestLength;
			Wheel.MountLocal = Vec3(bFront ? FrontAxleX : RearAxleX, (bLeft ? -0.5 : 0.5) * Track, Wheel.Radius + RideHeight - CgHeightM);
		}
		Accumulator = 0.0;
		CurrentAir = 0.0;
		AirPitch = 0.0;
		AirRoll = 0.0;
		LastAirRecord = AirRecord();
	}

	void ChassisSim::Teleport(const Vec3& GroundPoint, double YawRad)
	{
		State.Orientation = Quat::FromYaw(YawRad);
		State.Position = GroundPoint + Vec3(0.0, 0.0, CgHeightM);
		State.Velocity = Vec3();
		State.AngularVelocity = Vec3();
		Dyn.Reset();
		for (ChassisWheel& Wheel : WheelList)
		{
			Wheel.Compression = 0.0;
			Wheel.CompressionVel = 0.0;
			Wheel.bGrounded = false;
		}
		Accumulator = 0.0;
		CurrentAir = 0.0;
	}

	void ChassisSim::Freeze()
	{
		State.Velocity = Vec3();
		State.AngularVelocity = Vec3();
		Dyn.Reset();
	}

	Vec3 ChassisSim::PointVelocity(const Vec3& WorldPoint) const
	{
		return State.Velocity + Cross(State.AngularVelocity, WorldPoint - State.Position);
	}

	Vec3 ChassisSim::ApplyInverseInertia(const Vec3& WorldTorque) const
	{
		const Vec3 Local = State.Orientation.Unrotate(WorldTorque);
		const Vec3 Scaled(Local.X / InertiaBody.X, Local.Y / InertiaBody.Y, Local.Z / InertiaBody.Z);
		return State.Orientation.Rotate(Scaled);
	}

	void ChassisSim::ApplyImpulse(const Vec3& WorldPoint, const Vec3& Impulse)
	{
		if (!Impulse.IsFinite())
		{
			return;
		}
		State.Velocity += Impulse / MassKg;
		State.AngularVelocity += ApplyInverseInertia(Cross(WorldPoint - State.Position, Impulse));
	}

	double ChassisSim::ResolveContact(const Vec3& WorldPoint, const Vec3& Normal, double Restitution, double Friction, const Vec3& OtherVelocity)
	{
		const Vec3 N = Normal.Normalized();
		const Vec3 R = WorldPoint - State.Position;
		const Vec3 Relative = PointVelocity(WorldPoint) - OtherVelocity;
		const double Closing = Dot(Relative, N);
		if (Closing >= 0.0)
		{
			return 0.0;
		}
		const double AngularN = Dot(N, Cross(ApplyInverseInertia(Cross(R, N)), R));
		const double ImpulseN = -(1.0 + std::clamp(Restitution, 0.0, 1.0)) * Closing / (1.0 / MassKg + AngularN);
		Vec3 Impulse = N * ImpulseN;

		const Vec3 Tangent = Relative - N * Closing;
		const double Sliding = Tangent.Length();
		if (Sliding > 1e-4)
		{
			const Vec3 T = Tangent / Sliding;
			const double AngularT = Dot(T, Cross(ApplyInverseInertia(Cross(R, T)), R));
			const double ImpulseT = std::min(std::max(0.0, Friction) * ImpulseN, Sliding / (1.0 / MassKg + AngularT));
			Impulse -= T * ImpulseT;
		}
		ApplyImpulse(WorldPoint, Impulse);
		return -Closing;
	}

	int ChassisSim::GroundedWheels() const
	{
		int Count = 0;
		for (const ChassisWheel& Wheel : WheelList)
		{
			Count += Wheel.bGrounded ? 1 : 0;
		}
		return Count;
	}

	double ChassisSim::MaxSlip() const
	{
		const DynamicsTelemetry& Telem = Dyn.Telemetry();
		double Slip = 0.0;
		for (std::size_t Index = 0; Index < 4; ++Index)
		{
			if (!WheelList[Index].bGrounded)
			{
				continue;
			}
			const double Ratio = std::fabs(Telem.SlipRatio[Index]) / TirePeakSlipRatio;
			const double Angle = std::fabs(Telem.SlipAngle[Index]) / TirePeakSlipAngle;
			Slip = std::max(Slip, std::max(Ratio, Angle));
		}
		// 0 at the grip peak and below, rising to 1 when sliding at three times the peak slip.
		return std::clamp((Slip - 1.0) / 2.0, 0.0, 1.0);
	}

	bool ChassisSim::IsCapsized() const
	{
		return Up().Z < 0.3 && State.Velocity.Length() < 2.0;
	}

	void ChassisSim::BodyContacts(const GroundQuery& Ground, Vec3& Force, Vec3& Torque) const
	{
		using namespace detail_chassis;
		// Penalty contacts at the eight corners of the body box: stiff enough to carry the whole car on
		// a couple of centimetres, damped, with sliding friction.
		const double Stiffness = MassKg * Gravity / 0.02 / 4.0;
		const double Damping = 2.0 * std::sqrt(Stiffness * MassKg * 0.25) * 0.6;
		for (int Corner = 0; Corner < 8; ++Corner)
		{
			const Vec3 Local((Corner & 1) ? BodyMax.X : BodyMin.X, (Corner & 2) ? BodyMax.Y : BodyMin.Y, (Corner & 4) ? BodyMax.Z : BodyMin.Z);
			const Vec3 Offset = State.Orientation.Rotate(Local);
			const double Reach = Offset.Length();
			if (Reach < 1e-6)
			{
				continue;
			}
			const Vec3 Direction = Offset / Reach;
			const GroundHit Hit = Ground.Cast(State.Position, Direction, Reach);
			if (!Hit.bHit)
			{
				continue;
			}
			const double Depth = (Reach - Hit.Distance) * std::max(0.0, -Dot(Direction, Hit.Normal));
			if (Depth <= 0.0)
			{
				continue;
			}
			const Vec3 Point = State.Position + Offset;
			const Vec3 Velocity = PointVelocity(Point);
			const double NormalSpeed = Dot(Velocity, Hit.Normal);
			const double Push = std::max(0.0, Stiffness * Depth - Damping * NormalSpeed);
			Vec3 Contact = Hit.Normal * Push;
			const Vec3 Sliding = ProjectOnPlane(Velocity, Hit.Normal);
			const double SlideSpeed = Sliding.Length();
			if (SlideSpeed > 1e-3)
			{
				Contact -= Sliding / SlideSpeed * (0.6 * Push * std::min(1.0, SlideSpeed / 0.3));
			}
			Force += Contact;
			Torque += Cross(Offset, Contact);
		}
	}

	void ChassisSim::Step(double Dt, const DriverInput& Input, const GroundQuery& Ground)
	{
		if (!(Dt > 0.0))
		{
			return;
		}
		const double SubDt = 1.0 / StepHz;
		Accumulator = std::min(Accumulator + Dt, 0.25); // never spiral after a hitch
		DriverInput StepInput = Input;
		while (Accumulator >= SubDt)
		{
			Substep(SubDt, StepInput, Ground);
			StepInput.ShiftRequest = 0; // a shift request applies once
			Accumulator -= SubDt;
		}
	}

	void ChassisSim::Substep(double Dt, const DriverInput& Input, const GroundQuery& Ground)
	{
		using namespace detail_chassis;
		const Quat& Q = State.Orientation;
		const Vec3 BodyUp = Q.Up();
		const Vec3 Fwd = Q.Forward();
		const Vec3 Down = -BodyUp;

		Vec3 Force(0.0, 0.0, -Gravity * MassKg);
		Vec3 Torque;
		auto AddForceAt = [&](const Vec3& WorldPoint, const Vec3& Value)
		{
			Force += Value;
			Torque += Cross(WorldPoint - State.Position, Value);
		};

		// --- Suspension ---
		std::array<double, 4> Compression{};
		std::array<double, 4> CompressionVel{};
		std::array<bool, 4> Grounded{};
		std::array<Vec3, 4> MountWorld{};
		std::array<GroundHit, 4> Hits{};
		for (std::size_t Index = 0; Index < 4; ++Index)
		{
			ChassisWheel& Wheel = WheelList[Index];
			MountWorld[Index] = State.Position + Q.Rotate(Wheel.MountLocal);
			Hits[Index] = Ground.Cast(MountWorld[Index], Down, Wheel.RestLength + Wheel.Radius);
			double NewCompression = 0.0;
			if (Hits[Index].bHit)
			{
				NewCompression = Wheel.RestLength - (Hits[Index].Distance - Wheel.Radius);
			}
			const bool bTouching = Hits[Index].bHit && NewCompression > 0.0;
			NewCompression = std::max(0.0, NewCompression);
			Wheel.CompressionVel = bTouching && Wheel.bGrounded ? (NewCompression - Wheel.Compression) / Dt : 0.0;
			Wheel.Compression = NewCompression;
			Wheel.bGrounded = bTouching;
			Wheel.Extension = std::clamp(Wheel.RestLength - NewCompression, Wheel.RestLength - Wheel.Travel, Wheel.RestLength);
			Compression[Index] = std::min(NewCompression, Wheel.Travel);
			CompressionVel[Index] = Wheel.CompressionVel;
			Grounded[Index] = bTouching;
			if (bTouching)
			{
				Wheel.ContactPoint = Hits[Index].Point;
				Wheel.ContactNormal = Hits[Index].Normal;
				Wheel.SurfaceGrip = Hits[Index].Grip;
			}
		}
		const std::array<double, 4> Spring = VehicleDynamics::SuspensionForces(Suspension, Compression, CompressionVel, Grounded);

		std::array<WheelContact, 4> Contacts{};
		std::array<Vec3, 4> WheelFwd{};
		std::array<Vec3, 4> WheelRight{};
		for (std::size_t Index = 0; Index < 4; ++Index)
		{
			ChassisWheel& Wheel = WheelList[Index];
			if (!Grounded[Index])
			{
				Wheel.LoadN = 0.0;
				continue;
			}
			double Load = std::max(0.0, Spring[Index]);
			const double Overtravel = Wheel.Compression - Wheel.Travel;
			if (Overtravel > 0.0)
			{
				// Bump stop: very stiff and well damped.
				const double Stiff = 12.0 * (Index < 2 ? Suspension.SpringFront : Suspension.SpringRear);
				Load += Stiff * Overtravel + 0.3 * std::sqrt(Stiff * MassKg * 0.25) * std::max(0.0, Wheel.CompressionVel);
			}
			Wheel.LoadN = Load;
			AddForceAt(MountWorld[Index], BodyUp * Load);

			// Wheel axes on the contact surface.
			Wheel.SteerAngle = Dyn.SteerAngleRad(static_cast<int>(Index), Input.Steer);
			const Vec3 Steered = Quat::FromAxisAngle(BodyUp, Wheel.SteerAngle).Rotate(Fwd);
			const Vec3 N = Wheel.ContactNormal;
			Vec3 F = ProjectOnPlane(Steered, N).Normalized();
			if (F.LengthSquared() < 0.5)
			{
				F = Steered;
			}
			const Vec3 R = Cross(N, F);
			WheelFwd[Index] = F;
			WheelRight[Index] = R;
			const Vec3 PatchVel = PointVelocity(Wheel.ContactPoint);
			Contacts[Index].bGrounded = true;
			Contacts[Index].LoadN = Load;
			Contacts[Index].VelLongMs = Dot(PatchVel, F);
			Contacts[Index].VelLatMs = Dot(PatchVel, R);
			Contacts[Index].SurfaceGrip = Wheel.SurfaceGrip;
		}

		// --- Powertrain and tires ---
		const std::array<WheelOutput, 4> Tire = Dyn.Step(Dt, Input, Contacts);
		const DynamicsTelemetry& Telem = Dyn.Telemetry();
		for (std::size_t Index = 0; Index < 4; ++Index)
		{
			ChassisWheel& Wheel = WheelList[Index];
			Wheel.SpinAngle = std::fmod(Wheel.SpinAngle + Telem.WheelOmega[Index] * Dt, TwoPi);
			if (!Grounded[Index])
			{
				continue;
			}
			const double Rolling = -RollingResistance * Wheel.LoadN * std::tanh(Contacts[Index].VelLongMs / 0.5);
			const Vec3 ContactForce = WheelFwd[Index] * (Tire[Index].Fx + Rolling) + WheelRight[Index] * Tire[Index].Fy;
			AddForceAt(Wheel.ContactPoint, ContactForce);
		}

		// --- Body against the ground: only needed when something unusual is happening ---
		bool bBottomedOut = false;
		for (const ChassisWheel& Wheel : WheelList)
		{
			bBottomedOut = bBottomedOut || Wheel.Compression > Wheel.Travel;
		}
		if (BodyUp.Z < 0.75 || CurrentAir > 0.0 || bBottomedOut)
		{
			BodyContacts(Ground, Force, Torque);
		}

		// --- Aero ---
		const double Speed = State.Velocity.Length();
		double Drag = 0.0;
		double DownFront = 0.0;
		double DownRear = 0.0;
		VehicleDynamics::AeroForces(Aero, Speed, Drag, DownFront, DownRear);
		if (Speed > 0.01)
		{
			Force -= State.Velocity / Speed * Drag;
		}
		AddForceAt(State.Position + Fwd * FrontAxleX, Down * DownFront);
		AddForceAt(State.Position + Fwd * RearAxleX, Down * DownRear);

		// --- Integrate ---
		State.Velocity += Force * (Dt / MassKg);
		const Vec3 OmegaLocal = Q.Unrotate(State.AngularVelocity);
		const Vec3 Momentum(InertiaBody.X * OmegaLocal.X, InertiaBody.Y * OmegaLocal.Y, InertiaBody.Z * OmegaLocal.Z);
		const Vec3 TorqueLocal = Q.Unrotate(Torque) - Cross(OmegaLocal, Momentum);
		const Vec3 AlphaLocal(TorqueLocal.X / InertiaBody.X, TorqueLocal.Y / InertiaBody.Y, TorqueLocal.Z / InertiaBody.Z);
		State.AngularVelocity = Q.Rotate(OmegaLocal + AlphaLocal * Dt);
		// Light angular damping (air, tire hysteresis) keeps tumbling finite.
		State.AngularVelocity *= 1.0 - 0.15 * Dt;

		State.Position += State.Velocity * Dt;
		const double Spin = State.AngularVelocity.Length();
		if (Spin > 1e-9)
		{
			State.Orientation = Quat::FromAxisAngle(State.AngularVelocity / Spin, Spin * Dt) * State.Orientation;
			State.Orientation.Normalize();
		}

		// --- Air time and flips ---
		int Touching = 0;
		for (const bool bWheel : Grounded)
		{
			Touching += bWheel ? 1 : 0;
		}
		if (Touching == 0)
		{
			CurrentAir += Dt;
			const Vec3 Local = State.Orientation.Unrotate(State.AngularVelocity);
			AirPitch += Local.Y * Dt / TwoPi;
			AirRoll += Local.X * Dt / TwoPi;
		}
		else if (CurrentAir > 0.0)
		{
			if (CurrentAir > 0.25)
			{
				LastAirRecord.AirTime = CurrentAir;
				LastAirRecord.PitchTurns = AirPitch;
				LastAirRecord.RollTurns = AirRoll;
				++LastAirRecord.Landings;
			}
			CurrentAir = 0.0;
			AirPitch = 0.0;
			AirRoll = 0.0;
		}

		if (!State.Velocity.IsFinite() || !State.AngularVelocity.IsFinite() || !State.Position.IsFinite())
		{
			// Never let a numerical blow-up escape into the game.
			State.Velocity = Vec3();
			State.AngularVelocity = Vec3();
			State.Orientation = Quat::FromYaw(0.0);
		}
	}
}
