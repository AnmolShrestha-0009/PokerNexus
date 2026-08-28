#include "board.h"
#include <iostream>

// Constructor initializes empty board
Board::Board() {}

// Adds a card to the board if we haven't reached the 5 card limit
void Board::addCard(const card& c) {
    if (cards_.size() < 5) {
        cards_.push_back(c);
    }
}

// Clears all community cards
void Board::clear() {
    cards_.clear();
}

// Displays community cards side by side
void Board::display() const {
    std::cout << "Community Cards: ";
    if (cards_.empty()) {
        std::cout << "[ Empty ]";
    } else {
        for (const auto& c : cards_) {
            c.display();
            std::cout << "  ";
        }
    }
    std::cout << std::endl;
}

// Returns the vector of cards currently on the board
const std::vector<card>& Board::getCards() const {
    return cards_;
}
