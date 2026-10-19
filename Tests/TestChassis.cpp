#include "TestFramework.h"
#include "TestSupport.h"

#include "Sim/ChassisSim.h"

#include <cmath>
#include <limits>

using namespace dtg::sim;

namespace detail_chassis_test
{
	ChassisSim MakeCar(BodyStyle Body, const VehicleModel** OutModel = nullptr)
	{
		const GameState& State = dtg_test::LoadedState();
		const VehicleModel* Model = State.Vehicles.NewestOfBody(Body);
		ChassisSim Car;
		if (!Model)
		{
			return Car;
		}
		if (OutModel)
		{
			*OutModel = Model;
		}
		Car.Configure(State.SetupFor(Model->Id), Model->LengthM, Model->WidthM, Model->HeightM);
		Car.Teleport(Vec3(), 0.0);
		return Car;
	}

	/** Flat ground that returns a hit with a NaN contact point for a few casts, then behaves. */
	class PoisonedGround : public GroundQuery
	{
	public:
		explicit PoisonedGround(int InBadCasts) : BadCasts(InBadCasts) {}

		GroundHit Cast(const Vec3& Origin, const Vec3& Direction, double MaxDistance) const override
		{
			GroundHit Hit = Flat.Cast(Origin, Direction, MaxDistance);
			if (Hit.bHit && BadCasts > 0)
			{
				--BadCasts;
				const double Nan = std::numeric_limits<double>::quiet_NaN();
				Hit.Point = Vec3(Nan, Nan, Nan);
			}
			return Hit;
		}

	private:
		FlatGround Flat;
		mutable int BadCasts;
	};

	void Run(ChassisSim& Car, const GroundQuery& Ground, const DriverInput& Input, double Seconds)
	{
		const double Frame = 1.0 / 60.0;
		for (double Time = 0.0; Time < Seconds; Time += Frame)
		{
			Car.Step(Frame, Input, Ground);
		}
	}
}

DTG_TEST(ChassisSettlesOnItsSprings)
{
	using namespace detail_chassis_test;
	const FlatGround Ground;
	for (const BodyStyle Body : {BodyStyle::Sedan, BodyStyle::Supercar, BodyStyle::Bus, BodyStyle::MonsterTruck, BodyStyle::OpenWheel})
	{
		ChassisSim Car = MakeCar(Body);
		Car.State.Position.Z += 0.15; // drop it from a little above rest
		DriverInput Parked;
		Parked.Handbrake = 1.0; // automatics creep at idle, like the real thing (and held brake means reverse)
		Run(Car, Ground, Parked, 4.0);
		const std::string Name = BodyStyleName(Body);
		EXPECT_MSG(Car.GroundedWheels() == 4, Name);
		EXPECT_MSG(Car.State.Velocity.Length() < 0.05, Name + " still moving at " + std::to_string(Car.State.Velocity.Length()));
		EXPECT_MSG(Car.Up().Z > 0.995, Name + " not level");
		// Rest height: CG close to its design height above the road.
		EXPECT_MSG(std::fabs(Car.State.Position.Z - Car.CgHeight()) < 0.08,
			Name + " CG at " + std::to_string(Car.State.Position.Z) + " vs " + std::to_string(Car.CgHeight()));
		double Total = 0.0;
		for (const ChassisWheel& Wheel : Car.Wheels()) Total += Wheel.LoadN;
		EXPECT_MSG(std::fabs(Total - Car.Mass() * 9.81) < Car.Mass() * 9.81 * 0.03, Name + " loads don't carry the weight");
	}
}

DTG_TEST(ChassisDrivesStraightAndFast)
{
	using namespace detail_chassis_test;
	const FlatGround Ground;
	const VehicleModel* Model = nullptr;
	ChassisSim Car = MakeCar(BodyStyle::Sedan, &Model);
	Run(Car, Ground, DriverInput(), 1.0);
	DriverInput Input;
	Input.Throttle = 1.0;
	double Time = 0.0;
	double ZeroTo100 = 0.0;
	while (Time < 25.0 && ZeroTo100 == 0.0)
	{
		Car.Step(1.0 / 60.0, Input, Ground);
		Time += 1.0 / 60.0;
		if (Car.SpeedKph() >= 100.0) ZeroTo100 = Time;
	}
	EXPECT(ZeroTo100 > 0.0);
	// Same powertrain as the spec-sheet launch simulation, now with a real body: within 40%.
	const LaunchResult Spec = SimulateLaunch(dtg_test::LoadedState().SetupFor(Model->Id));
	EXPECT_MSG(ZeroTo100 < Spec.ZeroTo100S * 1.4 && ZeroTo100 > Spec.ZeroTo100S * 0.7,
		std::to_string(ZeroTo100) + " vs " + std::to_string(Spec.ZeroTo100S));
	EXPECT(std::fabs(Car.State.Position.Y) < 1.0);   // no wandering
	EXPECT(std::fabs(Car.State.Orientation.Yaw()) < 0.05);
	EXPECT(Car.Up().Z > 0.98);
}

