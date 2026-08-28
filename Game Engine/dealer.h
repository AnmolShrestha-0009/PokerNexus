#ifndef DEALER_H
#define DEALER_H

#include <vector>
#include "card.h" 
#include "deck.h"
#include "player.h"

class Dealer {
private:
    deck deck_;

public:
    Dealer();

    void shuffleDeck();

    void dealHoleCards(std::vector<Player>& players);

    std::vector<card> dealFlop();

    card dealTurn();

    card dealRiver();

    void resetDeck();
};

#endif
