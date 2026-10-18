// Game theory (John Nash and friends) used by the AI and the economy:
//   * Nash equilibria of small matrix games (pure, and mixed via support enumeration)
//   * the (generalized) Nash bargaining solution for every negotiation
//   * Cournot and differentiated-Bertrand duopoly equilibria for business competition
//   * repeated prisoner's dilemma strategies for NPC partnerships
//   * the inspection game for police patrol intensity
#pragma once

#include "Sim/SimCommon.h"

#include <string>
#include <utility>
#include <vector>

namespace dtg::sim
{
	/** Two-player game in normal form. A = row player's payoffs, B = column player's, both row-major. */
	struct MatrixGame
	{
		int Rows = 0;
		int Cols = 0;
		std::vector<double> A;
		std::vector<double> B;
		std::vector<std::string> RowLabels;
		std::vector<std::string> ColLabels;

		double PayRow(int Row, int Col) const { return A[static_cast<std::size_t>(Row * Cols + Col)]; }
		double PayCol(int Row, int Col) const { return B[static_cast<std::size_t>(Row * Cols + Col)]; }

		static MatrixGame Make(int InRows, int InCols, std::vector<double> InA, std::vector<double> InB);
		/** Zero-sum: the column player's payoff is the negative of the row player's. */
		static MatrixGame ZeroSum(int InRows, int InCols, std::vector<double> InA);
	};

	struct Equilibrium
	{
		std::vector<double> Row; // probability of each row strategy
		std::vector<double> Col;
		double RowValue = 0.0;   // expected payoffs at the equilibrium
		double ColValue = 0.0;
		bool bPure = false;
	};

	std::vector<std::pair<int, int>> PureEquilibria(const MatrixGame& Game);
	/** Every equilibrium found by support enumeration (games up to about 5x5). */
	std::vector<Equilibrium> FindEquilibria(const MatrixGame& Game);
	/** A single equilibrium, preferring a fully mixed one in zero-sum games (the minimax solution). */
	bool SolveEquilibrium(const MatrixGame& Game, Equilibrium& Out);

	double ExpectedRowPayoff(const MatrixGame& Game, const std::vector<double>& Row, const std::vector<double>& Col);
	double ExpectedColPayoff(const MatrixGame& Game, const std::vector<double>& Row, const std::vector<double>& Col);
	int BestResponseRow(const MatrixGame& Game, const std::vector<double>& ColMix);
	int BestResponseCol(const MatrixGame& Game, const std::vector<double>& RowMix);
	int SampleStrategy(const std::vector<double>& Mix, Rng& Random);

	/**
	 * Rubinstein alternating-offers bargaining: the subgame-perfect share of the pie claimed by the player who
	 * proposes first, given each side's per-round discount factor (patience). (1 - dR) / (1 - dP * dR).
	 */
	double RubinsteinShare(double ProposerPatience, double ResponderPatience);

	/**
	 * Generalized Nash bargaining over a surplus. Each side gets its disagreement payoff (walk-away value) plus a
	 * share of what is left, weighted by bargaining power: Share_A = D_A + Power_A * (Surplus - D_A - D_B).
	 */
	struct Bargain
	{
		bool bDeal = false; // false when walking away beats any split
		double PayoffA = 0.0;
		double PayoffB = 0.0;
	};

	Bargain NashBargain(double Surplus, double DisagreeA, double DisagreeB, double PowerA);

	/** Quantity competition (Cournot): inverse demand P = Intercept - Slope * (qA + qB). */
	struct CournotResult
	{
		double QuantityA = 0.0;
		double QuantityB = 0.0;
		double Price = 0.0;
		double ProfitA = 0.0;
		double ProfitB = 0.0;
	};

	CournotResult CournotDuopoly(double Intercept, double Slope, double CostA, double CostB);

	/** Price competition with differentiated products: q_i = Base - Own * p_i + Cross * p_j. */
	struct BertrandResult
	{
		double PriceA = 0.0;
		double PriceB = 0.0;
		double QuantityA = 0.0;
		double QuantityB = 0.0;
		double ProfitA = 0.0;
		double ProfitB = 0.0;
	};

	BertrandResult BertrandDuopoly(double Base, double Own, double Cross, double CostA, double CostB);
	/** The rival's best price against a given price. */
	double BertrandBestResponse(double Base, double Own, double Cross, double Cost, double RivalPrice);

	enum class RepeatedStrategy : int
	{
		TitForTat,
		GenerousTitForTat,
		GrimTrigger,
		AlwaysCooperate,
		AlwaysDefect,
		Count
	};

	const char* RepeatedStrategyName(RepeatedStrategy Value);

	/** One side's memory of an ongoing repeated prisoner's dilemma. */
	struct RepeatedGameMemory
	{
		RepeatedStrategy Strategy = RepeatedStrategy::TitForTat;
		int LastOpponentMove = -1; // 1 cooperated, 0 defected, -1 none yet
		bool bTriggered = false;   // grim trigger has been pulled
		int Rounds = 0;
		int OpponentDefections = 0;
	};

	bool DecideCooperate(const RepeatedGameMemory& Memory, Rng& Random);
	void RecordOpponentMove(RepeatedGameMemory& Memory, bool bCooperated);

	/**
	 * Inspection game: a violator gains Gain if unchecked and pays Fine if caught; inspecting costs Cost and
	 * an unchecked violation costs society Loss. Returns the equilibrium inspection and violation rates.
	 */
	struct InspectionResult
	{
		double InspectProbability = 0.0;
		double ViolateProbability = 0.0;
	};

	InspectionResult InspectionGame(double Gain, double Fine, double Cost, double Loss);

	/** The campaign strategy game: Positive message, Attack ads, Ground game. Payoffs are poll swings. */
	MatrixGame CampaignGame(double PeopleSkill, double CreativitySkill, double PlayerTrust);
}
