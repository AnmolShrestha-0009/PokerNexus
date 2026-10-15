
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER

#include "pokergame.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "pokergame.h"
#include <iostream>
#include <cstdio>
#include <cmath>
#include <algorithm>

// === ML INTEGRATION ===
// libcurl for the HTTP client (blended from the provided client.cpp) and
// nlohmann/json for building the request body and parsing app.py's nested
// JSON response. json.hpp is a new, single-header dependency — see the
// accompanying message for build-flag details (-lcurl, include path).
#include <curl/curl.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Constructor: Initializes the game state to the main menu
PokerGame::PokerGame() {
    state_ = GameState::MENU;
    currentTurn_ = 0;
    currentBetLevel_ = 0;
    minRaise_ = 20;
    raiseAmount_ = 20;
    lastRaiser_ = -1;
    actionsThisRound_ = 0;
    currentAnim_.active = false;
    statusMsg_ = "Welcome to Texas Hold'em Poker!";
    aiMsg_ = "";
    equityCalculated_ = false;
    lastEquity_ = EquityResult{0.0, 0.0, 0.0, 0};
    lastBluffProb_ = 0.0;
    aiBarrelStreak_ = 0;
    humanBarrelStreak_ = 0;
    debugMode_ = false;
    lastAiEquity_ = -1.0; // -1 = "no AI decision made yet"
    lastAiRequiredEquity_ = 0.0;
    lastAiAdjustedEquity_ = 0.0;
    lastHumanBluffRead_ = 0.0;

    // === ML INTEGRATION ===
    mlPredictionValid_ = false;
    mlPlayerIdx_ = -1;
    mlRfTopProb_ = 0.0;
    mlAnnTopProb_ = 0.0;
    mlPanelVisible_ = false; // === GUI IMPROVEMENT === hidden until the button is pressed
    curl_global_init(CURL_GLOBAL_DEFAULT); // once per process, matches client.cpp's main()

    initializePlayers();
}

// === ML INTEGRATION ===
// Mirrors client.cpp's curl_global_cleanup() call, which that sample made
// in main() — moved here since PokerGame now owns the libcurl session for
// its whole lifetime rather than a single one-shot request.
PokerGame::~PokerGame() {
    curl_global_cleanup();
}

// Sets up the human player and the AI opponent
void PokerGame::initializePlayers() {
    players_.clear();
    // Player 0 is the Human
    players_.push_back(Player("You", 1000));
    // Player 1 is the Opponent AI
    players_.push_back(Player("Opponent", 1000));
}

// Resets round properties for a clean slate
void PokerGame::resetRound() {
    board_.clear();
    pot_.clear();
    for (size_t i = 0; i < players_.size(); ++i) {
        players_[i].clearHand();
        players_[i].resetRoundState();
    }
    dealer_.resetDeck();
    currentBetLevel_ = 0;
    lastRaiser_ = -1;
    actionsThisRound_ = 0;
    aiMsg_ = "";
    statusMsg_ = "Dealt new hands. Make your move!";

    // Analytics reset: fresh equity reading, fresh in-hand bluff read seeded
    // from the long-run Beta posterior, and a clean barrel counter.
    equityCalculated_ = false;
    aiBarrelStreak_ = 0;
    humanBarrelStreak_ = 0;
    mlPanelVisible_ = false; // === GUI IMPROVEMENT === new hand, old prediction is stale
    bluffDetector_.startNewHand();
    humanBluffDetector_.startNewHand();
    lastBluffProb_ = bluffDetector_.getCurrentBluffProbability();
}

// Runs a Monte Carlo equity simulation for the human's current hole cards
// against a random opponent hand, given whatever community cards are known
// so far, and caches the result for draw() to display.
void PokerGame::calculateOdds() {
    const auto& myCards = players_[0].getHand().getCards();
    if (myCards.size() != 2) {
        equityCalculated_ = false;
        return;
    }

    lastEquity_ = mcSim_.estimateEquity(myCards, board_.getCards(), 3000);
    equityCalculated_ = true;
}

// Starts a new round, shuffles the deck, and queues dealing animations
void PokerGame::startRound() {
    resetRound();
    dealer_.shuffleDeck();

    // Get cards from the dealer's deck
    // We deal 2 cards for Player 0 (Human) and 2 for Player 1 (AI)
    card p0c1 = dealer_.dealTurn();
    card p1c1 = dealer_.dealTurn();
    card p0c2 = dealer_.dealTurn();
    card p1c2 = dealer_.dealTurn();

    // Set screen coordinates for animations
    Vector2 deckPos = { 950.0f, 350.0f };
    Vector2 p0Pos1 = { 540.0f, 580.0f };
    Vector2 p1Pos1 = { 540.0f, 120.0f };
    Vector2 p0Pos2 = { 620.0f, 580.0f };
    Vector2 p1Pos2 = { 620.0f, 120.0f };

    // Queue card-dealing animations sequentially
    queueCardDeal(p0c1, deckPos, p0Pos1, false, 0);
    queueCardDeal(p1c1, deckPos, p1Pos1, false, 1);
    queueCardDeal(p0c2, deckPos, p0Pos2, false, 0);
    queueCardDeal(p1c2, deckPos, p1Pos2, false, 1);

    // Set game state to animating hole cards
    state_ = GameState::DEALING_HOLE;
}

// Sets up pre-flop blinds and turn order
void PokerGame::preFlop() {
    // Human posts Small Blind (10 chips)
    players_[0].bet(10);
    // Opponent AI posts Big Blind (20 chips)
    players_[1].bet(20);

    currentBetLevel_ = 20;
    // Human acts first in heads-up preflop
    currentTurn_ = 0; 
    lastRaiser_ = 1; // Big blind acts as the last raise level
    actionsThisRound_ = 0;
    raiseAmount_ = currentBetLevel_ + 20; // Default raise size
    statusMsg_ = "Preflop betting round. Blinds posted.";
    state_ = GameState::BETTING_PREFLOP;

    // === ML INTEGRATION === (Step 3: engine hook)
    // First decision of the hand — no prior action flipped currentTurn_ to
    // trigger this, so it needs an explicit call here.
    updateMLPrediction(currentTurn_);
}

// Starts the flop stage: deals 3 community cards with animations
void PokerGame::flop() {
    endBettingRound();
    equityCalculated_ = false; // board is about to change; old equity reading is stale
    mlPanelVisible_ = false;   // === GUI IMPROVEMENT === old prediction no longer matches the board

    std::vector<card> flopCards = dealer_.dealFlop();

    Vector2 deckPos = { 950.0f, 350.0f };
    queueCardDeal(flopCards[0], deckPos, { 380.0f, 350.0f }, true, -1);
    queueCardDeal(flopCards[1], deckPos, { 470.0f, 350.0f }, true, -1);
    queueCardDeal(flopCards[2], deckPos, { 560.0f, 350.0f }, true, -1);

    state_ = GameState::DEALING_FLOP;
}

