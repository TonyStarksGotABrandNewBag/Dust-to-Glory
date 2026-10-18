#include "TestFramework.h"
#include "TestSupport.h"

#include <cmath>
#include <set>

using namespace dtg::sim;

DTG_TEST(CatalogHasEveryGenerationOfEveryBrand)
{
	const GameState& State = dtg_test::LoadedState();
	const VehicleCatalog& Catalog = State.Vehicles;
	EXPECT(Catalog.Brands().size() >= 15);
	EXPECT(Catalog.Models().size() >= 200);
	std::set<std::string> Ids;
	for (const VehicleModel& Model : Catalog.Models())
	{
		EXPECT_MSG(Ids.insert(Model.Id).second, "duplicate id " + Model.Id);
		EXPECT(Model.PowerKw > 10.0);
		EXPECT(Model.MarketPrice > 0);
		EXPECT(Model.YearEnd >= Model.YearStart);
		EXPECT(Model.RedlineRpm > Model.PowerRpm * 0.99);
		EXPECT(!Model.EngineLabel.empty());
	}
	for (const Brand& Maker : Catalog.Brands())
	{
		EXPECT_MSG(!Catalog.ModelsForBrand(Maker.Id, true).empty(), Maker.Id);
	}
	// Defunct brands stop producing.
	for (const VehicleModel& Model : Catalog.Models())
	{
		if (Model.BrandId == "packwell")
		{
			EXPECT(Model.YearEnd <= 1958);
		}
	}
}

DTG_TEST(CatalogEngineCharacter)
{
	const VehicleCatalog& Catalog = dtg_test::LoadedState().Vehicles;
	bool bRotary = false;
	bool bFlat6 = false;
	bool bV12 = false;
	bool bElectric = false;
	for (const VehicleModel& Model : Catalog.Models())
	{
		bRotary = bRotary || (Model.BrandId == "hayate" && Model.Layout == EngineLayout::Rotary);
		bFlat6 = bFlat6 || (Model.BrandId == "kessler" && Model.Layout == EngineLayout::Flat6);
		bV12 = bV12 || (Model.BrandId == "montecara" && Model.Layout == EngineLayout::V12);
		bElectric = bElectric || (Model.BrandId == "vanta" && Model.bElectric);
		if (Model.Body == BodyStyle::Muscle && Model.YearStart >= 1964 && Model.YearStart <= 1974 && !Model.bElectric)
		{
			EXPECT_MSG(Model.Layout == EngineLayout::V8Crossplane, Model.DisplayName + " " + Model.EngineLabel);
		}
	}
	EXPECT(bRotary);
	EXPECT(bFlat6);
	EXPECT(bV12);
	EXPECT(bElectric);
}

DTG_TEST(CatalogPricesFeelLikeTheNineties)
{
	const VehicleCatalog& Catalog = dtg_test::LoadedState().Vehicles;
	double Total = 0.0;
	int Count = 0;
	for (const VehicleModel& Model : Catalog.Models())
	{
		if (Model.Body == BodyStyle::Sedan && Model.YearEnd >= Catalog.CurrentYear() && !Model.bElectric)
		{
			Total += ToDollars(Model.MarketPrice);
			++Count;
		}
	}
	EXPECT(Count > 0);
	const double Average = Total / std::max(1, Count);
	EXPECT_MSG(Average > 10000.0 && Average < 30000.0, std::to_string(Average));
}

