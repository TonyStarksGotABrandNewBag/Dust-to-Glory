#include "Sim/EngineAudio.h"

namespace dtg::sim
{
	namespace detail_audio
	{
		constexpr double Pi = 3.14159265358979;

		void SetCoefficients(Biquad& Filter, double B0, double B1, double B2, double A0, double A1, double A2)
		{
			Filter.B0 = B0 / A0;
			Filter.B1 = B1 / A0;
			Filter.B2 = B2 / A0;
			Filter.A1 = A1 / A0;
			Filter.A2 = A2 / A0;
		}

		double SafeFrequency(double SampleRate, double Frequency)
		{
			return Clamp(Frequency, 10.0, 0.45 * SampleRate);
		}

		/** Builds a schedule from a firing order (cylinder numbers) at even intervals. */
		std::vector<FiringEvent> FromOrder(const std::vector<int>& Order, double IntervalDeg, int SplitAt)
		{
			std::vector<FiringEvent> Events;
			for (std::size_t Index = 0; Index < Order.size(); ++Index)
			{
				FiringEvent Event;
				Event.AngleDeg = IntervalDeg * static_cast<double>(Index);
				Event.Bank = Order[Index] <= SplitAt ? 0 : 1;
				Events.push_back(Event);
			}
			return Events;
		}

		double Wrap(double Phase)
		{
			return Phase - std::floor(Phase);
		}

		double SineTurns(double Phase)
		{
			return std::sin(2.0 * Pi * Phase);
		}
	}

	void Biquad::SetLowPass(double SampleRate, double Frequency, double Q)
	{
		const double W0 = 2.0 * detail_audio::Pi * detail_audio::SafeFrequency(SampleRate, Frequency) / SampleRate;
		const double Alpha = std::sin(W0) / (2.0 * Q);
		const double Cos = std::cos(W0);
		detail_audio::SetCoefficients(*this, (1.0 - Cos) * 0.5, 1.0 - Cos, (1.0 - Cos) * 0.5, 1.0 + Alpha, -2.0 * Cos, 1.0 - Alpha);
	}

	void Biquad::SetBandPass(double SampleRate, double Frequency, double Q)
	{
		const double W0 = 2.0 * detail_audio::Pi * detail_audio::SafeFrequency(SampleRate, Frequency) / SampleRate;
		const double Alpha = std::sin(W0) / (2.0 * Q);
		const double Cos = std::cos(W0);
		detail_audio::SetCoefficients(*this, Alpha, 0.0, -Alpha, 1.0 + Alpha, -2.0 * Cos, 1.0 - Alpha);
	}

	void Biquad::SetHighPass(double SampleRate, double Frequency, double Q)
	{
		const double W0 = 2.0 * detail_audio::Pi * detail_audio::SafeFrequency(SampleRate, Frequency) / SampleRate;
		const double Alpha = std::sin(W0) / (2.0 * Q);
		const double Cos = std::cos(W0);
		detail_audio::SetCoefficients(*this, (1.0 + Cos) * 0.5, -(1.0 + Cos), (1.0 + Cos) * 0.5, 1.0 + Alpha, -2.0 * Cos, 1.0 - Alpha);
	}

	std::vector<FiringEvent> FiringSchedule(EngineLayout Layout)
	{
		using detail_audio::FromOrder;
		switch (Layout)
		{
		case EngineLayout::I3: return FromOrder({1, 2, 3}, 240.0, 3);
		case EngineLayout::I4: return FromOrder({1, 3, 4, 2}, 180.0, 4);
		case EngineLayout::I5: return FromOrder({1, 2, 4, 5, 3}, 144.0, 5);
		case EngineLayout::I6:
		case EngineLayout::DieselI6: return FromOrder({1, 5, 3, 6, 2, 4}, 120.0, 3);
		case EngineLayout::V6:
		{
			// Odd cylinders on one bank, even on the other.
			std::vector<FiringEvent> Events = FromOrder({1, 2, 3, 4, 5, 6}, 120.0, 0);
			for (std::size_t Index = 0; Index < Events.size(); ++Index)
			{
				Events[Index].Bank = static_cast<int>(Index % 2);
			}
			return Events;
		}
		case EngineLayout::V8Crossplane:
		{
			// 1-8-4-3-6-5-7-2: each bank fires at uneven 90/180/270-degree gaps -> the classic burble.
			const std::vector<int> Order = {1, 8, 4, 3, 6, 5, 7, 2};
			std::vector<FiringEvent> Events = FromOrder(Order, 90.0, 0);
			for (std::size_t Index = 0; Index < Events.size(); ++Index)
			{
				Events[Index].Bank = Order[Index] % 2 == 1 ? 0 : 1;
			}
			return Events;
		}
		case EngineLayout::V8Flatplane: return FromOrder({1, 5, 3, 7, 4, 8, 2, 6}, 90.0, 4);
		case EngineLayout::V10: return FromOrder({1, 6, 5, 10, 2, 7, 3, 8, 4, 9}, 72.0, 5);
		case EngineLayout::V12: return FromOrder({1, 7, 5, 11, 3, 9, 6, 12, 2, 8, 4, 10}, 60.0, 6);
		case EngineLayout::Flat4:
		{
			// Unequal-length headers delay one bank's pulses: the boxer rumble.
			std::vector<FiringEvent> Events = FromOrder({1, 3, 2, 4}, 180.0, 0);
			const int Banks[] = {0, 0, 1, 1};
			for (std::size_t Index = 0; Index < Events.size(); ++Index)
			{
				Events[Index].Bank = Banks[Index];
				Events[Index].HeaderDelayDeg = Banks[Index] == 1 ? 34.0 : 0.0;
			}
			return Events;
		}
		case EngineLayout::Flat6: return FromOrder({1, 6, 2, 4, 3, 5}, 120.0, 3);
		case EngineLayout::Rotary: return FromOrder({1, 2, 1, 2}, 180.0, 2);
		case EngineLayout::Electric:
		case EngineLayout::Count: break;
		}
		return {};
	}

