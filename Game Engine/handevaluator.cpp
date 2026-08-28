#include "handevaluator.h"
#include <algorithm>
void handEvaluator::generateCombination(int start, int k, std::vector<card>& temp, const std::vector<card>& cards)
{
    
    if (k == 0)
    {
        
        HandRankfunction(temp);
        return;
    }

    if (start >= (int)cards.size())
        return;

   
    temp.push_back(cards[start]);
    generateCombination(start + 1, k - 1, temp, cards);

   
    temp.pop_back();
    generateCombination(start + 1, k, temp, cards);
}

handRank handEvaluator::HandRankfunction(std::vector<card> temp) {
    HandResult res = evaluate5CardHand(temp);

    if (res > bestResult_) {
        bestResult_ = res;
        best5CardHand_ = temp;
    }
    return res.rank;
}

handRank handEvaluator::hand(std::vector<card> a) {
  
    bestResult_.rank = handRank::HIGH_CARD;
    bestResult_.tieBreakers.clear();
    bestResult_.tieBreakers.assign(5, 0); 
    best5CardHand_.clear();

    std::vector<card> temp;
    
    generateCombination(0, 5, temp, a);

    return bestResult_.rank;
}

HandResult handEvaluator::getBestResult(const std::vector<card>& sevenCards) {
    hand(sevenCards);
    return bestResult_;
}

std::vector<card> handEvaluator::getBest5CardHand() const {
    return best5CardHand_;
}