DTG_TEST(PartsLoadAndChangePerformance)
{
	const GameState& State = dtg_test::LoadedState();
	EXPECT(State.Parts.All().size() >= 50);
	const VehicleModel* Model = nullptr;
	for (const VehicleModel& Candidate : State.Vehicles.Models())
	{
		if (Candidate.BrandId == "takumi" && Candidate.Body == BodyStyle::Sedan && !Candidate.bElectric && Candidate.Induction == Aspiration::Natural)
		{
			Model = &Candidate;
			break;
		}
	}
	EXPECT(Model != nullptr);
	if (!Model) return;
	VehicleBuild Stock;
	const VehicleSetup Base = BuildSetup(*Model, Stock, State.Parts);
	VehicleBuild Tuned;
	Tuned.Install(State.Parts, *State.Parts.Find("turbo-medium"));
	Tuned.Install(State.Parts, *State.Parts.Find("exhaust-straight"));
	Tuned.Install(State.Parts, *State.Parts.Find("susp-coilovers"));
	Tuned.Install(State.Parts, *State.Parts.Find("wing-gt"));
	Tuned.Install(State.Parts, *State.Parts.Find("widebody"));
	const VehicleSetup Hot = BuildSetup(*Model, Tuned, State.Parts);
	EXPECT(Hot.Engine.PeakPowerKw > Base.Engine.PeakPowerKw * 1.4);
	EXPECT(Hot.Engine.Induction == Aspiration::Turbo);
	EXPECT(Hot.Aero.DownforceRear > Base.Aero.DownforceRear);
	EXPECT(Hot.TrackFrontM > Base.TrackFrontM);
	EXPECT(Hot.Sound.ExhaustTone == 2);
	EXPECT(PerformanceRating(Hot) > PerformanceRating(Base));

	// Turbo and supercharger are mutually exclusive.
	Tuned.Install(State.Parts, *State.Parts.Find("super-roots"));
	EXPECT(Tuned.Installed(State.Parts, PartCategory::Turbo) == nullptr);
	EXPECT(BuildSetup(*Model, Tuned, State.Parts).Engine.Induction == Aspiration::Supercharged);

	// Engine swap changes the engine and its sound.
	VehicleBuild Swapped;
	Swapped.Install(State.Parts, *State.Parts.Find("swap-ardent-v8"));
	const VehicleSetup V8 = BuildSetup(*Model, Swapped, State.Parts);
	EXPECT(V8.Engine.Layout == EngineLayout::V8Crossplane);
	EXPECT(V8.Sound.Cylinders == 8);

	// The tuning sheet only applies when adjustable parts are fitted.
	VehicleBuild Sheet = Stock;
	Sheet.Tuning.RideHeightFrontMm = -50.0;
	EXPECT_NEAR(BuildSetup(*Model, Sheet, State.Parts).Suspension.RideHeightFrontM, Base.Suspension.RideHeightFrontM, 1e-9);
	Sheet.Install(State.Parts, *State.Parts.Find("susp-coilovers"));
	EXPECT(BuildSetup(*Model, Sheet, State.Parts).Suspension.RideHeightFrontM < Base.Suspension.RideHeightFrontM);
}

DTG_TEST(TireModelShape)
{
	// Symmetric and peaking near the designed slip.
	const TireForce Plus = ComputeTireForce(1.0, 1.0, 0.1, 0.0, 4000.0, 4000.0, 1.0, 1.0);
	const TireForce Minus = ComputeTireForce(1.0, 1.0, -0.1, 0.0, 4000.0, 4000.0, 1.0, 1.0);
	EXPECT_NEAR(Plus.Fx, -Minus.Fx, 1e-6);
	EXPECT(Plus.Fx > 3500.0 && Plus.Fx <= 4100.0);
	const TireForce Sliding = ComputeTireForce(1.0, 1.0, 1.0, 0.0, 4000.0, 4000.0, 1.0, 1.0);
	EXPECT(Sliding.Fx < Plus.Fx && Sliding.Fx > 0.6 * Plus.Fx);
	// Lateral force opposes the slide.
	const TireForce Side = ComputeTireForce(1.0, 1.0, 0.0, 0.1, 4000.0, 4000.0, 1.0, 1.0);
	EXPECT(Side.Fy < -2000.0);
	// Combined slip: braking hard leaves less grip for cornering.
	const TireForce Combined = ComputeTireForce(1.0, 1.0, -0.15, 0.1, 4000.0, 4000.0, 1.0, 1.0);
	EXPECT(std::fabs(Combined.Fy) < std::fabs(Side.Fy));
}