// Starts the turn stage: deals 1 community card with animation
void PokerGame::turn() {
    endBettingRound();
    equityCalculated_ = false;
    mlPanelVisible_ = false; // === GUI INTEGRATION ===

    card turnCard = dealer_.dealTurn();
    Vector2 deckPos = { 950.0f, 350.0f };
    queueCardDeal(turnCard, deckPos, { 650.0f, 350.0f }, true, -1);

    state_ = GameState::DEALING_TURN;
}

// Starts the river stage: deals 1 community card with animation
void PokerGame::river() {
    endBettingRound();
    equityCalculated_ = false;
    mlPanelVisible_ = false; // === GUI IMPROVEMENT === (river also isn't ML-supported — see step below)

    card riverCard = dealer_.dealRiver();
    Vector2 deckPos = { 950.0f, 350.0f };
    queueCardDeal(riverCard, deckPos, { 740.0f, 350.0f }, true, -1);

    state_ = GameState::DEALING_RIVER;
}

// Gathers players' bets and adds them to the pot
void PokerGame::endBettingRound() {
    for (size_t i = 0; i < players_.size(); ++i) {
        pot_.addBet(players_[i].getCurrentBet());
        players_[i].resetBet();
        players_[i].resetCheck();
    }
    currentBetLevel_ = 0;
    actionsThisRound_ = 0;
    lastRaiser_ = -1;
}

// Runs showdown: compares hand results, awards chips, and ends the round
void PokerGame::showdown() {
    endBettingRound();
    state_ = GameState::SHOWDOWN;
    declareWinner();
}

// Evaluates 7 cards for both players to determine the winner
void PokerGame::declareWinner() {
    // 1. Compile 7 cards for Player 0 (Human)
    std::vector<card> p0Seven = players_[0].getHand().getCards();
    const auto& community = board_.getCards();
    for (size_t i = 0; i < community.size(); ++i) {
        p0Seven.push_back(community[i]);
    }
    HandResult p0Result = evaluator_.getBestResult(p0Seven);

    // Symmetric with the AI-facing model below: only learns something when
    // the human was aggressive and the hand actually reached showdown.
    if (humanBarrelStreak_ > 0) {
        bool humanWasBluffing = (p0Result.rank == handRank::HIGH_CARD);
        humanBluffDetector_.recordShowdownReveal(humanWasBluffing);
    }

    // 2. Compile 7 cards for Player 1 (AI)
    std::vector<card> p1Seven = players_[1].getHand().getCards();
    for (size_t i = 0; i < community.size(); ++i) {
        p1Seven.push_back(community[i]);
    }
    HandResult p1Result = evaluator_.getBestResult(p1Seven);

    // Feed the long-run Bayesian bluff model: we only learn something here
    // because the hand actually reached showdown (not a fold-win), and only
    // if the AI was aggressive at some point this hand. A HIGH_CARD hand
    // that was bet/raised counts as a caught bluff; anything PAIR or better
    // counts as a genuine value hand.
    if (aiBarrelStreak_ > 0) {
        bool aiWasBluffing = (p1Result.rank == handRank::HIGH_CARD);
        bluffDetector_.recordShowdownReveal(aiWasBluffing);
    }

    int winnings = pot_.getPot();
    std::string p0HandName = evaluator_.getHandRankString(p0Result.rank);
    std::string p1HandName = evaluator_.getHandRankString(p1Result.rank);

    // 3. Compare hands and award pot
    if (p0Result > p1Result) {
        players_[0].addChips(winnings);
        statusMsg_ = "You win " + std::to_string(winnings) + " chips with " + p0HandName + "!";
    } else if (p1Result > p0Result) {
        players_[1].addChips(winnings);
        statusMsg_ = "Opponent wins " + std::to_string(winnings) + " chips with " + p1HandName + ".";
    } else {
        // Split pot tie
        int split = winnings / 2;
        players_[0].addChips(split);
        players_[1].addChips(split);
        statusMsg_ = "Split Pot! Both have " + p0HandName + ". (" + std::to_string(split) + " each)";
    }

    pot_.clear();
    state_ = GameState::ROUND_OVER;
}

// Helper to push card details into the animation queue
void PokerGame::queueCardDeal(const card& c, Vector2 start, Vector2 end, bool isCommunity, int playerIdx) {
    CardAnimation anim;
    anim.cardValue = c;
    anim.startPos = start;
    anim.endPos = end;
    anim.elapsedTime = 0.0f;
    anim.duration = 0.25f; // Animation takes 0.25 seconds
    anim.isCommunity = isCommunity;
    anim.targetIdx = playerIdx;
    anim.active = false;
    animQueue_.push_back(anim);
}

// Check Action implementation
void PokerGame::checkAction(int playerIdx) {
    players_[playerIdx].check();
    actionsThisRound_++;
    currentTurn_ = (currentTurn_ + 1) % 2;

    // === ML INTEGRATION === (Step 3) — right before the next player decides
    updateMLPrediction(currentTurn_);
}

// Call Action implementation
void PokerGame::callAction(int playerIdx) {
    int toCall = currentBetLevel_ - players_[playerIdx].getCurrentBet();
    players_[playerIdx].bet(toCall);
    actionsThisRound_++;
    currentTurn_ = (currentTurn_ + 1) % 2;

    // === ML INTEGRATION === (Step 3)
    updateMLPrediction(currentTurn_);
}

// Raise Action implementation
void PokerGame::raiseAction(int playerIdx, int amount) {
    int toRaise = amount - players_[playerIdx].getCurrentBet();
    players_[playerIdx].bet(toRaise);
    currentBetLevel_ = amount;
    lastRaiser_ = playerIdx;
    actionsThisRound_ = 1; // Reset actions counter to force opponents to act
    currentTurn_ = (currentTurn_ + 1) % 2;

    // === ML INTEGRATION === (Step 3)
    updateMLPrediction(currentTurn_);
}

// Fold Action: Ends the round immediately in favor of the opponent
void PokerGame::foldAction(int playerIdx) {
    players_[playerIdx].fold();
    actionsThisRound_++;

    // Other player wins
    int winnerIdx = (playerIdx + 1) % 2;
    endBettingRound();

    int winnings = pot_.getPot();
    players_[winnerIdx].addChips(winnings);
    pot_.clear();

    if (playerIdx == 0) {
        statusMsg_ = "You folded. Opponent wins " + std::to_string(winnings) + " chips!";
    } else {
        statusMsg_ = "Opponent folded. You win " + std::to_string(winnings) + " chips!";
    }

    state_ = GameState::ROUND_OVER;
}