	void EngineSynth::Configure(const SoundProfile& Profile, double InSampleRate, double RedlineRpm)
	{
		Sound = Profile;
		SampleRate = InSampleRate > 1000.0 ? InSampleRate : 48000.0;
		Redline = std::max(1500.0, RedlineRpm);
		Schedule = FiringSchedule(Profile.Layout);
		Pulses = {};
		CrankDeg = 0.0;
		NextEvent = 0;

		// Bigger displacement and quieter exhausts ring lower; straight pipes ring higher and brighter.
		const double PipeBase = Clamp(150.0 + 45.0 * Profile.ExhaustTone - 9.0 * Profile.DisplacementL, 70.0, 320.0);
		Header[0].SetBandPass(SampleRate, PipeBase, 3.5);
		Header[1].SetBandPass(SampleRate, PipeBase * 1.18, 3.5);
		BankDelay = {};
		DelayWrite = 0;
		DelaySamples = static_cast<std::size_t>(Clamp(0.0016 * SampleRate, 1.0, static_cast<double>(BankDelay.size() - 1)));
		const double MufflerCut = Profile.ExhaustTone >= 2 ? 4800.0 : (Profile.ExhaustTone == 1 ? 1900.0 : 800.0);
		Muffler.SetLowPass(SampleRate, MufflerCut, 0.707);
		MufflerSecond.SetLowPass(SampleRate, MufflerCut * 1.6, 0.707);
		HighCut.SetLowPass(SampleRate, 9000.0, 0.707);
		DcBlock.SetHighPass(SampleRate, 28.0, 0.707);
		Intake.SetBandPass(SampleRate, 500.0, 1.2);
		BlowOffFilter.SetBandPass(SampleRate, 2800.0, 1.4);
		PopFilter.SetBandPass(SampleRate, 1100.0, 0.8);

		PulseLife = Profile.Layout == EngineLayout::Rotary ? 0.012 : (Profile.Layout == EngineLayout::DieselI6 ? 0.010 : 0.008);
	}

	void EngineSynth::Trigger(int Bank, double Amplitude, double DelaySeconds)
	{
		// Reuse the oldest pulse slot.
		std::size_t Oldest = 0;
		for (std::size_t Index = 0; Index < Pulses.size(); ++Index)
		{
			if (Pulses[Index].Age > Pulses[Oldest].Age)
			{
				Oldest = Index;
			}
		}
		Pulses[Oldest].Age = -DelaySeconds;
		Pulses[Oldest].Amplitude = Amplitude;
		Pulses[Oldest].Bank = Bank;
	}

