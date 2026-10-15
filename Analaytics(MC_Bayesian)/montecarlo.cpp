#include "montecarlo.h"
#include <algorithm>

MonteCarloSimulator::MonteCarloSimulator() {
    std::random_device rd;
    rng_ = std::mt19937(rd());
}

std::vector<card> MonteCarloSimulator::buildRemainingDeck(const std::vector<card>& known) const {
    std::vector<card> remaining;
    remaining.reserve(52);

    for (int s = 0; s < 4; ++s) {
        for (int r = 2; r <= 14; ++r) {
            card candidate(static_cast<Suit>(s), static_cast<Rank>(r));

            bool isKnown = false;
            for (const auto& k : known) {
                if (k.getSuit() == candidate.getSuit() && k.getRank() == candidate.getRank()) {
                    isKnown = true;
                    break;
                }
            }
            if (!isKnown) {
                remaining.push_back(candidate);
            }
        }
    }
    return remaining;
}

EquityResult MonteCarloSimulator::estimateEquity(
    const std::vector<card>& heroHole,
    const std::vector<card>& board,
    int numSimulations)

{
    EquityResult result{0.0, 0.0, 0.0, 0};

    if (heroHole.size() != 2) {
        return result; 
    }

    int wins = 0, ties = 0, losses = 0;

    std::vector<card> known = heroHole;
    known.insert(known.end(), board.begin(), board.end());

    const int cardsNeededOnBoard = 5 - static_cast<int>(board.size());


    //monte carlo loop
    for (int trial = 0; trial < numSimulations; ++trial) {
        std::vector<card> deck = buildRemainingDeck(known);

        if (static_cast<int>(deck.size()) < 2 + cardsNeededOnBoard) {
            break; // Not enough unseen cards left (shouldn't normally happen)
        }

        std::shuffle(deck.begin(), deck.end(), rng_);

        card oppC1 = deck[0];
        card oppC2 = deck[1];

        std::vector<card> simBoard = board;
        for (int i = 0; i < cardsNeededOnBoard; ++i) {
            simBoard.push_back(deck[2 + i]);
        }

        std::vector<card> heroSeven = heroHole;
        heroSeven.insert(heroSeven.end(), simBoard.begin(), simBoard.end());

        std::vector<card> oppSeven = { oppC1, oppC2 };
        oppSeven.insert(oppSeven.end(), simBoard.begin(), simBoard.end());

        HandResult heroResult = evaluator_.getBestResult(heroSeven);
        HandResult oppResult  = evaluator_.getBestResult(oppSeven);

        if (heroResult > oppResult)      wins++;
        else if (oppResult > heroResult) losses++;
        else                              ties++;

        result.trialsRun++;
    }

    if (result.trialsRun > 0) {
        result.winProbability  = static_cast<double>(wins)   / result.trialsRun;
        result.tieProbability  = static_cast<double>(ties)   / result.trialsRun;
        result.loseProbability = static_cast<double>(losses) / result.trialsRun;
    }

    return result;
}