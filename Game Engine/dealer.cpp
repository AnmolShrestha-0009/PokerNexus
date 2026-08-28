#include "dealer.h"

Dealer::Dealer() {
    deck_ = deck();
}

void Dealer::shuffleDeck() {
    deck_.shuffleDeck();
}

void Dealer::dealHoleCards(std::vector<Player>& players) {
    for (size_t i = 0; i < players.size(); ++i) {
        players[i].clearHand();
        card c1 = deck_.dealCard();
        card c2 = deck_.dealCard();
        players[i].setHand(Hand(c1, c2));
    }
}

std::vector<card> Dealer::dealFlop() {
    std::vector<card> flop;
    flop.push_back(deck_.dealCard());
    flop.push_back(deck_.dealCard());
    flop.push_back(deck_.dealCard());
    return flop;
}

card Dealer::dealTurn() {
    return deck_.dealCard();
}

card Dealer::dealRiver() {
    return deck_.dealCard();
}

void Dealer::resetDeck() {
    deck_ = deck();
}