DTG_TEST(TorqueCurveMatchesRatedPower)
{
	EngineSetup Engine;
	Engine.Layout = EngineLayout::I4;
	Engine.PeakPowerKw = 150.0;
	Engine.PowerRpm = 6500.0;
	Engine.PeakTorqueNm = 260.0;
	Engine.TorqueRpm = 4500.0;
	Engine.RedlineRpm = 7200.0;
	const TorqueCurve Curve = TorqueCurve::FromEngine(Engine);
	double PeakRpm = 0.0;
	EXPECT_NEAR(Curve.PeakPowerKw(&PeakRpm), 150.0, 0.5);
	EXPECT(PeakRpm > 5500.0 && PeakRpm < 7300.0);
	EXPECT(Curve.TorqueAt(Engine.RedlineRpm + 400.0) == 0.0);
	const std::vector<DynoPoint> Dyno = RunDyno(Engine);
	EXPECT(Dyno.size() > 20);
}

DTG_TEST(LaunchTimesTrackSpecSheets)
{
	const GameState& State = dtg_test::LoadedState();
	int Checked = 0;
	for (const VehicleModel& Model : State.Vehicles.Models())
	{
		if (Model.bEventOnly || Model.Body == BodyStyle::Bus || Model.Body == BodyStyle::Truck || Model.YearStart < 1960)
		{
			continue;
		}
		if (++Checked % 7 != 0)
		{
			continue; // sample the catalog to keep the test fast
		}
		const VehicleSetup Setup = BuildSetup(Model, VehicleBuild(), State.Parts);
		const LaunchResult Result = SimulateLaunch(Setup, 30.0);
		EXPECT_MSG(Result.ZeroTo100S > 0.0, Model.DisplayName + " never reached 100 km/h");
		if (Result.ZeroTo100S > 0.0)
		{
			const double Ratio = Result.ZeroTo100S / Model.ZeroTo100S;
			EXPECT_MSG(Ratio > 0.6 && Ratio < 1.9, Model.DisplayName + " sim " + std::to_string(Result.ZeroTo100S) + " spec " + std::to_string(Model.ZeroTo100S));
		}
	}
	EXPECT(Checked > 50);
}

DTG_TEST(MorePowerIsFaster)
{
	const GameState& State = dtg_test::LoadedState();
	const VehicleModel* Model = nullptr;
	for (const VehicleModel& Candidate : State.Vehicles.Models())
	{
		if (Candidate.BrandId == "ardent" && Candidate.Body == BodyStyle::Muscle && Candidate.YearStart >= 2005)
		{
			Model = &Candidate;
			break;
		}
	}
	EXPECT(Model != nullptr);
	if (!Model) return;
	VehicleBuild Build;
	const LaunchResult Stock = SimulateLaunch(BuildSetup(*Model, Build, State.Parts), 30.0);
	Build.Install(State.Parts, *State.Parts.Find("super-twinscrew"));
	Build.Install(State.Parts, *State.Parts.Find("tires-drag"));
	Build.Install(State.Parts, *State.Parts.Find("clutch-twin"));
	const LaunchResult Built = SimulateLaunch(BuildSetup(*Model, Build, State.Parts), 30.0);
	EXPECT(Built.QuarterMileS > 0.0 && Built.QuarterMileS < Stock.QuarterMileS);
}

DTG_TEST(BrakesStopTheCar)
{
	const GameState& State = dtg_test::LoadedState();
	const VehicleModel* Model = State.Vehicles.NewestOfBody(BodyStyle::Sedan);
	EXPECT(Model != nullptr);
	if (!Model) return;
	const VehicleSetup Setup = BuildSetup(*Model, VehicleBuild(), State.Parts);
	VehicleDynamics Dyn(Setup);
	double Speed = 30.0;
	double Distance = 0.0;
	DriverInput Input;
	Input.Brake = 1.0;
	for (int Step = 0; Step < 240 * 10 && Speed > 0.05; ++Step)
	{
		std::array<WheelContact, 4> Contacts{};
		for (std::size_t W = 0; W < 4; ++W)
		{
			Contacts[W].bGrounded = true;
			Contacts[W].LoadN = Setup.MassKg * 9.81 * 0.25;
			Contacts[W].VelLongMs = Speed;
		}
		const auto Out = Dyn.Step(1.0 / 240.0, Input, Contacts);
		double Force = 0.0;
		for (const WheelOutput& Wheel : Out) Force += Wheel.Fx;
		Speed = std::max(0.0, Speed + Force / Setup.MassKg / 240.0);
		Distance += Speed / 240.0;
	}
	// 108 km/h to 0 in well under 70 m on a modern sedan.
	EXPECT_MSG(Distance > 25.0 && Distance < 70.0, std::to_string(Distance));
}

