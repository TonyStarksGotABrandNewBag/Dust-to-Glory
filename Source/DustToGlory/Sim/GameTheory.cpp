#include "Sim/GameTheory.h"

namespace dtg::sim
{
	namespace detail_gametheory
	{
		constexpr double Epsilon = 1e-9;

		/** Solves M x = b in place (partial pivoting). Returns false when singular. */
		bool SolveLinear(std::vector<std::vector<double>> M, std::vector<double> Rhs, std::vector<double>& Out)
		{
			const std::size_t N = Rhs.size();
			for (std::size_t Col = 0; Col < N; ++Col)
			{
				std::size_t Pivot = Col;
				for (std::size_t Row = Col + 1; Row < N; ++Row)
				{
					if (std::fabs(M[Row][Col]) > std::fabs(M[Pivot][Col]))
					{
						Pivot = Row;
					}
				}
				if (std::fabs(M[Pivot][Col]) < 1e-12)
				{
					return false;
				}
				std::swap(M[Pivot], M[Col]);
				std::swap(Rhs[Pivot], Rhs[Col]);
				for (std::size_t Row = 0; Row < N; ++Row)
				{
					if (Row == Col)
					{
						continue;
					}
					const double Factor = M[Row][Col] / M[Col][Col];
					for (std::size_t K = Col; K < N; ++K)
					{
						M[Row][K] -= Factor * M[Col][K];
					}
					Rhs[Row] -= Factor * Rhs[Col];
				}
			}
			Out.assign(N, 0.0);
			for (std::size_t Index = 0; Index < N; ++Index)
			{
				Out[Index] = Rhs[Index] / M[Index][Index];
			}
			return true;
		}

		/** All subsets of {0..N-1} with exactly K elements. */
		void Subsets(int N, int K, std::vector<std::vector<int>>& Out)
		{
			// Iterative combination generator.
			std::vector<int> Index(static_cast<std::size_t>(K));
			for (int I = 0; I < K; ++I)
			{
				Index[static_cast<std::size_t>(I)] = I;
			}
			if (K > N || K <= 0)
			{
				return;
			}
			while (true)
			{
				Out.push_back(Index);
				int Position = K - 1;
				while (Position >= 0 && Index[static_cast<std::size_t>(Position)] == N - K + Position)
				{
					--Position;
				}
				if (Position < 0)
				{
					break;
				}
				Index[static_cast<std::size_t>(Position)] += 1;
				for (int J = Position + 1; J < K; ++J)
				{
					Index[static_cast<std::size_t>(J)] = Index[static_cast<std::size_t>(J - 1)] + 1;
				}
			}
		}

		bool SameProfile(const Equilibrium& A, const Equilibrium& B)
		{
			for (std::size_t I = 0; I < A.Row.size(); ++I)
			{
				if (std::fabs(A.Row[I] - B.Row[I]) > 1e-6) return false;
			}
			for (std::size_t I = 0; I < A.Col.size(); ++I)
			{
				if (std::fabs(A.Col[I] - B.Col[I]) > 1e-6) return false;
			}
			return true;
		}
	}

	MatrixGame MatrixGame::Make(int InRows, int InCols, std::vector<double> InA, std::vector<double> InB)
	{
		MatrixGame Game;
		Game.Rows = InRows;
		Game.Cols = InCols;
		Game.A = std::move(InA);
		Game.B = std::move(InB);
		Game.A.resize(static_cast<std::size_t>(InRows * InCols), 0.0);
		Game.B.resize(static_cast<std::size_t>(InRows * InCols), 0.0);
		return Game;
	}

	MatrixGame MatrixGame::ZeroSum(int InRows, int InCols, std::vector<double> InA)
	{
		std::vector<double> Negated = InA;
		for (double& Value : Negated)
		{
			Value = -Value;
		}
		return Make(InRows, InCols, std::move(InA), std::move(Negated));
	}

	double ExpectedRowPayoff(const MatrixGame& Game, const std::vector<double>& Row, const std::vector<double>& Col)
	{
		double Total = 0.0;
		for (int R = 0; R < Game.Rows; ++R)
		{
			for (int C = 0; C < Game.Cols; ++C)
			{
				Total += Row[static_cast<std::size_t>(R)] * Col[static_cast<std::size_t>(C)] * Game.PayRow(R, C);
			}
		}
		return Total;
	}

	double ExpectedColPayoff(const MatrixGame& Game, const std::vector<double>& Row, const std::vector<double>& Col)
	{
		double Total = 0.0;
		for (int R = 0; R < Game.Rows; ++R)
		{
			for (int C = 0; C < Game.Cols; ++C)
			{
				Total += Row[static_cast<std::size_t>(R)] * Col[static_cast<std::size_t>(C)] * Game.PayCol(R, C);
			}
		}
		return Total;
	}

