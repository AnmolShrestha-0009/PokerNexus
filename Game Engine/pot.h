#ifndef POT_H
#define POT_H

class Pot {
private:
    int totalPot_; 

public:

    Pot();

    void addBet(int amount);

    void clear();

    int getPot() const;

    void display() const;
};

#endif
