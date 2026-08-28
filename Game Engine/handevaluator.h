#ifndef HANDEVALUATOR_H
#define HANDEVALUATOR_H

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include "card.h"
#include "player.h"

enum class handRank {
    HIGH_CARD = 0,
    PAIR,
    TWO_PAIR,
    THREE_OF_A_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    FOUR_OF_A_KIND,
    STRAIGHT_FLUSH
};

struct HandResult {
    handRank rank;
    std::vector<int> tieBreakers; 

    
    bool operator>(const HandResult& other) const {
        if (rank != other.rank) {
            return rank > other.rank;
        }
        for (size_t i = 0; i < tieBreakers.size() && i < other.tieBreakers.size(); ++i) {
            if (tieBreakers[i] != other.tieBreakers[i]) {
                return tieBreakers[i] > other.tieBreakers[i];
            }
        }
        return false;
    }

       bool operator==(const HandResult& other) const {
        if (rank != other.rank) return false;
        if (tieBreakers.size() != other.tieBreakers.size()) return false;
        for (size_t i = 0; i < tieBreakers.size(); ++i) {
            if (tieBreakers[i] != other.tieBreakers[i]) return false;
        }
        return true;
    }
};

class handEvaluator {
private:
    HandResult bestResult_;             
    std::vector<card> best5CardHand_;   

    HandResult evaluate5CardHand(std::vector<card> temp);

public:
    int k = 5;
    int n = 7;
    int start = 0;

    handRank hand(std::vector<card> a);

    void generateCombination(int start, int k, std::vector<card>& temp, const std::vector<card>& cards);

    handRank HandRankfunction(std::vector<card> temp);

    HandResult getBestResult(const std::vector<card>& sevenCards);
  
    std::vector<card> getBest5CardHand() const;

    std::string getHandRankString(handRank rank) const;
};

#endif