// Maps the current betting state to a street index for the bluff detector
// (0 = preflop, 1 = flop, 2 = turn, 3 = river)
int PokerGame::currentStreetIndex() const {
    switch (state_) {
        case GameState::BETTING_PREFLOP: return 0;
        case GameState::BETTING_FLOP:    return 1;
        case GameState::BETTING_TURN:    return 2;
        case GameState::BETTING_RIVER:   return 3;
        default:                         return 0;
    }
}

// True if the board texture could plausibly complete a flush (3+ cards of
// one suit) or a straight (3+ distinct ranks within a 5-wide span). This is
// purely public information derived from the board, used as evidence for
// the Bayesian bluff detector — it never looks at anyone's hole cards.
bool PokerGame::isBoardScary() const {
    const auto& cards = board_.getCards();
    if (cards.size() < 3) return false;

    int suitCounts[4] = { 0, 0, 0, 0 };
    for (const auto& c : cards) {
        suitCounts[static_cast<int>(c.getSuit())]++;
    }
    for (int i = 0; i < 4; ++i) {
        if (suitCounts[i] >= 3) return true; // flush texture
    }

    std::vector<int> ranks;
    for (const auto& c : cards) {
        ranks.push_back(static_cast<int>(c.getRank()));
    }
    std::sort(ranks.begin(), ranks.end());
    ranks.erase(std::unique(ranks.begin(), ranks.end()), ranks.end());
    for (size_t i = 0; i + 2 < ranks.size(); ++i) {
        if (ranks[i + 2] - ranks[i] <= 4) return true; // straight texture
    }
    return false;
}

// Pot size including bets already placed this street but not yet swept in
// by endBettingRound(). This is the denominator used for bet-to-pot ratios.
int PokerGame::getEffectivePot() const {
    return pot_.getPot() + players_[0].getCurrentBet() + players_[1].getCurrentBet();
}

// === ML INTEGRATION ===
// Blended from client.cpp's WriteCallback: libcurl calls this repeatedly as
// response bytes arrive, and it's expected to append them into whatever
// std::string* was passed as CURLOPT_WRITEDATA.
static size_t MLWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// === ML INTEGRATION === (Step 2: feature pipeline, part 1 — card encoding)
// Converts our internal `card` (Suit/Rank enums) into the ASCII "RankSuit"
// notation app.py's parse_card() expects, e.g. Rank::ACE/Suit::SPADES -> "AS".
// This is the entire "feature extraction" needed on the C++ side: the real
// feature engineering (build_features() in app.py) runs server-side, so all
// the client has to get right is this string format.
static std::string cardToMLString(const card& c) {
    static const std::map<Rank, std::string> rankMap = {
        {Rank::TWO, "2"}, {Rank::THREE, "3"}, {Rank::FOUR, "4"}, {Rank::FIVE, "5"},
        {Rank::SIX, "6"}, {Rank::SEVEN, "7"}, {Rank::EIGHT, "8"}, {Rank::NINE, "9"},
        {Rank::TEN, "T"}, {Rank::JACK, "J"}, {Rank::QUEEN, "Q"}, {Rank::KING, "K"},
        {Rank::ACE, "A"},
    };
    // NOTE: these letters must match app.py's SUIT_MAP keys (S/H/D/C), which
    // is a completely separate concern from that map's internal numeric
    // values (S=0,H=1,D=2,C=3) — we're sending letters, not our own Suit
    // enum's ordinal, so our internal HEARTS=0..SPADES=3 ordering never
    // has to match the server's numbering at all.
    static const std::map<Suit, std::string> suitMap = {
        {Suit::HEARTS, "H"}, {Suit::DIAMONDS, "D"}, {Suit::CLUBS, "C"}, {Suit::SPADES, "S"},
    };
    return rankMap.at(c.getRank()) + suitMap.at(c.getSuit());
}

