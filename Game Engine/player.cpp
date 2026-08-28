#include "player.h"
#include <iostream>

Player::Player()
{
    name = "";
    chips = 1000;
    currentBet = 0;
    folded = false;
    allIn = false;
    check_=false;
}

Player::Player(std::string name, int chips)
{
    this->name = name;
    this->chips = chips;

    currentBet = 0;

    folded = false;

    allIn = false;
}

//-------------------- Getters --------------------

std::string Player::getName() const
{
    return name;
}

int Player::getChips() const
{
    return chips;
}

int Player::getCurrentBet() const
{
    return currentBet;
}

bool Player::isFolded() const
{
    return folded;
}

bool Player::isAllIn() const
{
    return allIn;
}

Hand& Player::getHand()
{
    return hand;
}


void Player::setHand(const Hand& hand)
{
    this->hand = hand;
}

void Player::clearHand()
{
    hand.clear();
}


void Player::bet(int amount)
{
    if(amount > chips)
    {
        amount = chips;
        allIn = true;
    }

    chips -= amount;

    currentBet += amount;
}

void Player::call(int amount)
{
    bet(amount);
}

void Player::raise(int amount)
{
    bet(amount);
}

void Player::fold()
{
    folded = true;
}

void Player::check()
{
    check_=true;
}

void Player::addChips(int amount)
{
    chips += amount;
}

void Player::resetRoundState()
{
    currentBet = 0;
    folded = false;
    allIn = false;
    check_ = false;
}

void Player::resetBet()
{
    currentBet = 0;
}

void Player::resetCheck()
{
    check_ = false;
}

bool Player::getCheck() const
{
    return check_;
}

void Player::displayPlayer() const
{
    std::cout << "Player : " << name << '\n';

    std::cout << "Chips  : " << chips << '\n';

    std::cout << "Bet    : " << currentBet << '\n';

    std::cout<<"check: "<< (check_ ? "Yes" : "No")<<"\n";

    std::cout << "Folded : " << (folded ? "Yes" : "No") << '\n';

    std::cout << "All In : " << (allIn ? "Yes" : "No") << '\n';

    hand.display();
}