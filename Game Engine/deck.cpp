#include "player.h"
#include "deck.h"
#include<iostream>
#include<algorithm>
#include<random>
#include<vector>
#include "dealer.h"

deck::deck()
{
    std::random_device rd;
    rng_ = std::mt19937(rd());

    for(int suit=0;suit<4;suit++)
    {
        for(int rank=2;rank<=14;rank++)
        {
            cards_.push_back(

                card(
                    static_cast<Suit>(suit),
                    static_cast<Rank>(rank)
                )
            );
        }
    }
}

void deck::displayDeck() const
{
    for(auto c : cards_)
    {
        c.display();
        std::cout<<" ";
    }
}

void deck::shuffleDeck() {
    std::shuffle(cards_.begin(), cards_.end(), rng_);
}

int deck::remainingCards() const
{
    return cards_.size();
}

card deck::dealCard(){
    if(cards_.empty()){
    std::cout<<"cards out of limit ";
    }
  
    card top = cards_.back();
    cards_.pop_back();
    return top;
    
}