// === ML INTEGRATION === (Steps 2 & 3: feature pipeline + engine hook)
// Calls the Flask server (app.py) for whichever player is about to act,
// using their hole cards and the current board, and caches the parsed
// result for draw() to display. See callers below for exactly where this
// fires ("right before a player or bot makes a decision").
//
// Deliberately SKIPPED on the river / showdown: app.py's STAGE_MAP only
// knows Preflop/Flop/Turn (0/3/4 community cards) — sending 5 cards would
// get a 400 Bad Request, so we just don't call rather than "handle" that.
//
// Deliberately SYNCHRONOUS: this blocks the calling frame for as long as
// the HTTP round-trip takes. It only fires once per turn change (not once
// per frame — see call sites), so in practice that's a handful of times
// per hand, not 60/sec. A slow or unreachable Flask server will still
// stall that one frame; CURLOPT_TIMEOUT_MS below bounds how long.
void PokerGame::updateMLPrediction(int playerIdx) {
    mlPredictionValid_ = false;
    mlLastError_.clear();

    const auto& board = board_.getCards();
    if (board.size() != 0 && board.size() != 3 && board.size() != 4) {
        return; // River / showdown — unsupported by app.py, so skip silently
    }

    const auto& holeCards = players_[playerIdx].getHand().getCards();
    if (holeCards.size() != 2) {
        return; // Hole cards not dealt yet this frame
    }

    // ---- Build the request body (matches app.py's expected JSON exactly) ----
    json body;
    body["hole"] = { cardToMLString(holeCards[0]), cardToMLString(holeCards[1]) };
    json community = json::array();
    for (const auto& c : board) {
        community.push_back(cardToMLString(c));
    }
    body["community"] = community;
    body["model"] = "both";
    std::string jsonData = body.dump();

    // ---- Send it via libcurl (blended from client.cpp) ----
    CURL* curl = curl_easy_init();
    if (!curl) {
        mlLastError_ = "Failed to initialize CURL";
        return;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/predict");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, MLWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 800L); // bound the stall on a slow/dead server

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        mlLastError_ = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return;
    }

    // ---- Parse the response (matches app.py's response schema) ----
    try {
        json parsed = json::parse(response);
        if (parsed.contains("error")) {
            mlLastError_ = parsed["error"].get<std::string>();
        } else {
            // Each distribution is {"Category Name": "NN%", ...}; pick the
            // highest-probability entry rather than assuming key order.
            auto pickTop = [](const json& dist, std::string& outName, double& outProb) {
                outName.clear();
                outProb = -1.0;
                for (auto it = dist.begin(); it != dist.end(); ++it) {
                    std::string pctStr = it.value().get<std::string>(); // e.g. "62%"
                    double pct = std::stod(pctStr.substr(0, pctStr.size() - 1));
                    if (pct > outProb) {
                        outProb = pct;
                        outName = it.key();
                    }
                }
            };
            if (parsed["predictions"].contains("rf")) {
                pickTop(parsed["predictions"]["rf"], mlRfTopCategory_, mlRfTopProb_);
            }
            if (parsed["predictions"].contains("ann")) {
                pickTop(parsed["predictions"]["ann"], mlAnnTopCategory_, mlAnnTopProb_);
            }
            mlPlayerIdx_ = playerIdx;
            mlPredictionValid_ = true;
        }
    } catch (const std::exception& e) {
        mlLastError_ = std::string("Bad response JSON: ") + e.what();
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

// AI makes a bet or raise up to desiredRaiseAmount (clamped to its stack),
// feeding the Bayesian bluff detector with publicly observable evidence
// about the action before actually applying it.
void PokerGame::aiRaiseOrCall(int desiredRaiseAmount) {
    int maxPossible = players_[1].getChips() + players_[1].getCurrentBet();
    int amount = std::min(desiredRaiseAmount, maxPossible);

    int potBefore = getEffectivePot();
    int increment = amount - players_[1].getCurrentBet();
    double betToPotRatio = (potBefore > 0)
        ? static_cast<double>(increment) / static_cast<double>(potBefore)
        : 1.0;

    aiBarrelStreak_++;

    BluffEvidence ev;
    ev.betToPotRatio = betToPotRatio;
    ev.streetIndex = currentStreetIndex();
    ev.boardIsScary = isBoardScary();
    ev.consecutiveBarrels = aiBarrelStreak_;

    lastBluffProb_ = bluffDetector_.observeAggression(ev);

    if (amount > currentBetLevel_) {
        raiseAction(1, amount);
        aiMsg_ = "Opponent raises to " + std::to_string(amount) + ".";
    } else {
        // Not enough chips left to raise above the current level; treat as a call.
        callAction(1);
        aiMsg_ = "Opponent calls.";
    }
}

// AI decision-making routine.
//
// Uses the AI's own real-time Monte Carlo equity (the same
// MonteCarloSimulator the human's "Show Odds" button calls) compared
// against the pot odds it's being offered, adjusted by how much the AI
// currently believes the human has been bluffing this hand (via
// humanBluffDetector_, fed only by the human's own observable betting —
// never their hidden cards). This replaces the old separate preflop /
// postflop hand-rank heuristics with one equity-driven decision that
// behaves identically on every street, since MonteCarloSimulator handles
// an empty board (preflop) exactly the same way it handles a full one.
void PokerGame::processAITurn() {
    if (players_[1].isFolded() || players_[1].isAllIn()) {
        currentTurn_ = 0;
        return;
    }

    int toCall = currentBetLevel_ - players_[1].getCurrentBet();

    // The AI's own win probability right now. 1500 trials is enough for a
    // stable read and runs once per AI decision (not once per frame).
    const auto& aiHole = players_[1].getHand().getCards();
    EquityResult eq = mcSim_.estimateEquity(aiHole, board_.getCards(), 1500);
    double equity = eq.winProbability + 0.5 * eq.tieProbability; // count ties as half a win

    // Bayesian opponent read: the more we currently believe the human is
    // bluffing, the weaker we treat their represented hand as being, which
    // widens the AI's continuing range. Capped so a maxed-out bluff read
    // (100%) adds at most 0.20 equity, not an unbounded call-everything push.
    double humanBluffRead = humanBluffDetector_.getCurrentBluffProbability();
    double adjustedEquity = std::min(1.0, equity + 0.20 * humanBluffRead);

    // Cache for the debug overlay (F1) regardless of which branch runs below
    lastAiEquity_ = equity;
    lastAiAdjustedEquity_ = adjustedEquity;
    lastHumanBluffRead_ = humanBluffRead;

    if (toCall == 0) {
        lastAiRequiredEquity_ = 0.0; // nothing to call, no pot-odds threshold
        // Free to check or bet for value.
        if (adjustedEquity > 0.62) {
            aiRaiseOrCall(currentBetLevel_ + 40);
        } else {
            checkAction(1);
            aiBarrelStreak_ = 0; // checking breaks a barrel streak
            aiMsg_ = "Opponent checks.";
        }
    } else {
        // Facing a bet: compare adjusted equity to the pot odds being
        // offered. requiredEquity is the break-even win probability needed
        // to profitably call: toCall / (pot after the call).
        int potBefore = getEffectivePot();
        double requiredEquity = static_cast<double>(toCall) / static_cast<double>(potBefore + toCall);
        lastAiRequiredEquity_ = requiredEquity;

        if (adjustedEquity > requiredEquity + 0.20) {
            aiRaiseOrCall(currentBetLevel_ + 40); // comfortably ahead: raise for value
        } else if (adjustedEquity > requiredEquity) {
            callAction(1);                        // priced in: call
            aiMsg_ = "Opponent calls.";
        } else {
            foldAction(1);
            aiMsg_ = "Opponent folds!";
        }
    }
}

// Determines if the betting round is complete
bool PokerGame::checkBettingRoundComplete() const {
    // Count active non-folded, non-allin players
    int activeCount = 0;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (!players_[i].isFolded() && !players_[i].isAllIn()) {
            activeCount++;
        }
    }

    // If 1 or 0 players are active, betting is over (others folded or went all-in)
    if (activeCount <= 1) {
        return true;
    }

    // Ensure all players have had at least one action
    if (actionsThisRound_ < (int)players_.size()) {
        return false;
    }

    // Verify all active bets match the highest level
    for (size_t i = 0; i < players_.size(); ++i) {
        if (!players_[i].isFolded() && !players_[i].isAllIn()) {
            if (players_[i].getCurrentBet() != currentBetLevel_) {
                return false;
            }
        }
    }

    return true;
}

// Checks if any player is all-in
bool PokerGame::hasAllIn() const {
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].isAllIn()) return true;
    }
    return false;
}