	int BestResponseRow(const MatrixGame& Game, const std::vector<double>& ColMix)
	{
		int Best = 0;
		double BestValue = -1e300;
		for (int R = 0; R < Game.Rows; ++R)
		{
			double Value = 0.0;
			for (int C = 0; C < Game.Cols; ++C)
			{
				Value += ColMix[static_cast<std::size_t>(C)] * Game.PayRow(R, C);
			}
			if (Value > BestValue + detail_gametheory::Epsilon)
			{
				BestValue = Value;
				Best = R;
			}
		}
		return Best;
	}

	int BestResponseCol(const MatrixGame& Game, const std::vector<double>& RowMix)
	{
		int Best = 0;
		double BestValue = -1e300;
		for (int C = 0; C < Game.Cols; ++C)
		{
			double Value = 0.0;
			for (int R = 0; R < Game.Rows; ++R)
			{
				Value += RowMix[static_cast<std::size_t>(R)] * Game.PayCol(R, C);
			}
			if (Value > BestValue + detail_gametheory::Epsilon)
			{
				BestValue = Value;
				Best = C;
			}
		}
		return Best;
	}

	std::vector<std::pair<int, int>> PureEquilibria(const MatrixGame& Game)
	{
		std::vector<std::pair<int, int>> Result;
		for (int R = 0; R < Game.Rows; ++R)
		{
			for (int C = 0; C < Game.Cols; ++C)
			{
				bool bRowBest = true;
				for (int Other = 0; Other < Game.Rows && bRowBest; ++Other)
				{
					bRowBest = Game.PayRow(Other, C) <= Game.PayRow(R, C) + detail_gametheory::Epsilon;
				}
				bool bColBest = true;
				for (int Other = 0; Other < Game.Cols && bColBest; ++Other)
				{
					bColBest = Game.PayCol(R, Other) <= Game.PayCol(R, C) + detail_gametheory::Epsilon;
				}
				if (bRowBest && bColBest)
				{
					Result.emplace_back(R, C);
				}
			}
		}
		return Result;
	}

	std::vector<Equilibrium> FindEquilibria(const MatrixGame& Game)
	{
		using namespace detail_gametheory;
		std::vector<Equilibrium> Found;
		const int MaxSupport = std::min(Game.Rows, Game.Cols);
		for (int K = 1; K <= MaxSupport; ++K)
		{
			std::vector<std::vector<int>> RowSets;
			std::vector<std::vector<int>> ColSets;
			Subsets(Game.Rows, K, RowSets);
			Subsets(Game.Cols, K, ColSets);
			for (const std::vector<int>& S : RowSets)
			{
				for (const std::vector<int>& T : ColSets)
				{
					const std::size_t N = static_cast<std::size_t>(K) + 1;
					// Column mix y over T makes the row player indifferent across S.
					std::vector<std::vector<double>> My(N, std::vector<double>(N, 0.0));
					std::vector<double> By(N, 0.0);
					for (std::size_t I = 0; I < S.size(); ++I)
					{
						for (std::size_t J = 0; J < T.size(); ++J)
						{
							My[I][J] = Game.PayRow(S[I], T[J]);
						}
						My[I][N - 1] = -1.0; // - v
					}
					for (std::size_t J = 0; J < T.size(); ++J)
					{
						My[N - 1][J] = 1.0;
					}
					By[N - 1] = 1.0;
					// Row mix x over S makes the column player indifferent across T.
					std::vector<std::vector<double>> Mx(N, std::vector<double>(N, 0.0));
					std::vector<double> Bx(N, 0.0);
					for (std::size_t J = 0; J < T.size(); ++J)
					{
						for (std::size_t I = 0; I < S.size(); ++I)
						{
							Mx[J][I] = Game.PayCol(S[I], T[J]);
						}
						Mx[J][N - 1] = -1.0;
					}
					for (std::size_t I = 0; I < S.size(); ++I)
					{
						Mx[N - 1][I] = 1.0;
					}
					Bx[N - 1] = 1.0;

					std::vector<double> Y;
					std::vector<double> X;
					if (!SolveLinear(My, By, Y) || !SolveLinear(Mx, Bx, X))
					{
						continue;
					}
					Equilibrium Candidate;
					Candidate.Row.assign(static_cast<std::size_t>(Game.Rows), 0.0);
					Candidate.Col.assign(static_cast<std::size_t>(Game.Cols), 0.0);
					bool bValid = true;
					for (std::size_t I = 0; I < S.size(); ++I)
					{
						if (X[I] < -1e-9) bValid = false;
						Candidate.Row[static_cast<std::size_t>(S[I])] = std::max(0.0, X[I]);
					}
					for (std::size_t J = 0; J < T.size(); ++J)
					{
						if (Y[J] < -1e-9) bValid = false;
						Candidate.Col[static_cast<std::size_t>(T[J])] = std::max(0.0, Y[J]);
					}
					if (!bValid)
					{
						continue;
					}
					const double RowValue = Y[N - 1];
					const double ColValue = X[N - 1];
					// No profitable deviation outside the supports.
					for (int R = 0; R < Game.Rows && bValid; ++R)
					{
						double Value = 0.0;
						for (int C = 0; C < Game.Cols; ++C) Value += Candidate.Col[static_cast<std::size_t>(C)] * Game.PayRow(R, C);
						bValid = Value <= RowValue + 1e-7;
					}
					for (int C = 0; C < Game.Cols && bValid; ++C)
					{
						double Value = 0.0;
						for (int R = 0; R < Game.Rows; ++R) Value += Candidate.Row[static_cast<std::size_t>(R)] * Game.PayCol(R, C);
						bValid = Value <= ColValue + 1e-7;
					}
					if (!bValid)
					{
						continue;
					}
					Candidate.RowValue = RowValue;
					Candidate.ColValue = ColValue;
					Candidate.bPure = K == 1;
					bool bDuplicate = false;
					for (const Equilibrium& Existing : Found)
					{
						bDuplicate = bDuplicate || SameProfile(Existing, Candidate);
					}
					if (!bDuplicate)
					{
						Found.push_back(Candidate);
					}
				}
			}
		}
		return Found;
	}