DTG_TEST(LockedDiffTurnsWheelsTogether)
{
	const GameState& State = dtg_test::LoadedState();
	const VehicleModel* Model = State.Vehicles.NewestOfBody(BodyStyle::Muscle);
	EXPECT(Model != nullptr);
	if (!Model) return;
	VehicleBuild Build;
	Build.Install(State.Parts, *State.Parts.Find("diff-spool"));
	VehicleDynamics Dyn(BuildSetup(*Model, Build, State.Parts));
	DriverInput Input;
	Input.Throttle = 1.0;
	std::array<WheelContact, 4> Contacts{};
	for (std::size_t W = 0; W < 4; ++W)
	{
		Contacts[W].bGrounded = true;
		Contacts[W].LoadN = 4000.0;
	}
	Contacts[2].LoadN = 300.0; // one rear wheel nearly unloaded
	for (int Step = 0; Step < 120; ++Step) Dyn.Step(1.0 / 120.0, Input, Contacts);
	EXPECT_NEAR(Dyn.Telemetry().WheelOmega[2], Dyn.Telemetry().WheelOmega[3], 1e-6);
}

DTG_TEST(EngineSoundIsBoundedAndCharacterful)
{
	auto Render = [](EngineLayout Layout, double Rpm)
	{
		SoundProfile Profile;
		Profile.Layout = Layout;
		Profile.Cylinders = CylinderCount(Layout);
		Profile.DisplacementL = 4.0;
		Profile.ExhaustTone = 1;
		Profile.bElectric = Layout == EngineLayout::Electric;
		EngineSynth Synth;
		Synth.Configure(Profile, 48000.0, 8000.0);
		std::vector<float> Samples(48000);
		EngineSoundInput Input;
		Input.Rpm = Rpm;
		Input.Throttle = 1.0;
		Input.Load = 1.0;
		for (std::size_t Offset = 0; Offset < Samples.size(); Offset += 480)
		{
			Synth.Render(Samples.data() + Offset, 480, Input);
		}
		return std::vector<float>(Samples.begin() + 12000, Samples.end());
	};
	auto Autocorrelation = [](const std::vector<float>& X, std::size_t Lag)
	{
		double Sum = 0.0;
		double Energy = 0.0;
		for (std::size_t I = 0; I + Lag < X.size(); ++I)
		{
			Sum += static_cast<double>(X[I]) * X[I + Lag];
			Energy += static_cast<double>(X[I]) * X[I];
		}
		return Sum / std::max(1e-12, Energy);
	};
	for (const EngineLayout Layout : {EngineLayout::I4, EngineLayout::V8Crossplane, EngineLayout::Rotary, EngineLayout::Electric})
	{
		for (const float Sample : Render(Layout, 5000.0))
		{
			if (!(Sample >= -1.0f && Sample <= 1.0f))
			{
				EXPECT_MSG(false, std::string("out of range for ") + EngineLayoutName(Layout));
				break;
			}
		}
	}
	// Engine orders: order 1 is once per crank revolution (50 Hz at 3000 rpm). Each bank of a flat-plane V8
	// fires evenly every 180 degrees, so its sound lives on even orders (2, 4, ...). A cross-plane V8's banks
	// fire at uneven 90/180/270-degree gaps, which puts energy on the half and odd orders: that is the burble.
	auto Goertzel = [](const std::vector<float>& X, double Frequency)
	{
		const double Coefficient = 2.0 * std::cos(2.0 * 3.14159265358979 * Frequency / 48000.0);
		double S1 = 0.0;
		double S2 = 0.0;
		for (const float Value : X)
		{
			const double S0 = Value + Coefficient * S1 - S2;
			S2 = S1;
			S1 = S0;
		}
		return (S1 * S1 + S2 * S2 - Coefficient * S1 * S2) / static_cast<double>(X.size());
	};
	auto OddOrderShare = [&](EngineLayout Layout)
	{
		const std::vector<float> X = Render(Layout, 3000.0);
		double Odd = 0.0;
		for (const double Order : {0.5, 1.0, 1.5, 2.5, 3.0, 3.5})
		{
			Odd += Goertzel(X, 50.0 * Order);
		}
		const double Even = Goertzel(X, 100.0) + Goertzel(X, 200.0);
		return Odd / std::max(1e-12, Even);
	};
	const double Flat = OddOrderShare(EngineLayout::V8Flatplane);
	const double Cross = OddOrderShare(EngineLayout::V8Crossplane);
	EXPECT_MSG(Flat < 0.02 && Cross > 0.2, std::to_string(Flat) + " vs " + std::to_string(Cross));
	// An inline-four at 3000 rpm repeats every 10 ms.
	EXPECT(Autocorrelation(Render(EngineLayout::I4, 3000.0), 480) > 0.5);
}