// Updates game frames, animations, turn order, and state transitions
void PokerGame::update() {
    // Debug overlay toggle — checked first so it works even mid-animation
    if (IsKeyPressed(KEY_F1)) {
        debugMode_ = !debugMode_;
    }

    // 1. Update Card Animations
    if (currentAnim_.active) {
        currentAnim_.elapsedTime += GetFrameTime();
        if (currentAnim_.elapsedTime >= currentAnim_.duration) {
            currentAnim_.active = false;
            // Deliver card to Board or Player Hand
            if (currentAnim_.isCommunity) {
                board_.addCard(currentAnim_.cardValue);
            } else {
                players_[currentAnim_.targetIdx].getHand().addCard(currentAnim_.cardValue);
            }
        }
        return; // Pause game updates while cards are in flight
    }

    // 2. Load next card deal animation in queue
    if (!animQueue_.empty()) {
        currentAnim_ = animQueue_.front();
        animQueue_.erase(animQueue_.begin());
        currentAnim_.active = true;
        return;
    }

    // 3. Transition states if a dealing animation phase just finished
    if (state_ == GameState::DEALING_HOLE) {
        preFlop();
        return;
    }
    if (state_ == GameState::DEALING_FLOP) {
        state_ = GameState::BETTING_FLOP;
        currentTurn_ = 0; // Human acts first post-flop
        actionsThisRound_ = 0;
        raiseAmount_ = 20;
        statusMsg_ = "Flop betting round.";
        return;
    }
    if (state_ == GameState::DEALING_TURN) {
        state_ = GameState::BETTING_TURN;
        currentTurn_ = 0;
        actionsThisRound_ = 0;
        raiseAmount_ = 20;
        statusMsg_ = "Turn betting round.";
        return;
    }
    if (state_ == GameState::DEALING_RIVER) {
        state_ = GameState::BETTING_RIVER;
        currentTurn_ = 0;
        actionsThisRound_ = 0;
        raiseAmount_ = 20;
        statusMsg_ = "River betting round.";
        return;
    }

    // 4. Handle Turn execution
    if (state_ == GameState::BETTING_PREFLOP || state_ == GameState::BETTING_FLOP ||
        state_ == GameState::BETTING_TURN || state_ == GameState::BETTING_RIVER) {

        // Check if betting round has finished
        if (checkBettingRoundComplete()) {
            if (state_ == GameState::BETTING_PREFLOP) flop();
            else if (state_ == GameState::BETTING_FLOP) turn();
            else if (state_ == GameState::BETTING_TURN) river();
            else if (state_ == GameState::BETTING_RIVER) showdown();
            return;
        }

        // Opponent's turn: trigger simple AI choice
        if (currentTurn_ == 1) {
            processAITurn();
        }
    }
}

// Custom button renderer with click feedback
void PokerGame::drawButton(Rectangle rect, const char* text, Color baseColor, Color hoverColor, bool enabled, bool& clicked) const {
    if (!enabled) {
        DrawRectangleRec(rect, GRAY);
        DrawRectangleLinesEx(rect, 2, DARKGRAY);
        int fontSize = 20;
        int textWidth = MeasureText(text, fontSize);
        DrawText(text, rect.x + (rect.width - textWidth) / 2, rect.y + (rect.height - fontSize) / 2, fontSize, LIGHTGRAY);
        clicked = false;
        return;
    }

    Vector2 mousePos = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mousePos, rect);
    Color drawColor = hovered ? hoverColor : baseColor;

    DrawRectangleRec(rect, drawColor);
    DrawRectangleLinesEx(rect, 2, BLACK);

    int fontSize = 20;
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, rect.x + (rect.width - textWidth) / 2, rect.y + (rect.height - fontSize) / 2, fontSize, WHITE);

    clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Renders a suit symbol at (cx, cy) using basic geometric primitives
// (two circles + a triangle for hearts/clubs/spades, two triangles for a
// diamond) instead of a font glyph, matching the hand-drawn card look.
// === GUI IMPROVEMENT ===
// Redrawn as cleaner, more clearly distinguished vector icons. The previous
// version built spades and clubs from near-identical "N circles + triangle"
// constructions, which is exactly why they were hard to tell apart at card
// size (see the earlier hand-misreading discussion). Real card typography
// distinguishes them structurally, not just by which triangle is biggest:
// a spade is ONE smooth teardrop (merged/overlapping circles) with a sharp
// point on top, while a club is THREE separated round lobes with a small
// stem and no point at all. Each shape also gets a thin outline stroke
// (DrawCircleLines / DrawTriangleLines) for a crisper, less "blobby" look.
void PokerGame::drawSuit(float cx, float cy, Suit suit, Color color) const {
    switch (suit) {
        case Suit::HEARTS: {
            DrawCircle(cx - 6, cy - 4, 7, color);
            DrawCircle(cx + 6, cy - 4, 7, color);
            Vector2 v1 = { cx - 12.0f, cy - 2.0f };
            Vector2 v2 = { cx, cy + 14.0f };
            Vector2 v3 = { cx + 12.0f, cy - 2.0f };
            DrawTriangle(v1, v2, v3, color);
            DrawCircleLines(cx - 6, cy - 4, 7, BLACK);
            DrawCircleLines(cx + 6, cy - 4, 7, BLACK);
            DrawTriangleLines(v1, v2, v3, BLACK);
            break;
        }
        case Suit::DIAMONDS: {
            Vector2 top    = { cx, cy - 12.0f };
            Vector2 right  = { cx + 9.0f, cy };
            Vector2 bottom = { cx, cy + 12.0f };
            Vector2 left   = { cx - 9.0f, cy };
            DrawTriangle(top, left, bottom, color);
            DrawTriangle(top, bottom, right, color);
            DrawLineEx(top, right, 1.5f, BLACK);
            DrawLineEx(right, bottom, 1.5f, BLACK);
            DrawLineEx(bottom, left, 1.5f, BLACK);
            DrawLineEx(left, top, 1.5f, BLACK);
            break;
        }
        case Suit::CLUBS: {
            // Three clearly SEPARATED lobes (trefoil) — the gap between
            // them is the whole point, so each gets its own outline.
            Vector2 topLobe   = { cx, cy - 9.0f };
            Vector2 leftLobe  = { cx - 8.0f, cy + 3.0f };
            Vector2 rightLobe = { cx + 8.0f, cy + 3.0f };
            DrawCircle(topLobe.x, topLobe.y, 6, color);
            DrawCircle(leftLobe.x, leftLobe.y, 6, color);
            DrawCircle(rightLobe.x, rightLobe.y, 6, color);
            DrawCircleLines(topLobe.x, topLobe.y, 6, DARKGRAY);
            DrawCircleLines(leftLobe.x, leftLobe.y, 6, DARKGRAY);
            DrawCircleLines(rightLobe.x, rightLobe.y, 6, DARKGRAY);
            Vector2 s1 = { cx - 3.0f, cy + 6.0f };
            Vector2 s2 = { cx + 3.0f, cy + 6.0f };
            Vector2 s3 = { cx, cy + 17.0f };
            DrawTriangle(s1, s2, s3, color);
            DrawTriangleLines(s1, s2, s3, DARKGRAY);
            break;
        }
        case Suit::SPADES: {
            // ONE smooth teardrop: a sharp point on top, merged/overlapping
            // circles below it (distance between centers < sum of radii,
            // so there's no visible gap — unlike the club's three lobes).
            Vector2 tip  = { cx, cy - 16.0f };
            Vector2 bl   = { cx - 12.0f, cy + 2.0f };
            Vector2 br   = { cx + 12.0f, cy + 2.0f };
            DrawTriangle(bl, tip, br, color);
            DrawTriangleLines(bl, tip, br, DARKGRAY);
            DrawCircle(cx - 5, cy + 3, 8, color);
            DrawCircle(cx + 5, cy + 3, 8, color);
            DrawCircleLines(cx - 5, cy + 3, 8, DARKGRAY);
            DrawCircleLines(cx + 5, cy + 3, 8, DARKGRAY);
            Vector2 s1 = { cx - 3.0f, cy + 8.0f };
            Vector2 s2 = { cx + 3.0f, cy + 8.0f };
            Vector2 s3 = { cx, cy + 17.0f };
            DrawTriangle(s1, s2, s3, color);
            DrawTriangleLines(s1, s2, s3, DARKGRAY);
            break;
        }
    }
}

