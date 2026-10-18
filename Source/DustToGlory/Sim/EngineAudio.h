// Physically-inspired engine sound synthesis.
//
// Each cylinder firing launches an exhaust pressure pulse into its bank's header. The firing schedule
// comes from the real crank layout (a cross-plane V8's uneven per-bank firing is what makes the burble,
// a flat-plane V8 fires each bank evenly and screams), pulses ring through a pipe resonance, then a
// muffler filter whose brightness depends on the exhaust fitted. Turbo whistle, blow-off, supercharger
// whine, straight-cut gear whine, overrun pops and electric-motor whine are layered on top.
//
// Recorded RPM loops can replace or blend with this per engine family later; see docs/AUDIO.md.
#pragma once

#include "Sim/SimCommon.h"
#include "Sim/Tuning.h"

#include <array>
#include <cstddef>
#include <vector>

namespace dtg::sim
{
	struct EngineSoundInput
	{
		double Rpm = 800.0;
		double Throttle = 0.0;    // pedal 0..1
		double Load = 0.0;        // 0..1 torque being produced
		double Boost = 0.0;       // 0..1
		double SpeedMs = 0.0;
		bool bRevLimiter = false;
		bool bShifting = false;
	};

	/** Second-order filter (RBJ cookbook). */
	struct Biquad
	{
		double B0 = 1.0, B1 = 0.0, B2 = 0.0, A1 = 0.0, A2 = 0.0;
		double Z1 = 0.0, Z2 = 0.0;

		void SetLowPass(double SampleRate, double Frequency, double Q);
		void SetBandPass(double SampleRate, double Frequency, double Q);
		void SetHighPass(double SampleRate, double Frequency, double Q);
		double Process(double Input)
		{
			const double Output = B0 * Input + Z1;
			Z1 = B1 * Input - A1 * Output + Z2;
			Z2 = B2 * Input - A2 * Output;
			return Output;
		}
	};

	/** Firing angles (degrees in a 720-degree four-stroke cycle) and exhaust bank of each cylinder. */
	struct FiringEvent
	{
		double AngleDeg = 0.0;
		int Bank = 0;
		double HeaderDelayDeg = 0.0; // unequal-length headers (boxer rumble)
	};

	std::vector<FiringEvent> FiringSchedule(EngineLayout Layout);

	class EngineSynth
	{
	public:
		void Configure(const SoundProfile& Profile, double SampleRate, double RedlineRpm);
		/** Renders mono samples in -1..1. Real-time safe: no allocation. */
		void Render(float* Out, int NumSamples, const EngineSoundInput& Input);
		const SoundProfile& Profile() const { return Sound; }

	private:
		struct Pulse
		{
			double Age = 1.0;    // seconds since firing (>= Life means inactive)
			double Amplitude = 0.0;
			int Bank = 0;
		};

		static constexpr int MaxPulses = 32;

		void Trigger(int Bank, double Amplitude, double DelaySeconds);
		double Noise() { return Random.NextDouble() * 2.0 - 1.0; }

		SoundProfile Sound;
		double SampleRate = 48000.0;
		double Redline = 7000.0;
		std::vector<FiringEvent> Schedule;
		std::array<Pulse, MaxPulses> Pulses{};
		std::array<Biquad, 2> Header{};      // per-bank pipe resonance
		Biquad Muffler;
		Biquad MufflerSecond;
		Biquad Intake;
		Biquad BlowOffFilter;
		Biquad PopFilter;
		Biquad HighCut;
		Biquad DcBlock;
		// The second bank's pipe is longer: its pulses arrive later, so a cross-plane V8's uneven per-bank
		// firing is heard instead of merging back into an even beat.
		std::array<double, 2048> BankDelay{};
		std::size_t DelayWrite = 0;
		std::size_t DelaySamples = 64;
		Rng Random{0x51ED5EEDULL};
		double CrankDeg = 0.0;
		std::size_t NextEvent = 0;
		double SmoothRpm = 800.0;
		double SmoothLoad = 0.0;
		double SmoothThrottle = 0.0;
		double LastThrottle = 0.0;
		double HighThrottleTime = 0.0;
		double BlowOff = 0.0;
		double WhistlePhase = 0.0;
		double WhinePhase = 0.0;
		double GearPhase = 0.0;
		std::array<double, 3> MotorPhase{};
		double PopEnvelope = 0.0;
		double PulseLife = 0.008;
		double CycleJitter = 0.0;
		int CurrentCycle = 0;
	};

	/** Tire squeal, rolling roar and wind, driven by slip and speed. */
	class RoadSynth
	{
	public:
		void Configure(double InSampleRate);
		void Render(float* Out, int NumSamples, double SpeedMs, double MaxSlip, double SurfaceGrip);

	private:
		double SampleRate = 48000.0;
		Biquad Squeal;
		Biquad Roar;
		Biquad Wind;
		Rng Random{0xA11CE5ULL};
		double SmoothSlip = 0.0;
		double SmoothSpeed = 0.0;
	};
}
