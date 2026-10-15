#ifndef MONTECARLO_H
#define MONTECARLO_H

#include <vector>
#include <random>
#include "card.h"
#include "handevaluator.h"

struct EquityResult {
    double winProbability;   
    double tieProbability;   
    double loseProbability;  
    int trialsRun;          
};

class MonteCarloSimulator {
private:
    handEvaluator evaluator_;
    std::mt19937 rng_;

    std::vector<card> buildRemainingDeck(const std::vector<card>& known) const;

public:
    MonteCarloSimulator();

    EquityResult estimateEquity(
        const std::vector<card>& heroHole,
        const std::vector<card>& board,
        int numSimulations = 2000
    );
};

#endif