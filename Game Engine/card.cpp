#include"card.h"
#include<iostream>
#include<string>
using namespace std;
card::card(){}
card::card(Suit suit,Rank rank){
    this->suit=suit;
    this->rank=rank;
}
string card::convertSuitToString(Suit suit) const {
switch(suit){
        case Suit::HEARTS:   
        return "H";

        case Suit::DIAMONDS: 
        return "D";

        case Suit::CLUBS:  
        return "C";

        case Suit::SPADES:  
        return "S";

        default:
        return "?";
}
return " ";
}
string card::convertRankToString(Rank rank) const {
    switch(rank){
        case Rank::TWO:   
        return "2";

        case Rank::THREE: 
        return "3";

        case Rank::FOUR:  
        return "4";

        case Rank::FIVE:  
        return "5";

        case Rank::SIX:   
        return "6";

        case Rank::SEVEN: 
        return "7";

        case Rank::EIGHT: 
        return "8";

        case Rank::NINE:  
        return "9";

        case Rank::TEN:   
        return "10";

        case Rank::JACK:  
        return "J";

        case Rank::QUEEN: 
        return "Q";

        case Rank::KING:  
        return "K";

        case Rank::ACE:   
        return "A";

        default:       
        return "?";
    }
}
Suit card::getSuit() const
{
    return suit;
}

Rank card::getRank() const
{
    return rank;
}
void card::display() const {
{
    std::cout
        << convertRankToString(rank)
        << convertSuitToString(suit);
}
}