HandResult handEvaluator::evaluate5CardHand(std::vector<card> temp) {
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 5; ++j) {
            if (static_cast<int>(temp[i].getRank()) < static_cast<int>(temp[j].getRank())) {
                card backup = temp[i];
                temp[i] = temp[j];
                temp[j] = backup;
            }
        }
    }

    bool isFlush = true;
    for (int i = 1; i < 5; ++i) {
        if (temp[i].getSuit() != temp[0].getSuit()) {
            isFlush = false;
            break;
        }
    }

    bool isStraight = false;
    bool isAceLow = false;

    if (static_cast<int>(temp[0].getRank()) - static_cast<int>(temp[4].getRank()) == 4) {
        bool consecutive = true;
        for (int i = 1; i < 5; ++i) {
            if (static_cast<int>(temp[i].getRank()) != static_cast<int>(temp[i - 1].getRank()) - 1) {
                consecutive = false;
                break;
            }
        }
        if (consecutive) isStraight = true;
    }

    if (!isStraight && 
        temp[0].getRank() == Rank::ACE && 
        temp[1].getRank() == Rank::FIVE &&
        temp[2].getRank() == Rank::FOUR && 
        temp[3].getRank() == Rank::THREE && 
        temp[4].getRank() == Rank::TWO) {
        isStraight = true;
        isAceLow = true;
    }

    // 4. Perform Frequency Counting using std::map
    std::map<Rank, int> counts;
    for (int i = 0; i < 5; ++i) {
        counts[temp[i].getRank()]++;
    }

    HandResult result;

    // 5. Evaluate the poker hand and generate tie-breakers
    
    // Case A: Straight Flush
    if (isFlush && isStraight) {
        result.rank = handRank::STRAIGHT_FLUSH;
        // Tie-breaker: rank of the highest card in the straight (5 for Ace-low)
        result.tieBreakers.push_back(isAceLow ? 5 : static_cast<int>(temp[0].getRank()));
    }
    // Case B: Four of a Kind or Full House (occurs when there are only 2 distinct ranks)
    else if (counts.size() == 2) {
        // Find which rank has the higher count (4 for quads, 3 for full house triplet)
        Rank quadOrTriRank = Rank::TWO;
        Rank otherRank = Rank::TWO;
        int maxCount = 0;

        for (auto const& pair : counts) {
            if (pair.second > maxCount) {
                maxCount = pair.second;
                quadOrTriRank = pair.first;
            }
        }
        for (auto const& pair : counts) {
            if (pair.first != quadOrTriRank) {
                otherRank = pair.first;
            }
        }

        if (maxCount == 4) {
            result.rank = handRank::FOUR_OF_A_KIND;
            result.tieBreakers.push_back(static_cast<int>(quadOrTriRank)); // Quads rank
            result.tieBreakers.push_back(static_cast<int>(otherRank));     // Kicker rank
        } else {
            result.rank = handRank::FULL_HOUSE;
            result.tieBreakers.push_back(static_cast<int>(quadOrTriRank)); // Triplet rank
            result.tieBreakers.push_back(static_cast<int>(otherRank));     // Pair rank
        }
    }
    else if (isFlush) {
        result.rank = handRank::FLUSH;
       
        for (int i = 0; i < 5; ++i) {
            result.tieBreakers.push_back(static_cast<int>(temp[i].getRank()));
        }
    }
    else if (isStraight) {
        result.rank = handRank::STRAIGHT;
        result.tieBreakers.push_back(isAceLow ? 5 : static_cast<int>(temp[0].getRank()));
    }
   
    else if (counts.size() == 3) {
        bool hasThree = false;
        Rank triRank = Rank::TWO;

        for (auto const& pair : counts) {
            if (pair.second == 3) {
                hasThree = true;
                triRank = pair.first;
                break;
            }
        }

        if (hasThree) {
            result.rank = handRank::THREE_OF_A_KIND;
            result.tieBreakers.push_back(static_cast<int>(triRank)); 
         
            for (int i = 0; i < 5; ++i) {
                if (temp[i].getRank() != triRank) {
                    result.tieBreakers.push_back(static_cast<int>(temp[i].getRank()));
                }
            }
        } else {
            
            result.rank = handRank::TWO_PAIR;
            std::vector<int> pairRanks;
            int kickerRank = 0;

            for (auto const& pair : counts) {
                if (pair.second == 2) {
                    pairRanks.push_back(static_cast<int>(pair.first));
                } else {
                    kickerRank = static_cast<int>(pair.first);
                }
            }
            
            if (pairRanks[0] < pairRanks[1]) {
                int swap = pairRanks[0];
                pairRanks[0] = pairRanks[1];
                pairRanks[1] = swap;
            }
            result.tieBreakers.push_back(pairRanks[0]); 
            result.tieBreakers.push_back(pairRanks[1]); 
            result.tieBreakers.push_back(kickerRank);   
        }
    }
   
    else if (counts.size() == 4) {
        result.rank = handRank::PAIR;
        Rank pairRank = Rank::TWO;

        for (auto const& pair : counts) {
            if (pair.second == 2) {
                pairRank = pair.first;
                break;
            }
        }
        result.tieBreakers.push_back(static_cast<int>(pairRank)); 
       
        for (int i = 0; i < 5; ++i) {
            if (temp[i].getRank() != pairRank) {
                result.tieBreakers.push_back(static_cast<int>(temp[i].getRank()));
            }
        }
    }
    
    else {
        result.rank = handRank::HIGH_CARD;
 
        for (int i = 0; i < 5; ++i) {
            result.tieBreakers.push_back(static_cast<int>(temp[i].getRank()));
        }
    }

    return result;
}

std::string handEvaluator::getHandRankString(handRank rank) const {
    switch (rank) {
        case handRank::HIGH_CARD:       return "High Card";
        case handRank::PAIR:            return "Pair";
        case handRank::TWO_PAIR:        return "Two Pair";
        case handRank::THREE_OF_A_KIND: return "Three of a Kind";
        case handRank::STRAIGHT:        return "Straight";
        case handRank::FLUSH:           return "Flush";
        case handRank::FULL_HOUSE:      return "Full House";
        case handRank::FOUR_OF_A_KIND:  return "Four of a Kind";
        case handRank::STRAIGHT_FLUSH:  return "Straight Flush";
        default:                        return "Unknown Hand";
    }
}