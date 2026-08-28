#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include "card.h"

// The Board class stores community cards on the poker table.
class Board {
private:
    std::vector<card> cards_; // stores community cards (max 5)

public:
    // Constructor
    Board();

    // Adds a card to the board
    void addCard(const card& c);

    // Clears all cards from the board
    void clear();

    // Prints the community cards to standard output
    void display() const;

    // Returns a reference to the community cards
    const std::vector<card>& getCards() const;
};

#endif