	void EngineSynth::Render(float* Out, int NumSamples, const EngineSoundInput& Input)
	{
		using namespace detail_audio;
		if (!Out || NumSamples <= 0)
		{
			return;
		}
		const double Dt = 1.0 / SampleRate;
		const double SmoothK = 1.0 - std::exp(-Dt / 0.02);
		const bool bElectric = Sound.bElectric || Schedule.empty();
		const double Tau = PulseLife / 5.0;
		const double Loudness = Clamp(Sound.ExhaustLoudness, 0.2, 3.0);

		// Blow-off valve: a sharp lift with boost in the system.
		if (Sound.bTurbo && Input.Throttle < 0.2 && LastThrottle > 0.6 && Input.Boost > 0.35)
		{
			BlowOff = Input.Boost;
		}
		LastThrottle = Input.Throttle;

		Intake.SetBandPass(SampleRate, 350.0 + 0.12 * SmoothRpm, 1.2);

		for (int SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			SmoothRpm += (std::max(0.0, Input.Rpm) - SmoothRpm) * SmoothK;
			SmoothLoad += (Clamp(Input.Load, 0.0, 1.0) - SmoothLoad) * SmoothK;
			SmoothThrottle += (Clamp(Input.Throttle, 0.0, 1.0) - SmoothThrottle) * SmoothK;
			const double RpmRatio = Clamp(SmoothRpm / Redline, 0.0, 1.2);
			double Mix = 0.0;

			if (bElectric)
			{
				// Motor: electrical frequency plus harmonics from the inverter.
				const double Fundamental = SmoothRpm / 60.0 * 4.0;
				const double Gains[3] = {1.0, 0.45, 0.2};
				const double Multiples[3] = {1.0, 2.0, 6.0};
				double Whine = 0.0;
				for (std::size_t Harmonic = 0; Harmonic < 3; ++Harmonic)
				{
					MotorPhase[Harmonic] = Wrap(MotorPhase[Harmonic] + Fundamental * Multiples[Harmonic] * Dt);
					Whine += Gains[Harmonic] * SineTurns(MotorPhase[Harmonic]);
				}
				Mix += Whine * (0.03 + 0.12 * SmoothLoad) * std::min(1.0, SmoothRpm / 800.0);
				Mix += Noise() * 0.004 * RpmRatio;
			}
			else
			{
				// Advance the crank and fire cylinders as their angles come around.
				const double Degrees = SmoothRpm / 60.0 * 360.0 * Dt;
				CrankDeg += Degrees;
				int Guard = 0;
				while (Guard++ < 16)
				{
					const FiringEvent& Event = Schedule[NextEvent];
					const double LopeJitter = Sound.CamLope * 14.0 * (1.0 - RpmRatio) * CycleJitter;
					if (CrankDeg < Event.AngleDeg + LopeJitter)
					{
						break;
					}
					const double DegToSeconds = SmoothRpm > 1.0 ? 60.0 / (SmoothRpm * 360.0) : 0.0;
					double Amplitude = (0.3 + 0.7 * SmoothLoad) * (0.65 + 0.35 * RpmRatio);
					// Lumpy cams make idle uneven; small combustion variation everywhere.
					Amplitude *= 1.0 + (0.08 + 0.5 * Sound.CamLope * (1.0 - RpmRatio)) * Noise();
					Trigger(Event.Bank, Amplitude, Event.HeaderDelayDeg * DegToSeconds);
					++NextEvent;
					if (NextEvent >= Schedule.size())
					{
						NextEvent = 0;
						CrankDeg -= 720.0;
						CycleJitter = Noise();
						++CurrentCycle;
					}
				}

				// Sum pulses per bank. Each pulse is a sharp rise and exponential decay with combustion noise.
				double BankSignal[2] = {0.0, 0.0};
				for (Pulse& P : Pulses)
				{
					if (P.Age >= PulseLife)
					{
						continue;
					}
					if (P.Age >= 0.0)
					{
						const double Envelope = std::exp(-P.Age / Tau) * (1.0 - std::exp(-P.Age / (Tau * 0.15)));
						double Shape = Envelope * (1.0 + 0.35 * Noise());
						if (Sound.Layout == EngineLayout::DieselI6 && P.Age < 0.0006)
						{
							Shape += 0.6 * Noise(); // diesel knock
						}
						if (Sound.Layout == EngineLayout::Rotary)
						{
							Shape += 0.25 * Envelope * Noise(); // raspy peripheral ports
						}
						BankSignal[P.Bank & 1] += P.Amplitude * Shape;
					}
					P.Age += Dt;
				}
				double Exhaust = 0.6 * BankSignal[0] + 1.6 * Header[0].Process(BankSignal[0]);
				const double SecondBank = 0.6 * BankSignal[1] + 1.6 * Header[1].Process(BankSignal[1]);
				BankDelay[DelayWrite] = SecondBank;
				const std::size_t ReadIndex = (DelayWrite + BankDelay.size() - DelaySamples) % BankDelay.size();
				Exhaust += BankDelay[ReadIndex];
				DelayWrite = (DelayWrite + 1) % BankDelay.size();
				Exhaust = MufflerSecond.Process(Muffler.Process(Exhaust));
				Mix += Exhaust * 0.9 * Loudness;

				// Induction roar.
				Mix += Intake.Process(Noise()) * Sound.IntakeLoudness * SmoothThrottle * RpmRatio * 0.35;

				// Overrun pops: after a pull, lifting at high rpm crackles through the exhaust.
				if (SmoothThrottle > 0.6)
				{
					HighThrottleTime = std::min(2.0, HighThrottleTime + Dt);
				}
				else
				{
					HighThrottleTime = std::max(0.0, HighThrottleTime - Dt * 0.6);
				}
				const bool bOverrun = SmoothThrottle < 0.15 && RpmRatio > 0.45 && HighThrottleTime > 0.3;
				const bool bLimiterPops = Input.bRevLimiter && Sound.Pops > 0.5;
				if ((bOverrun || bLimiterPops) && Random.Chance(Sound.Pops * 10.0 * Dt))
				{
					PopEnvelope = 0.6 + 0.4 * Random.NextDouble();
				}
				if (PopEnvelope > 0.001)
				{
					Mix += PopFilter.Process(Noise() * PopEnvelope) * 1.6 + PopEnvelope * 0.4 * Noise();
					PopEnvelope *= std::exp(-Dt / 0.012);
				}
			}

			// Forced induction.
			if (Sound.bTurbo)
			{
				const double Boost = Clamp(Input.Boost, 0.0, 1.0);
				WhistlePhase = Wrap(WhistlePhase + (1800.0 + 6500.0 * Boost) * Dt);
				Mix += SineTurns(WhistlePhase) * 0.03 * Boost * (0.3 + 0.7 * SmoothThrottle);
				if (BlowOff > 0.001)
				{
					double Valve = BlowOffFilter.Process(Noise()) * BlowOff * 0.5;
					if (Sound.ExhaustTone >= 1)
					{
						// Compressor flutter instead of a clean psssh.
						Valve *= 0.5 + 0.5 * SineTurns(static_cast<double>(SampleIndex) * Dt * 28.0 + BlowOff);
					}
					Mix += Valve;
					BlowOff *= std::exp(-Dt / 0.25);
				}
			}
			if (Sound.bSupercharger)
			{
				WhinePhase = Wrap(WhinePhase + SmoothRpm / 60.0 * 6.5 * Dt);
				Mix += SineTurns(WhinePhase) * (0.02 + 0.05 * SmoothThrottle * SmoothLoad);
			}
			if (Sound.bStraightCutGears && Input.SpeedMs > 0.5)
			{
				GearPhase = Wrap(GearPhase + std::min(3000.0, Input.SpeedMs * 38.0) * Dt);
				Mix += SineTurns(GearPhase) * 0.025 * std::min(1.0, Input.SpeedMs / 20.0);
			}

			// Exhaust pressure pulses are one-sided; remove the DC so speakers hear only the sound.
			const double Clipped = std::tanh(1.6 * HighCut.Process(DcBlock.Process(Mix))) * 0.8;
			Out[SampleIndex] = static_cast<float>(Clamp(Clipped, -1.0, 1.0));
		}
	}

