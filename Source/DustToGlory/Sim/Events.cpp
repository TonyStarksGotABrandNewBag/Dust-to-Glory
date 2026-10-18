#include "Sim/Events.h"

namespace dtg::sim
{
	const char* EventKindName(EventKind Kind)
	{
		switch (Kind)
		{
		case EventKind::DragRace: return "Drag Racing";
		case EventKind::DemolitionDerby: return "Demolition Derby";
		case EventKind::MonsterJam: return "Monster Truck Freestyle";
		case EventKind::Formula: return "Open-Wheel Grand Prix";
		case EventKind::Count: break;
		}
		return "Event";
	}

	Money EventPrize(EventKind Kind, int Tier, int Position, int FieldSize)
	{
		const int T = Clamp(Tier, 1, 5);
		double Purse = 0.0;
		switch (Kind)
		{
		case EventKind::DragRace: Purse = 4000.0; break;
		case EventKind::DemolitionDerby: Purse = 6000.0; break;
		case EventKind::MonsterJam: Purse = 8000.0; break;
		case EventKind::Formula: Purse = 20000.0; break;
		case EventKind::Count: break;
		}
		Purse *= std::pow(2.0, T - 1);
		const int Field = std::max(1, FieldSize);
		const int Place = Clamp(Position, 1, Field);
		// Winner takes 50%, then a sliding share for the rest of the top half.
		double Share = 0.0;
		if (Place == 1) Share = 0.5;
		else if (Place == 2) Share = 0.25;
		else if (Place == 3) Share = 0.12;
		else if (Place <= std::max(4, Field / 2)) Share = 0.04;
		return ToMoney(std::round(Purse * Share / 10.0) * 10.0);
	}

	void DragTree::Start(double StageDelay)
	{
		bStarted = true;
		bLaunched = false;
		Clock = 0.0;
		Reaction = 0.0;
		FirstAmberAt = std::max(0.0, StageDelay);
		GreenAt = Type == TreeType::Pro ? FirstAmberAt + 0.4 : FirstAmberAt + 1.5;
	}

	void DragTree::Tick(double Dt)
	{
		if (bStarted && Dt > 0.0)
		{
			Clock += Dt;
		}
	}

	int DragTree::AmbersLit() const
	{
		if (!bStarted || Clock < FirstAmberAt)
		{
			return 0;
		}
		if (Type == TreeType::Pro)
		{
			return 3;
		}
		return Clamp(1 + static_cast<int>((Clock - FirstAmberAt) / 0.5), 1, 3);
	}

	double DragTree::Launch()
	{
		if (!bStarted || bLaunched)
		{
			return Reaction;
		}
		bLaunched = true;
		Reaction = Clock - GreenAt;
		return Reaction;
	}

	bool DragWinner(const DragResult& A, const DragResult& B)
	{
		if (A.bFoul != B.bFoul)
		{
			return !A.bFoul;
		}
		if (A.bFoul && B.bFoul)
		{
			// Both red-lit: the one who left later (less negative) wins.
			return A.Reaction > B.Reaction;
		}
		return A.Reaction + A.ElapsedTime < B.Reaction + B.ElapsedTime;
	}

	RaceTracker::RaceTracker(int InCheckpoints, int InLaps, int InRacers)
		: CheckpointCount(std::max(1, InCheckpoints)), LapCount(std::max(1, InLaps)), State(static_cast<std::size_t>(std::max(1, InRacers)))
	{
	}

	bool RaceTracker::PassCheckpoint(int Racer, int Checkpoint, double Time)
	{
		if (Racer < 0 || Racer >= Racers())
		{
			return false;
		}
		RacerState& Entry = State[static_cast<std::size_t>(Racer)];
		if (Entry.bFinished || Checkpoint != Entry.Passed % CheckpointCount)
		{
			return false;
		}
		Entry.Passed += 1;
		Entry.LastPassTime = Time;
		// Checkpoint 0 is the start/finish line; passing it again completes a lap.
		if (Checkpoint == 0 && Entry.Passed > 1)
		{
			const double LapTime = Time - Entry.LapStart;
			Entry.LastLap = LapTime;
			if (Entry.BestLap <= 0.0 || LapTime < Entry.BestLap)
			{
				Entry.BestLap = LapTime;
			}
			Entry.LapStart = Time;
			if ((Entry.Passed - 1) / CheckpointCount >= LapCount)
			{
				Entry.bFinished = true;
				Entry.FinishTime = Time;
			}
		}
		else if (Checkpoint == 0)
		{
			Entry.LapStart = Time;
		}
		return true;
	}

	int RaceTracker::Lap(int Racer) const
	{
		if (Racer < 0 || Racer >= Racers())
		{
			return 0;
		}
		const int Passed = State[static_cast<std::size_t>(Racer)].Passed;
		return Passed <= 0 ? 0 : std::min(LapCount, 1 + (Passed - 1) / CheckpointCount);
	}

