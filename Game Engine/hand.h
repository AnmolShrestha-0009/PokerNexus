#ifndef HAND_H
#define HAND_H
#include<iostream>
#include<vector>
#include "card.h"
class Hand
{
private:

    std::vector<card> cards;

public:
Hand();
    Hand(card,card);


    void clear();

    void sort();

    void display() const;
    void addCard(const card& c);
   const std:: vector<card>& getCards() const;
};
#endif