// === GUI IMPROVEMENT ===
// Decorative poker-chip icon: a filled disc, a couple of concentric outline
// rings to suggest the edge-stripe pattern real chips have, and a value
// label centered on top. Purely visual — takes a real value (blind/bet
// amount) so it never displays a number the game doesn't actually use.
void PokerGame::drawChipStack(float cx, float cy, int value, Color chipColor) const {
    DrawCircle(cx, cy, 16, chipColor);
    DrawCircleLines(cx, cy, 16, WHITE);
    DrawCircleLines(cx, cy, 11, WHITE);
    std::string label = std::to_string(value);
    int w = MeasureText(label.c_str(), 14);
    DrawText(label.c_str(), cx - w / 2, cy - 7, 14, WHITE);
}

// Renders individual cards as custom rounded shapes
void PokerGame::drawCard(float x, float y, const card& c, bool faceUp) const {
    Rectangle rect = { x, y, 70.0f, 100.0f };

    // Card white background or red/maroon back pattern
    DrawRectangleRounded(rect, 0.12f, 4, faceUp ? WHITE : RED);
    DrawRectangleRoundedLines(rect, 0.12f, 4, BLACK);

    if (faceUp) {
        std::string rankStr = c.convertRankToString(c.getRank());
        std::string suitStr = c.convertSuitToString(c.getSuit());

        Color txtColor = BLACK;
        if (suitStr == "H" || suitStr == "D") {
            txtColor = RED;
        }

        // Draw rank top-left
        char buf[8];
        sprintf(buf, "%s", rankStr.c_str());
        DrawText(buf, rect.x + 8, rect.y + 8, 20, txtColor);

        // Draw suit center-ish using custom geometric shapes
        drawSuit(rect.x + 35.0f, rect.y + 50.0f, c.getSuit(), txtColor);

        // Draw rank bottom-right
        DrawText(buf, rect.x + rect.width - 20, rect.y + rect.height - 25, 20, txtColor);
    } else {
        // Card Backing design
        Rectangle inner = { rect.x + 5.0f, rect.y + 5.0f, rect.width - 10.0f, rect.height - 10.0f };
        DrawRectangleRounded(inner, 0.12f, 4, MAROON);
        DrawText("?", rect.x + 27, rect.y + 35, 36, WHITE);
    }
}

