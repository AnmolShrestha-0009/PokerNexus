#ifndef DECK_H
#define DECK_H

#include<vector>
#include<algorithm>
#include<iostream>
#include<random>
#include "card.h"
class deck{
private:
    std::vector<card> cards_;
    std::mt19937 rng_;

    public:

deck();


int remainingCards() const;

void displayDeck() const;

void shuffleDeck();

card dealCard();


};

#endif