	bool SolveEquilibrium(const MatrixGame& Game, Equilibrium& Out)
	{
		const std::vector<Equilibrium> All = FindEquilibria(Game);
		if (All.empty())
		{
			return false;
		}
		// Prefer the equilibrium with the widest support (the "unexploitable" mix), then the first found.
		const Equilibrium* Best = &All.front();
		auto SupportSize = [](const Equilibrium& Value)
		{
			int Count = 0;
			for (const double P : Value.Row) Count += P > 1e-9 ? 1 : 0;
			for (const double P : Value.Col) Count += P > 1e-9 ? 1 : 0;
			return Count;
		};
		for (const Equilibrium& Candidate : All)
		{
			if (SupportSize(Candidate) > SupportSize(*Best))
			{
				Best = &Candidate;
			}
		}
		Out = *Best;
		return true;
	}

	int SampleStrategy(const std::vector<double>& Mix, Rng& Random)
	{
		double Roll = Random.NextDouble();
		for (std::size_t Index = 0; Index < Mix.size(); ++Index)
		{
			Roll -= Mix[Index];
			if (Roll <= 0.0)
			{
				return static_cast<int>(Index);
			}
		}
		return Mix.empty() ? 0 : static_cast<int>(Mix.size()) - 1;
	}

	Bargain NashBargain(double Surplus, double DisagreeA, double DisagreeB, double PowerA)
	{
		Bargain Result;
		const double Gains = Surplus - DisagreeA - DisagreeB;
		if (Gains < 0.0)
		{
			Result.PayoffA = DisagreeA;
			Result.PayoffB = DisagreeB;
			return Result;
		}
		const double Power = Clamp(PowerA, 0.0, 1.0);
		Result.bDeal = true;
		Result.PayoffA = DisagreeA + Power * Gains;
		Result.PayoffB = DisagreeB + (1.0 - Power) * Gains;
		return Result;
	}

	CournotResult CournotDuopoly(double Intercept, double Slope, double CostA, double CostB)
	{
		CournotResult Result;
		const double B = std::max(1e-9, Slope);
		// Best responses q_i = (a - c_i - b q_j) / 2b intersect at q_i = (a - 2c_i + c_j) / 3b.
		Result.QuantityA = std::max(0.0, (Intercept - 2.0 * CostA + CostB) / (3.0 * B));
		Result.QuantityB = std::max(0.0, (Intercept - 2.0 * CostB + CostA) / (3.0 * B));
		Result.Price = std::max(0.0, Intercept - B * (Result.QuantityA + Result.QuantityB));
		Result.ProfitA = (Result.Price - CostA) * Result.QuantityA;
		Result.ProfitB = (Result.Price - CostB) * Result.QuantityB;
		return Result;
	}

	double BertrandBestResponse(double Base, double Own, double Cross, double Cost, double RivalPrice)
	{
		const double B = std::max(1e-9, Own);
		return std::max(Cost, (Base + B * Cost + Cross * RivalPrice) / (2.0 * B));
	}