DTG_TEST(UsedCarsHaveFaultsAndFlip)
{
	GameState State = dtg_test::FreshGame(7);
	EXPECT(State.UsedCars.Listings().size() == 10);
	State.Funds.Deposit(Dollars(200000), MoneyCategory::Other, "test", 0);
	State.Player.AddSkillXp(Skill::Trades, 2500);
	const UsedListing* Pick = nullptr;
	for (const UsedListing& Listing : State.UsedCars.Listings())
	{
		if (!Listing.Issues.empty() && (!Pick || Listing.Issues.size() > Pick->Issues.size()))
		{
			Pick = &Listing;
		}
	}
	EXPECT(Pick != nullptr);
	if (!Pick) return;
	const std::string ListingId = Pick->Id;
	const Money Asking = Pick->AskingPrice;
	const HaggleResult Lowball = State.HaggleUsed(ListingId, Asking / 4);
	EXPECT(Lowball.Outcome == HaggleOutcome::WalkedAway);
	const HaggleResult Fair = State.HaggleUsed(ListingId, Asking * 85 / 100);
	EXPECT(Fair.Outcome != HaggleOutcome::WalkedAway);
	EXPECT(State.BuyUsed(ListingId, Fair.Price).bOk);
	OwnedVehicle* Car = State.FindOwned(State.ActiveVehicleUid);
	EXPECT(Car != nullptr);
	if (!Car) return;
	const VehicleModel* Model = State.Vehicles.FindModel(Car->ModelId);
	const Money Before = ResaleValue(*Model, *Car);
	const std::vector<CarIssue> Issues = Car->Issues;
	for (const CarIssue Issue : Issues)
	{
		for (int Attempt = 0; Attempt < 6 && State.FindOwned(State.ActiveVehicleUid)->Has(Issue); ++Attempt)
		{
			State.RepairDiy(State.ActiveVehicleUid, Issue, 1.0);
		}
	}
	Car = State.FindOwned(State.ActiveVehicleUid);
	EXPECT(Car->Issues.empty());
	EXPECT(ResaleValue(*Model, *Car) > Before);
}

DTG_TEST(FaultsHurtDriving)
{
	const GameState& State = dtg_test::LoadedState();
	const VehicleModel* Model = State.Vehicles.NewestOfBody(BodyStyle::Coupe);
	EXPECT(Model != nullptr);
	if (!Model) return;
	OwnedVehicle Car;
	Car.ModelId = Model->Id;
	const VehicleSetup Healthy = State.SetupForOwned(Car);
	Car.Issues = {CarIssue::WornEngine, CarIssue::WornBrakes, CarIssue::BaldTires};
	const VehicleSetup Worn = State.SetupForOwned(Car);
	EXPECT(Worn.Engine.PeakPowerKw < Healthy.Engine.PeakPowerKw);
	EXPECT(Worn.Brakes.MaxTorqueFront < Healthy.Brakes.MaxTorqueFront);
	EXPECT(Worn.FrontTire.Mu < Healthy.FrontTire.Mu);
}
