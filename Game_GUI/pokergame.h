#ifndef POKERGAME_H
#define POKERGAME_H

#include <vector>
#include <string>
#include <raylib.h>
#include "card.h"
#include "deck.h"
#include "player.h"
#include "dealer.h"
#include "board.h"
#include "pot.h"
#include "handevaluator.h"
#include "montecarlo.h"
#include "bayesianbluff.h"

// Game states to manage Texas Hold'em round progression
enum class GameState {
    MENU,
    DEALING_HOLE,
    BETTING_PREFLOP,
    DEALING_FLOP,
    BETTING_FLOP,
    DEALING_TURN,
    BETTING_TURN,
    DEALING_RIVER,
    BETTING_RIVER,
    SHOWDOWN,
    ROUND_OVER
};

// Struct to represent a card dealing animation
struct CardAnimation {
    card cardValue;      // The card payload to be delivered
    Vector2 startPos;    // Start screen coordinates (deck)
    Vector2 endPos;      // Destination screen coordinates (player/board)
    float elapsedTime;   // Time elapsed since animation start
    float duration;      // Total duration of animation (0.25s)
    bool isCommunity;    // If true, card goes to board; else, goes to player
    int targetIdx;       // Target player index (0 or 1)
    bool active;         // If the animation is currently running
};

// Main controller class for the Texas Hold'em game and Raylib GUI
class PokerGame {
private:
    Dealer dealer_;                     // Deals cards, shuffles
    Board board_;                       // Stores community cards
    Pot pot_;                           // Stores pot total
    std::vector<Player> players_;       // Stores players (0: You, 1: Opponent AI)
    handEvaluator evaluator_;           // Evaluates poker hands

    // Analytics
    MonteCarloSimulator mcSim_;         // Estimates win probability via random rollouts — used for BOTH players
    BayesianBluffDetector bluffDetector_;      // Models the AI's own bluff frequency (feeds the human's HUD)
    BayesianBluffDetector humanBluffDetector_; // Models the HUMAN's bluff frequency (feeds the AI's decisions)
    EquityResult lastEquity_;           // Most recent Monte Carlo result for the human
    bool equityCalculated_;             // Whether lastEquity_ holds a value for the current board
    double lastBluffProb_;              // Most recent live bluff-probability reading for the AI (shown to human)
    int aiBarrelStreak_;                // Consecutive streets the AI has bet/raised this hand
    int humanBarrelStreak_;             // Consecutive streets the human has bet/raised this hand

    // Debug overlay (toggled with F1) — proves the AI's Monte Carlo /
    // Bayesian numbers are actually being computed, not just its final action
    bool debugMode_;
    double lastAiEquity_;           // AI's raw Monte Carlo win probability on its last decision
    double lastAiRequiredEquity_;   // Pot-odds break-even equity it was facing (0 if no bet to call)
    double lastAiAdjustedEquity_;   // Equity after the Bayesian bluff-read adjustment
    double lastHumanBluffRead_;     // humanBluffDetector_'s live reading at that moment

    // === ML INTEGRATION ===
    // Cached result of the last call to the Flask /predict server (app.py),
    // which serves RandomForest + ANN hand-category probability predictions.
    // Populated by updateMLPrediction(), read by draw() for live display.
    bool mlPredictionValid_;         // true once a prediction has been received
    int mlPlayerIdx_;                // which player (0=human, 1=AI) this prediction is for
    std::string mlRfTopCategory_;    // RF's highest-probability hand category, e.g. "Pair"
    double mlRfTopProb_;             // that category's probability, as a percentage (0-100)
    std::string mlAnnTopCategory_;   // ANN's highest-probability hand category
    double mlAnnTopProb_;            // that category's probability, as a percentage (0-100)
    std::string mlLastError_;        // non-empty if the last request failed (shown instead)
    bool mlPanelVisible_;            // === GUI IMPROVEMENT === panel stays hidden until requested

    GameState state_;                   // Current game phase state
    
    // Betting variables
    int currentTurn_;                   // 0 = Human, 1 = AI Opponent
    int currentBetLevel_;               // Maximum bet amount in the current round
    int minRaise_;                      // Minimum raise amount
    int raiseAmount_;                   // Human's raise selector value
    int lastRaiser_;                    // Track index of last player to raise
    int actionsThisRound_;              // Track how many actions taken in this round

    // Animation variables
    std::vector<CardAnimation> animQueue_; // Queue of card dealing animations
    CardAnimation currentAnim_;            // Currently playing animation

    // Message logging
    std::string statusMsg_;             // Message shown at the center/bottom
    std::string aiMsg_;                 // Message showing the AI's last action

    // Helper functions
    void checkAction(int playerIdx);
    void callAction(int playerIdx);
    void raiseAction(int playerIdx, int amount);
    void foldAction(int playerIdx);

    // AI decision making logic
    void processAITurn();
    void aiRaiseOrCall(int desiredRaiseAmount);

    // Ends a betting round and moves bets to the pot
    void endBettingRound();

    // Adds a card deal to the animation queue
    void queueCardDeal(const card& c, Vector2 start, Vector2 end, bool isCommunity, int playerIdx);

    // Determines if a betting round is completed
    bool checkBettingRoundComplete() const;

    // Checks if any player has gone all-in
    bool hasAllIn() const;

    // Maps the current betting GameState to a street index (0=preflop..3=river)
    int currentStreetIndex() const;

    // True if the board texture could plausibly complete a flush or straight
    // (used as public evidence for the Bayesian bluff detector)
    bool isBoardScary() const;

    // Pot size including bets not yet swept in (settled pot + both current bets)
    int getEffectivePot() const;

    // === ML INTEGRATION ===
    // Calls the Flask hand-strength server for the given player (0 or 1)
    // using their current hole cards + board, and caches the result in
    // mlRfTopCategory_ / mlAnnTopCategory_ / etc. Silently no-ops (leaving
    // mlPredictionValid_ false) on the river/showdown, since app.py only
    // supports 0/3/4 community cards.
    void updateMLPrediction(int playerIdx);

    // Draw helpers
    void drawSuit(float cx, float cy, Suit suit, Color color) const;
    void drawCard(float x, float y, const card& c, bool faceUp) const;
    void drawButton(Rectangle rect, const char* text, Color baseColor, Color hoverColor, bool enabled, bool& clicked) const;

    // === GUI IMPROVEMENT === decorative poker-chip icon (stacked rings + value label)
    void drawChipStack(float cx, float cy, int value, Color chipColor) const;

public:
    // Constructor
    PokerGame();

    // === ML INTEGRATION === destructor added to release libcurl's global state
    ~PokerGame();

    // Set up players
    void initializePlayers();

    // Starts a new poker round
    void startRound();

    // Initiates pre-flop betting and posts blinds
    void preFlop();

    // Deals 3 cards to the board and initiates flop betting
    void flop();

    // Deals 1 card to the board and initiates turn betting
    void turn();

    // Deals 1 card to the board and initiates river betting
    void river();

    // Evaluates hand strengths and reveals opponent cards
    void showdown();

    // Determines winner(s) and awards the pot
    void declareWinner();

    // Resets all state for a clean new round
    void resetRound();

    // Runs a Monte Carlo equity simulation for the human's current hand
    // and stores the result in lastEquity_ / equityCalculated_
    void calculateOdds();

    // Updates game logic, animation frames, and click interactions (runs in game loop)
    void update();

    // Draws all graphics, text, cards, buttons (runs in game loop)
    void draw();
};

#endif