	BertrandResult BertrandDuopoly(double Base, double Own, double Cross, double CostA, double CostB)
	{
		// Solve p_A = (a + b c_A + d p_B)/2b and p_B = (a + b c_B + d p_A)/2b simultaneously.
		const double B = std::max(1e-9, Own);
		const double D = Cross;
		const double Denominator = 4.0 * B * B - D * D;
		BertrandResult Result;
		if (std::fabs(Denominator) < 1e-12)
		{
			return Result;
		}
		const double KA = Base + B * CostA;
		const double KB = Base + B * CostB;
		Result.PriceA = (2.0 * B * KA + D * KB) / Denominator;
		Result.PriceB = (2.0 * B * KB + D * KA) / Denominator;
		Result.QuantityA = std::max(0.0, Base - B * Result.PriceA + D * Result.PriceB);
		Result.QuantityB = std::max(0.0, Base - B * Result.PriceB + D * Result.PriceA);
		Result.ProfitA = (Result.PriceA - CostA) * Result.QuantityA;
		Result.ProfitB = (Result.PriceB - CostB) * Result.QuantityB;
		return Result;
	}

	const char* RepeatedStrategyName(RepeatedStrategy Value)
	{
		switch (Value)
		{
		case RepeatedStrategy::TitForTat: return "Tit for tat";
		case RepeatedStrategy::GenerousTitForTat: return "Generous tit for tat";
		case RepeatedStrategy::GrimTrigger: return "Grim trigger";
		case RepeatedStrategy::AlwaysCooperate: return "Always cooperate";
		case RepeatedStrategy::AlwaysDefect: return "Always defect";
		case RepeatedStrategy::Count: break;
		}
		return "Tit for tat";
	}

	bool DecideCooperate(const RepeatedGameMemory& Memory, Rng& Random)
	{
		switch (Memory.Strategy)
		{
		case RepeatedStrategy::AlwaysCooperate: return true;
		case RepeatedStrategy::AlwaysDefect: return false;
		case RepeatedStrategy::GrimTrigger: return !Memory.bTriggered;
		case RepeatedStrategy::GenerousTitForTat:
			// Forgives a defection a third of the time, which escapes revenge spirals.
			return Memory.LastOpponentMove != 0 || Random.Chance(0.33);
		case RepeatedStrategy::TitForTat:
		case RepeatedStrategy::Count: break;
		}
		return Memory.LastOpponentMove != 0;
	}

	void RecordOpponentMove(RepeatedGameMemory& Memory, bool bCooperated)
	{
		Memory.Rounds += 1;
		Memory.LastOpponentMove = bCooperated ? 1 : 0;
		if (!bCooperated)
		{
			Memory.OpponentDefections += 1;
			Memory.bTriggered = true;
		}
	}

	InspectionResult InspectionGame(double Gain, double Fine, double Cost, double Loss)
	{
		InspectionResult Result;
		// The violator is indifferent when p * (-Fine) + (1 - p) * Gain = 0.
		Result.InspectProbability = Clamp(Gain / std::max(1e-9, Gain + Fine), 0.0, 1.0);
		// The inspector is indifferent when the inspection cost equals the expected loss prevented.
		Result.ViolateProbability = Clamp(Cost / std::max(1e-9, Loss), 0.0, 1.0);
		return Result;
	}

	MatrixGame CampaignGame(double PeopleSkill, double CreativitySkill, double PlayerTrust)
	{
		// Rows: player strategy, columns: opponent strategy. Payoffs: swing in the player's poll share.
		const double People = Clamp(PeopleSkill, 0.0, 10.0) / 10.0;
		const double Creative = Clamp(CreativitySkill, 0.0, 10.0) / 10.0;
		const double Trust = Clamp(PlayerTrust, 0.0, 1.0);
		// Attacks land harder for a creative campaign but backfire on a trusted candidate's image.
		const double AttackEdge = 0.02 + 0.015 * Creative - 0.02 * Trust;
		std::vector<double> Payoffs = {
			// vs Positive      vs Attack                 vs Ground game
			0.0,                -0.02 + 0.015 * Trust,    0.012 + 0.01 * People,   // Positive message
			AttackEdge,         -0.006,                   -0.015,                  // Attack ads
			-0.01,              0.02 + 0.01 * People,     0.0                      // Ground game
		};
		MatrixGame Game = MatrixGame::ZeroSum(3, 3, Payoffs);
		Game.RowLabels = {"Positive message", "Attack ads", "Ground game"};
		Game.ColLabels = Game.RowLabels;
		return Game;
	}

	double RubinsteinShare(double ProposerPatience, double ResponderPatience)
	{
		const double Proposer = Clamp(ProposerPatience, 0.0, 0.999);
		const double Responder = Clamp(ResponderPatience, 0.0, 0.999);
		return (1.0 - Responder) / (1.0 - Proposer * Responder);
	}
}