DTG_TEST(ChassisTurnsWithoutRollingOver)
{
	using namespace detail_chassis_test;
	const FlatGround Ground;
	for (const BodyStyle Body : {BodyStyle::Sedan, BodyStyle::Sports, BodyStyle::SUV})
	{
		ChassisSim Car = MakeCar(Body);
		Run(Car, Ground, DriverInput(), 1.0);
		Car.State.Velocity = Car.Forward() * 20.0;
		DriverInput Input;
		Input.Throttle = 0.35;
		Input.Steer = 0.6;
		double MinUp = 1.0;
		const double YawBefore = Car.State.Orientation.Yaw();
		for (int Frame = 0; Frame < 180; ++Frame)
		{
			Car.Step(1.0 / 60.0, Input, Ground);
			MinUp = std::min(MinUp, Car.Up().Z);
		}
		const std::string Name = BodyStyleName(Body);
		const double Turned = std::fabs(Car.State.Orientation.Yaw() - YawBefore);
		EXPECT_MSG(Turned > 0.5, Name + " turned only " + std::to_string(Turned));
		EXPECT_MSG(MinUp > 0.9, Name + " leaned to " + std::to_string(MinUp));
		EXPECT_MSG(Car.GroundedWheels() >= 3, Name);
		// Positive steer turns right (+Y).
		EXPECT_MSG(Car.State.Position.Y > 1.0, Name + " went the wrong way");
	}
}

DTG_TEST(ChassisBrakesToAStop)
{
	using namespace detail_chassis_test;
	const FlatGround Ground;
	ChassisSim Car = MakeCar(BodyStyle::Sedan);
	Run(Car, Ground, DriverInput(), 1.0);
	Car.State.Velocity = Car.Forward() * 27.0;
	const double StartX = Car.State.Position.X;
	DriverInput Input;
	Input.Brake = 1.0;
	double Time = 0.0;
	while (Time < 8.0 && Car.ForwardSpeed() > 0.3)
	{
		Car.Step(1.0 / 60.0, Input, Ground); // a held brake reverses once stopped, so stop measuring there
		Time += 1.0 / 60.0;
	}
	const double Distance = Car.State.Position.X - StartX;
	EXPECT(Car.ForwardSpeed() <= 0.3);
	EXPECT_MSG(Distance > 25.0 && Distance < 70.0, std::to_string(Distance));
	EXPECT(Car.Up().Z > 0.98);
}

DTG_TEST(ChassisLandsJumpsAndRecordsAir)
{
	using namespace detail_chassis_test;
	const FlatGround Ground;
	ChassisSim Car = MakeCar(BodyStyle::MonsterTruck);
	Run(Car, Ground, DriverInput(), 1.0);
	Car.State.Position.Z += 3.0;
	Car.State.Velocity = Vec3(10.0, 0.0, 6.0);
	Car.State.AngularVelocity = Car.Right() * -0.15; // slightly nose up
	Run(Car, Ground, DriverInput(), 6.0);
	EXPECT(Car.State.Position.IsFinite());
	EXPECT(Car.LastAir().Landings >= 1);
	EXPECT(Car.LastAir().Landings <= 3 && Car.LastAir().AirTime > 0.25);
	EXPECT(Car.State.Position.Z < 3.0 && Car.State.Position.Z > 0.0);
	EXPECT(Car.Up().Z > 0.9);
}

DTG_TEST(ChassisRollsOntoItsRoofWithoutFallingThrough)
{
	using namespace detail_chassis_test;
	const FlatGround Ground;
	ChassisSim Car = MakeCar(BodyStyle::Sedan);
	Car.State.Position.Z += 2.5;
	Car.State.Velocity = Vec3(8.0, 0.0, 4.0);
	Car.State.AngularVelocity = Car.Forward() * 4.0; // barrel roll
	Run(Car, Ground, DriverInput(), 8.0);
	EXPECT(Car.State.Position.IsFinite());
	EXPECT_MSG(Car.State.Position.Z > 0.2, std::to_string(Car.State.Position.Z));
	EXPECT(Car.State.Velocity.Length() < 4.0);
	EXPECT(std::fabs(Car.State.Velocity.Z) < 0.1);
	// Wherever it ended up (wheels or roof), it sits on the ground, not in it.
	EXPECT(Car.State.Position.Z < 2.0);
}

DTG_TEST(ChassisRecoversFromANumericalBlowUp)
{
	// The first run in the real engine sent one parked car's position to NaN, and it never came back.
	using namespace detail_chassis_test;
	ChassisSim Car = MakeCar(BodyStyle::Sedan);
	DriverInput Parked;
	Parked.Handbrake = 1.0;
	Run(Car, FlatGround(), Parked, 1.0);
	const Vec3 Rest = Car.State.Position;
	EXPECT(Car.NumericalResets() == 0);

	const PoisonedGround Ground(1);
	Run(Car, Ground, Parked, 2.0);
	EXPECT(Car.NumericalResets() >= 1);
	EXPECT(Car.State.Position.IsFinite());
	EXPECT(Car.State.Velocity.IsFinite());
	// Rolled back and settled again, not thrown somewhere else.
	EXPECT_MSG((Car.State.Position - Rest).Length() < 0.05, std::to_string((Car.State.Position - Rest).Length()));
	EXPECT(Car.GroundedWheels() == 4);
}

DTG_TEST(ChassisCollisionsBounceAndSpin)
{
	using namespace detail_chassis_test;
	ChassisSim Car = MakeCar(BodyStyle::Sedan);
	Car.State.Velocity = Vec3(15.0, 0.0, 0.0);
	// A wall ahead, hit on the front-right corner.
	const Vec3 Corner = Car.State.Position + Vec3(2.2, 0.8, 0.0);
	const double Impact = Car.ResolveContact(Corner, Vec3(-1.0, 0.0, 0.0), 0.2, 0.4);
	EXPECT_NEAR(Impact, 15.0, 1e-6);
	EXPECT(Car.State.Velocity.X < 0.0 || std::fabs(Car.State.Velocity.X) < 15.0 * 0.5);
	EXPECT(std::fabs(Car.State.AngularVelocity.Z) > 0.1);
	EXPECT(Car.ResolveContact(Corner, Vec3(-1.0, 0.0, 0.0), 0.2, 0.4) == 0.0 || Car.PointVelocity(Corner).X <= 1e-6);
}