	int RaceTracker::NextCheckpoint(int Racer) const
	{
		if (Racer < 0 || Racer >= Racers())
		{
			return 0;
		}
		return State[static_cast<std::size_t>(Racer)].Passed % CheckpointCount;
	}

	bool RaceTracker::Finished(int Racer) const
	{
		return Racer >= 0 && Racer < Racers() && State[static_cast<std::size_t>(Racer)].bFinished;
	}

	double RaceTracker::FinishTime(int Racer) const
	{
		return Finished(Racer) ? State[static_cast<std::size_t>(Racer)].FinishTime : 0.0;
	}

	double RaceTracker::BestLap(int Racer) const
	{
		return (Racer >= 0 && Racer < Racers()) ? State[static_cast<std::size_t>(Racer)].BestLap : 0.0;
	}

	double RaceTracker::LastLap(int Racer) const
	{
		return (Racer >= 0 && Racer < Racers()) ? State[static_cast<std::size_t>(Racer)].LastLap : 0.0;
	}

	std::vector<int> RaceTracker::Standings(const std::vector<double>& DistanceToNext) const
	{
		std::vector<int> Order;
		for (int Racer = 0; Racer < Racers(); ++Racer)
		{
			Order.push_back(Racer);
		}
		auto Distance = [&DistanceToNext](int Racer)
		{
			return static_cast<std::size_t>(Racer) < DistanceToNext.size() ? DistanceToNext[static_cast<std::size_t>(Racer)] : 1e9;
		};
		std::stable_sort(Order.begin(), Order.end(), [this, &Distance](int A, int B)
		{
			const RacerState& SA = State[static_cast<std::size_t>(A)];
			const RacerState& SB = State[static_cast<std::size_t>(B)];
			if (SA.bFinished != SB.bFinished) return SA.bFinished;
			if (SA.bFinished && SB.bFinished) return SA.FinishTime < SB.FinishTime;
			if (SA.Passed != SB.Passed) return SA.Passed > SB.Passed;
			return Distance(A) < Distance(B);
		});
		return Order;
	}

	int RaceTracker::Position(int Racer, const std::vector<double>& DistanceToNext) const
	{
		const std::vector<int> Order = Standings(DistanceToNext);
		for (std::size_t Index = 0; Index < Order.size(); ++Index)
		{
			if (Order[Index] == Racer)
			{
				return static_cast<int>(Index) + 1;
			}
		}
		return Racers();
	}

	int RaceTracker::FastestLapRacer() const
	{
		int Best = -1;
		for (int Racer = 0; Racer < Racers(); ++Racer)
		{
			const double Lap = State[static_cast<std::size_t>(Racer)].BestLap;
			if (Lap > 0.0 && (Best < 0 || Lap < State[static_cast<std::size_t>(Best)].BestLap))
			{
				Best = Racer;
			}
		}
		return Best;
	}

	int FormulaPoints(int Position)
	{
		static const int Points[] = {25, 18, 15, 12, 10, 8, 6, 4, 2, 1};
		return (Position >= 1 && Position <= 10) ? Points[Position - 1] : 0;
	}

	int DerbyHitPoints(double ImpactSpeedKph)
	{
		if (ImpactSpeedKph < 12.0)
		{
			return 0;
		}
		return 1 + static_cast<int>((ImpactSpeedKph - 12.0) / 10.0);
	}

	std::vector<int> DerbyRanking(const std::vector<DerbyScore>& Scores)
	{
		std::vector<int> Order;
		for (std::size_t Index = 0; Index < Scores.size(); ++Index)
		{
			Order.push_back(static_cast<int>(Index));
		}
		std::stable_sort(Order.begin(), Order.end(), [&Scores](int A, int B)
		{
			const DerbyScore& SA = Scores[static_cast<std::size_t>(A)];
			const DerbyScore& SB = Scores[static_cast<std::size_t>(B)];
			if (SA.bSurvived != SB.bSurvived) return SA.bSurvived;
			return SA.Points > SB.Points;
		});
		return Order;
	}

	double ScoreFreestyle(const FreestyleRun& Run)
	{
		double Score = 0.0;
		Score += std::min(2.5, Run.AirtimeS * 0.25);
		Score += 0.5 * Run.BigAirs;
		Score += 2.0 * Run.Backflips + 2.5 * Run.Frontflips;
		Score += 0.8 * Run.Rolls;
		Score += 0.4 * Run.Donuts;
		Score += std::min(1.5, Run.WheelieS * 0.3);
		Score += 0.3 * Run.CarsCrushed;
		if (Run.bCrashed)
		{
			Score *= 0.85;
		}
		return std::round(Clamp(Score, 0.0, 10.0) * 10.0) / 10.0;
	}
}
