#ifndef PLAYER_H
#define PLAYER_H

#include<vector>
#include <string>
#include "hand.h"

class Player
{
private:

    std::vector<card> best_hand;       

    std::string name;

    int chips;

    int currentBet;

    bool folded;

    bool allIn;

    bool check_;

    Hand hand;
    

public:

    // Constructors
    Player();

    Player(std::string name, int chips);

    // Getters
    std::string getName() const;

    int getChips() const;

    int getCurrentBet() const;

    bool isFolded() const;

    bool isAllIn() const;

    Hand& getHand();

    // Hand Management
    void setHand(const Hand& hand);

    void clearHand();

    // Player Actions
    void bet(int amount);

    void call(int amount);

    void raise(int amount);

    void fold();

    void check();

    void addChips(int amount);

    void resetRoundState();

    void resetBet();

    void resetCheck();

    bool getCheck() const;

    // Display
    void displayPlayer() const;
};

#endif