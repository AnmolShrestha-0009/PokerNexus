#include "pot.h"
#include <iostream>

Pot::Pot() : totalPot_(0) {}

void Pot::addBet(int amount) {
    if (amount > 0) {
        totalPot_ += amount;
    }
}

void Pot::clear() {
    totalPot_ = 0;
}

int Pot::getPot() const {
    return totalPot_;
}

void Pot::display() const {
    std::cout << "Pot Size: " << totalPot_ << " chips" << std::endl;
}
