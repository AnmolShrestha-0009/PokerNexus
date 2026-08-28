#include"hand.h"
#include<iostream>
#include<vector>
Hand::Hand(){}
Hand::Hand(card c1,card c2){
cards.push_back(c1);
cards.push_back(c2);
}

   void Hand::display() const{
    std::cout<<"The hole cards are"<<"\t";
    for (card c : cards) {
        c.display();
        std::cout<<"  ";
    }
   
}
    void Hand::addCard(const card& c)
{
    cards.push_back(c);
}
void Hand::clear()
{
    cards.clear();
}
const std::vector<card>& Hand::getCards() const
{
    return cards;
}