	void RoadSynth::Configure(double InSampleRate)
	{
		SampleRate = InSampleRate > 1000.0 ? InSampleRate : 48000.0;
		Squeal.SetBandPass(SampleRate, 1150.0, 6.0);
		Roar.SetLowPass(SampleRate, 280.0, 0.707);
		Wind.SetBandPass(SampleRate, 650.0, 0.5);
	}

	void RoadSynth::Render(float* Out, int NumSamples, double SpeedMs, double MaxSlip, double SurfaceGrip)
	{
		if (!Out || NumSamples <= 0)
		{
			return;
		}
		const double Dt = 1.0 / SampleRate;
		const double SmoothK = 1.0 - std::exp(-Dt / 0.03);
		const bool bLoose = SurfaceGrip < 0.8;
		for (int Index = 0; Index < NumSamples; ++Index)
		{
			SmoothSlip += (std::fabs(MaxSlip) - SmoothSlip) * SmoothK;
			SmoothSpeed += (std::fabs(SpeedMs) - SmoothSpeed) * SmoothK;
			const double NoiseValue = Random.NextDouble() * 2.0 - 1.0;
			const double SquealAmount = bLoose ? 0.0 : Clamp((SmoothSlip - 0.12) * 2.0, 0.0, 1.0) * std::min(1.0, SmoothSpeed / 3.0);
			double Mix = Squeal.Process(NoiseValue) * SquealAmount * 1.2;
			Mix += Roar.Process(NoiseValue) * std::min(1.0, SmoothSpeed / 40.0) * (bLoose ? 0.5 : 0.25);
			Mix += Wind.Process(NoiseValue) * std::min(1.0, 0.0004 * SmoothSpeed * SmoothSpeed);
			Out[Index] = static_cast<float>(Clamp(std::tanh(Mix), -1.0, 1.0));
		}
	}
}
