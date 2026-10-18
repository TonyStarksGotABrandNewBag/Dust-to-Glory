// Motorsport events: drag racing, demolition derby, monster truck freestyle and open-wheel circuit racing.
#pragma once

#include "Sim/SimCommon.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	enum class EventKind : int
	{
		DragRace,
		DemolitionDerby,
		MonsterJam,
		Formula,
		Count
	};

	const char* EventKindName(EventKind Kind);

	/** Prize for a finishing position; tier 1..5 scales the purse. */
	Money EventPrize(EventKind Kind, int Tier, int Position, int FieldSize);

	// ------------------------------------------------------------------ Drag racing

	enum class TreeType : int
	{
		Sportsman, // ambers count down 0.5 s apart
		Pro        // all ambers at once, green 0.4 s later
	};

	/** The "Christmas tree" start lights, with red-light fouls and reaction times. */
	class DragTree
	{
	public:
		explicit DragTree(TreeType InType = TreeType::Sportsman) : Type(InType) {}

		/** Both cars staged; the tree starts after StageDelay seconds. */
		void Start(double StageDelay = 1.0);
		void Tick(double Dt);
		/** Number of amber bulbs lit (0..3). */
		int AmbersLit() const;
		bool IsGreen() const { return bStarted && Clock >= GreenAt; }
		/** Driver launches. Returns reaction time (negative = red light). */
		double Launch();
		bool IsFoul() const { return bLaunched && Reaction < 0.0; }
		bool HasLaunched() const { return bLaunched; }
		double ReactionTime() const { return Reaction; }
		double GreenTime() const { return GreenAt; }
		double Elapsed() const { return Clock; }

	private:
		TreeType Type;
		bool bStarted = false;
		bool bLaunched = false;
		double Clock = 0.0;
		double FirstAmberAt = 0.0;
		double GreenAt = 0.0;
		double Reaction = 0.0;
	};

	struct DragResult
	{
		double Reaction = 0.0;
		double ElapsedTime = 0.0; // ET from launch to the finish line
		double TrapSpeedKph = 0.0;
		bool bFoul = false;
	};

	/** True when A beats B: fouls lose, otherwise lowest reaction + ET wins (the first to the line). */
	bool DragWinner(const DragResult& A, const DragResult& B);

	// ------------------------------------------------------------------ Circuit racing

	/** Tracks checkpoints, laps and standings for any number of racers. */
	class RaceTracker
	{
	public:
		RaceTracker(int InCheckpoints, int InLaps, int InRacers);

		/** Records a checkpoint; only the racer's next expected checkpoint counts. Returns true if accepted. */
		bool PassCheckpoint(int Racer, int Checkpoint, double Time);
		int Lap(int Racer) const;
		int NextCheckpoint(int Racer) const;
		bool Finished(int Racer) const;
		double FinishTime(int Racer) const;
		double BestLap(int Racer) const;
		double LastLap(int Racer) const;
		/** Order of racers, leader first. DistanceToNext breaks ties within the same checkpoint. */
		std::vector<int> Standings(const std::vector<double>& DistanceToNext) const;
		int Position(int Racer, const std::vector<double>& DistanceToNext) const;
		int Laps() const { return LapCount; }
		int Checkpoints() const { return CheckpointCount; }
		int Racers() const { return static_cast<int>(State.size()); }
		int FastestLapRacer() const;

	private:
		struct RacerState
		{
			int Passed = 0;       // total checkpoints passed
			double LastPassTime = 0.0;
			double LapStart = 0.0;
			double BestLap = 0.0;
			double LastLap = 0.0;
			double FinishTime = 0.0;
			bool bFinished = false;
		};

		int CheckpointCount;
		int LapCount;
		std::vector<RacerState> State;
	};

	/** Championship points for a finishing position (25-18-15-12-10-8-6-4-2-1). */
	int FormulaPoints(int Position);

	// ------------------------------------------------------------------ Demolition derby

	struct DerbyScore
	{
		int Hits = 0;
		int Points = 0;
		bool bSurvived = true;
	};

	/** Points for one hit: harder hits count more; very slow bumps count for nothing. */
	int DerbyHitPoints(double ImpactSpeedKph);
	/** Final ranking: survivors first, then by points. Returns racer indices. */
	std::vector<int> DerbyRanking(const std::vector<DerbyScore>& Scores);

	// ------------------------------------------------------------------ Monster truck freestyle

	struct FreestyleRun
	{
		double AirtimeS = 0.0;
		int BigAirs = 0;       // jumps with > 1.5 s of air
		int Backflips = 0;
		int Frontflips = 0;
		int Rolls = 0;
		int Donuts = 0;
		double WheelieS = 0.0;
		int CarsCrushed = 0;
		bool bCrashed = false; // rolled over and couldn't recover
	};

	/** Judges' score 0..10, as in real freestyle: big tricks score, a crash ends the run early but keeps tricks. */
	double ScoreFreestyle(const FreestyleRun& Run);
}