// Renders the entire graphical UI layout
void PokerGame::draw() {
    BeginDrawing();
    Color charcoal = { 30, 30, 30, 255 };
    ClearBackground(charcoal);

    if (state_ == GameState::MENU) {
        // Draw main menu
        DrawText("TEXAS HOLD'EM POKER", 600 - MeasureText("TEXAS HOLD'EM POKER", 48) / 2, 200, 48, GOLD);
        DrawText("Computer Engineering Project", 600 - MeasureText("Computer Engineering Project", 24) / 2, 270, 24, LIGHTGRAY);

        bool startClicked = false;
        drawButton({ 475, 400, 250, 50 }, "Start New Game", DARKGREEN, LIME, true, startClicked);
        if (startClicked) {
            initializePlayers();
            startRound();
        }

        bool exitClicked = false;
        drawButton({ 475, 480, 250, 50 }, "Exit Game", MAROON, RED, true, exitClicked);
        if (exitClicked) {
            // Close window by sending window close event to Raylib
            // (or we will handle it in the main game loop)
            CloseWindow();
        }
    } else {
        // Draw main poker table view
        // Draw Green poker felt ellipse
        DrawEllipse(600, 400, 520, 320, Fade(DARKGREEN, 0.95f));
        DrawEllipseLines(600, 400, 520, 320, GOLD);

        // 2. Draw Dealer Deck representation
        Rectangle deckRect = { 950.0f, 350.0f, 70.0f, 100.0f };
        DrawRectangleRounded(deckRect, 0.12f, 4, DARKGRAY);
        DrawRectangleRoundedLines(deckRect, 0.12f, 4, BLACK);
        DrawText("DECK", deckRect.x + 12, deckRect.y + 40, 16, WHITE);

        // 3. Draw Community Cards on Board
        const auto& boardCards = board_.getCards();
        for (size_t i = 0; i < boardCards.size(); ++i) {
            drawCard(380.0f + i * 90.0f, 350.0f, boardCards[i], true);
        }

        // === GUI IMPROVEMENT ===
        // Consolidated info bar (Pot | Blinds | Current Bet) replacing the
        // old single "Pot: X chips" line, styled after the bordered info
        // panel in the reference image. Values are all real game state —
        // blinds are the fixed 10/20 posted in preFlop(), current bet is
        // currentBetLevel_, same number the betting logic itself uses.
        Rectangle infoBar = { 370.0f, 250.0f, 460.0f, 56.0f };
        DrawRectangleRounded(infoBar, 0.25f, 8, Fade(BLACK, 0.55f));
        DrawRectangleRoundedLines(infoBar, 0.25f, 8, GOLD);

        DrawText("POT", 395, 258, 12, LIGHTGRAY);
        std::string potVal = "$" + std::to_string(pot_.getPot());
        DrawText(potVal.c_str(), 395, 274, 20, GOLD);

        DrawLineEx({ 520, 258 }, { 520, 296 }, 1.0f, GRAY);
        DrawText("BLINDS", 535, 258, 12, LIGHTGRAY);
        DrawText("10 / 20", 535, 274, 20, GOLD);

        DrawLineEx({ 660, 258 }, { 660, 296 }, 1.0f, GRAY);
        DrawText("CURRENT BET", 675, 258, 12, LIGHTGRAY);
        std::string betVal = "$" + std::to_string(currentBetLevel_);
        DrawText(betVal.c_str(), 675, 274, 20, GOLD);

        // A couple of decorative chip icons next to the bar, using the same
        // real blind values rather than made-up denominations.
        drawChipStack(325, 278, 10, MAROON);
        drawChipStack(305, 278, 20, DARKBLUE);

        // 5. Draw Opponent AI (Top)
        DrawText(players_[1].getName().c_str(), 500, 50, 22, LIGHTGRAY);
        std::string aiChips = "Chips: " + std::to_string(players_[1].getChips());
        DrawText(aiChips.c_str(), 500, 80, 18, LIGHTGRAY);
        std::string aiBet = "Bet: " + std::to_string(players_[1].getCurrentBet());
        DrawText(aiBet.c_str(), 500, 100, 18, GOLD);

        const auto& aiCards = players_[1].getHand().getCards();
        if (aiCards.size() == 2) {
            // Cards are face-down until showdown or opponent folds
            bool reveal = (state_ == GameState::SHOWDOWN || state_ == GameState::ROUND_OVER);
            drawCard(540.0f, 120.0f, aiCards[0], reveal);
            drawCard(620.0f, 120.0f, aiCards[1], reveal);
        }

        // 6. Draw Human Player (Bottom)
        DrawText(players_[0].getName().c_str(), 500, 715, 22, WHITE);
        std::string myChips = "Chips: " + std::to_string(players_[0].getChips());
        DrawText(myChips.c_str(), 500, 740, 18, WHITE);
        std::string myBet = "Bet: " + std::to_string(players_[0].getCurrentBet());
        DrawText(myBet.c_str(), 650, 740, 18, GOLD);

        const auto& myCards = players_[0].getHand().getCards();
        if (myCards.size() == 2) {
            drawCard(540.0f, 580.0f, myCards[0], true);
            drawCard(620.0f, 580.0f, myCards[1], true);
        }

        // 7. Draw status messages in the middle
        DrawText(statusMsg_.c_str(), 600 - MeasureText(statusMsg_.c_str(), 20) / 2, 500, 20, YELLOW);
        if (!aiMsg_.empty()) {
            DrawText(aiMsg_.c_str(), 600 - MeasureText(aiMsg_.c_str(), 18) / 2, 230, 18, ORANGE);
        }

        // 7b. Bayesian bluff read — only meaningful once the AI has been
        // aggressive at least once this hand (aiBarrelStreak_ > 0)
        if (aiBarrelStreak_ > 0) {
            char bluffBuf[48];
            sprintf(bluffBuf, "Bluff read: %.0f%%", lastBluffProb_ * 100.0);
            DrawText(bluffBuf, 600 - MeasureText(bluffBuf, 18) / 2, 252, 18, PINK);
        }

        // 7c. AI decision debug overlay (F1) — proves the AI is actually
        // computing Monte Carlo equity + a Bayesian bluff read, not just
        // producing an action from nowhere.
        DrawText("F1: toggle AI debug info", 950, 20, 14, GRAY);
        if (debugMode_ && lastAiEquity_ >= 0.0) {
            char buf[96];
            int dy = 40;
            DrawText("--- AI decision (last turn) ---", 950, dy, 14, SKYBLUE); dy += 18;
            sprintf(buf, "Monte Carlo equity: %.1f%%", lastAiEquity_ * 100.0);
            DrawText(buf, 950, dy, 14, SKYBLUE); dy += 18;
            sprintf(buf, "Bayesian bluff read (you): %.1f%%", lastHumanBluffRead_ * 100.0);
            DrawText(buf, 950, dy, 14, SKYBLUE); dy += 18;
            sprintf(buf, "Adjusted equity: %.1f%%", lastAiAdjustedEquity_ * 100.0);
            DrawText(buf, 950, dy, 14, SKYBLUE); dy += 18;
            if (lastAiRequiredEquity_ > 0.0) {
                sprintf(buf, "Pot-odds required: %.1f%%", lastAiRequiredEquity_ * 100.0);
                DrawText(buf, 950, dy, 14, SKYBLUE); dy += 18;
            }
        }

        // === ML INTEGRATION === (Step 4: live output)
        // Shows whatever updateMLPrediction() last cached — updates every
        // time the turn passes (see checkAction/callAction/raiseAction/
        // preFlop above), not once per frame, since that's when a new
        // prediction is actually fetched.
        {
            const char* who = (mlPlayerIdx_ == 0) ? "You" : "Opponent";
            if (mlPanelVisible_ && mlPredictionValid_) {
                char buf[160];
                int dy = 170;
                sprintf(buf, "--- Predicted FINAL HAND (%s) ---", who);
                DrawText(buf, 950, dy, 14, LIME); dy += 16;
                DrawText("(category, not win %)", 950, dy, 11, GRAY); dy += 20;
                sprintf(buf, "RF:  %s likely (%.0f%%)", mlRfTopCategory_.c_str(), mlRfTopProb_);
                DrawText(buf, 950, dy, 14, LIME); dy += 18;
                sprintf(buf, "ANN: %s likely (%.0f%%)", mlAnnTopCategory_.c_str(), mlAnnTopProb_);
                DrawText(buf, 950, dy, 14, LIME);
            } else if (mlPanelVisible_ && !mlLastError_.empty()) {
                char buf[160];
                sprintf(buf, "ML server: %s", mlLastError_.c_str());
                DrawText(buf, 950, 170, 12, RED);
            }
        }

        // 8. Draw Turn indicator
        if (currentTurn_ == 0 && (state_ == GameState::BETTING_PREFLOP || state_ == GameState::BETTING_FLOP ||
                                  state_ == GameState::BETTING_TURN || state_ == GameState::BETTING_RIVER)) {
            DrawText("YOUR TURN", 600 - MeasureText("YOUR TURN", 20) / 2, 540, 20, GREEN);
        }

        // 9. Draw Card Deals Animation
        if (currentAnim_.active) {
            float t = currentAnim_.elapsedTime / currentAnim_.duration;
            if (t > 1.0f) t = 1.0f;
            float cx = currentAnim_.startPos.x + t * (currentAnim_.endPos.x - currentAnim_.startPos.x);
            float cy = currentAnim_.startPos.y + t * (currentAnim_.endPos.y - currentAnim_.startPos.y);
            drawCard(cx, cy, currentAnim_.cardValue, false);
        }

        // 10. Action Buttons (bottom left area) when it is user's turn
        bool bettingActive = (state_ == GameState::BETTING_PREFLOP || state_ == GameState::BETTING_FLOP ||
                              state_ == GameState::BETTING_TURN || state_ == GameState::BETTING_RIVER);
        bool isMyTurn = (currentTurn_ == 0 && bettingActive && !currentAnim_.active && animQueue_.empty());

        // Check Button is enabled if currentBetLevel matches my current bet
        bool canCheck = (isMyTurn && (currentBetLevel_ == players_[0].getCurrentBet()));
        // Call Button is enabled if currentBetLevel exceeds my current bet
        int toCallAmount = currentBetLevel_ - players_[0].getCurrentBet();
        bool canCall = (isMyTurn && (toCallAmount > 0));

        bool checkBtnClicked = false;
        drawButton({ 50, 600, 100, 40 }, "Check", DARKGREEN, LIME, canCheck, checkBtnClicked);
        if (checkBtnClicked) {
            checkAction(0);
            humanBarrelStreak_ = 0; // checking breaks a barrel streak
            statusMsg_ = "You checked.";
        }

        bool callBtnClicked = false;
        std::string callText = "Call (" + std::to_string(toCallAmount) + ")";
        drawButton({ 160, 600, 110, 40 }, toCallAmount > 0 ? callText.c_str() : "Call", DARKBLUE, BLUE, canCall, callBtnClicked);
        if (callBtnClicked) {
            callAction(0);
            statusMsg_ = "You called.";
        }

        // Raise Controls
        int maxPossibleRaise = players_[0].getChips() + players_[0].getCurrentBet();
        int minAllowedRaise = currentBetLevel_ + 20;
        if (minAllowedRaise > maxPossibleRaise) {
            minAllowedRaise = maxPossibleRaise;
        }
        if (raiseAmount_ < minAllowedRaise) {
            raiseAmount_ = minAllowedRaise;
        }
        // Clamp raiseAmount to remaining chips
        if (raiseAmount_ > maxPossibleRaise) {
            raiseAmount_ = maxPossibleRaise;
        }

        bool canRaise = (isMyTurn && (players_[0].getChips() > toCallAmount) && (raiseAmount_ >= minAllowedRaise) && (maxPossibleRaise > currentBetLevel_));

        bool raiseBtnClicked = false;
        std::string raiseText = "Raise (" + std::to_string(raiseAmount_) + ")";
        drawButton({ 280, 600, 120, 40 }, raiseText.c_str(), ORANGE, GOLD, canRaise, raiseBtnClicked);
        if (raiseBtnClicked) {
            // Feed the AI's model of the human with this raise's public
            // evidence *before* applying the action, exactly mirroring what
            // aiRaiseOrCall() does for the AI's own raises.
            int potBefore = getEffectivePot();
            int increment = raiseAmount_ - players_[0].getCurrentBet();
            double betToPotRatio = (potBefore > 0)
                ? static_cast<double>(increment) / static_cast<double>(potBefore)
                : 1.0;

            humanBarrelStreak_++;
            BluffEvidence ev;
            ev.betToPotRatio = betToPotRatio;
            ev.streetIndex = currentStreetIndex();
            ev.boardIsScary = isBoardScary();
            ev.consecutiveBarrels = humanBarrelStreak_;
            humanBluffDetector_.observeAggression(ev);

            raiseAction(0, raiseAmount_);
            statusMsg_ = "You raised to " + std::to_string(raiseAmount_) + "!";
        }

        // "-" Button to decrease raise amount
        bool decClicked = false;
        drawButton({ 410, 600, 30, 40 }, "-", DARKGRAY, GRAY, isMyTurn, decClicked);
        if (decClicked) {
            raiseAmount_ -= 20;
            if (raiseAmount_ < minAllowedRaise) raiseAmount_ = minAllowedRaise;
        }

        // "+" Button to increase raise amount
        bool incClicked = false;
        drawButton({ 450, 600, 30, 40 }, "+", DARKGRAY, GRAY, isMyTurn, incClicked);
        if (incClicked) {
            raiseAmount_ += 20;
            int maxPossibleRaise = players_[0].getChips() + players_[0].getCurrentBet();
            if (raiseAmount_ > maxPossibleRaise) raiseAmount_ = maxPossibleRaise;
        }

        // Fold Button
        bool foldBtnClicked = false;
        drawButton({ 50, 650, 100, 40 }, "Fold", DARKGRAY, RED, isMyTurn, foldBtnClicked);
        if (foldBtnClicked) {
            foldAction(0);
        }

        // "Show Odds" Button — runs a Monte Carlo equity simulation for the
        // human's current hole cards + board. Available any time the human
        // has hole cards and no animation is in flight (not just on your turn).
        bool canShowOdds = (myCards.size() == 2 && !currentAnim_.active && animQueue_.empty()
                             && bettingActive);
        bool oddsBtnClicked = false;
        drawButton({ 950, 600, 180, 40 }, "Show Odds", DARKBLUE, BLUE, canShowOdds, oddsBtnClicked);
        if (oddsBtnClicked) {
            calculateOdds();
        }
        if (equityCalculated_) {
            char oddsBuf[64];
            sprintf(oddsBuf, "Win %.0f%%  Tie %.0f%%  Lose %.0f%%",
                    lastEquity_.winProbability * 100.0,
                    lastEquity_.tieProbability * 100.0,
                    lastEquity_.loseProbability * 100.0);
            DrawText(oddsBuf, 950, 650, 16, SKYBLUE);
        }

        // === GUI IMPROVEMENT ===
        // "ML Prediction" Button — on-demand, mirrors "Show Odds" exactly.
        // The panel stays hidden (see mlPanelVisible_ resets above) until
        // this is pressed, instead of silently populating from the
        // automatic per-turn calls in checkAction/callAction/raiseAction —
        // keeps the screen uncluttered and puts the human in control of
        // when to consult it. Disabled on the river: app.py doesn't support
        // a 5-card board, so there's nothing useful to request there.
        bool canShowMLPrediction = (myCards.size() == 2 && !currentAnim_.active && animQueue_.empty()
                                     && bettingActive && board_.getCards().size() != 5);
        bool mlBtnClicked = false;
        drawButton({ 950, 700, 180, 40 }, "ML Prediction", DARKGREEN, GREEN, canShowMLPrediction, mlBtnClicked);
        if (mlBtnClicked) {
            updateMLPrediction(0); // explicit fetch for the human, not whichever player last triggered a turn-change
            mlPanelVisible_ = true;
        }

        // "Next Round" Button (ROUND_OVER)
        if (state_ == GameState::ROUND_OVER) {
            bool nextClicked = false;
            // Check if game over (one player out of chips)
            bool gameOver = (players_[0].getChips() <= 0 || players_[1].getChips() <= 0);
            
            if (gameOver) {
                std::string winText = (players_[0].getChips() > 0) ? "YOU WON THE GAME!" : "OPPONENT WON THE GAME!";
                DrawText(winText.c_str(), 600 - MeasureText(winText.c_str(), 32) / 2, 320, 32, GOLD);
                
                drawButton({ 500, 600, 200, 50 }, "Return to Menu", DARKGREEN, LIME, true, nextClicked);
                if (nextClicked) {
                    state_ = GameState::MENU;
                }
            } else {
                drawButton({ 950, 680, 180, 50 }, "Next Round", DARKGREEN, LIME, true, nextClicked);
                if (nextClicked) {
                    startRound();
                }
            }
        }
    }

    EndDrawing();
}