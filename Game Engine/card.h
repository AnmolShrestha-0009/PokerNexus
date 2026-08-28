#ifndef CARD_H
#define CARD_H

#include<iostream>
#include<string>

enum class Suit{
    HEARTS,
    DIAMONDS,
    CLUBS,
    SPADES
};
enum class Rank{
    TWO   = 2,
    THREE = 3,
    FOUR  = 4,
    FIVE  = 5,
    SIX   = 6,
    SEVEN = 7,
    EIGHT = 8,
    NINE  = 9,
    TEN   = 10,
    JACK  = 11,
    QUEEN = 12,
    KING  = 13,
    ACE   = 14
};
class card{
    Suit suit;
    Rank rank;
public:
card();
card(Suit suit,Rank rank);
Suit getSuit() const;
Rank getRank() const;

std::string convertSuitToString(Suit suit) const;

std::string convertRankToString(Rank rank) const;

void display() const;
};
#endif
