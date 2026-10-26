<div align="center">

# ♠️ Texas Hold'em Poker Simulator

### A real-time C++ poker engine with Monte Carlo equity, Bayesian bluff detection, and a Python ML prediction service

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![raylib](https://img.shields.io/badge/GUI-raylib-000000?logo=raylib&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.x-3776AB?logo=python&logoColor=white)
![Flask](https://img.shields.io/badge/API-Flask-000000?logo=flask&logoColor=white)
![scikit-learn](https://img.shields.io/badge/ML-scikit--learn-F7931E?logo=scikitlearn&logoColor=white)
![TensorFlow](https://img.shields.io/badge/ML-TensorFlow%2FKeras-FF6F00?logo=tensorflow&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green)

</div>

---

## 📖 Table of Contents

- [About](#-about)
- [Key Features](#-key-features)
- [Architecture](#-architecture)
- [Tech Stack](#-tech-stack)
- [Getting Started](#-getting-started)
  - [Prerequisites](#prerequisites)
  - [Build the Game (C++)](#build-the-game-c)
  - [Run the ML Server (Optional)](#run-the-ml-server-optional)
- [Usage](#-usage)
  - [Controls](#controls)
  - [ML Server API](#ml-server-api)
  - [Debug Overlay](#debug-overlay)
- [Project Structure](#-project-structure)
- [Known Limitations](#-known-limitations)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)
- [License](#-license)
- [Acknowledgments](#-acknowledgments)

---

## 📌 About

**Texas Hold'em Poker Simulator** is a heads-up (2-player) poker game written in modern C++17 with a [raylib](https://www.raylib.com/) graphical interface. Beyond a complete, rules-accurate engine, it embeds probabilistic and machine-learning decision support directly into gameplay:

- A **Monte Carlo equity simulator** estimates win probability by random sampling.
- A **Bayesian bluff detector** infers bluff likelihood from publicly observable betting behavior only — never from hidden cards.
- An optional **Python/Flask service** serves pre-trained Random Forest and ANN models that predict the final hand category, consumed by the C++ client over HTTP via libcurl.

The AI opponent decides using live Monte Carlo equity compared against pot odds, adjusted by its Bayesian read on you. The project doubles as a reference implementation of a layered game architecture and a practical example of C++/Python ML integration.

> **Screenshot / Demo:** _None yet — add a screenshot or GIF here._

> <img width="598" height="394" alt="image" src="https://github.com/user-attachments/assets/b9f98ca1-4983-4263-9872-d6142a460582" />


---

## ✨ Key Features

- 🃏 **Complete Texas Hold'em engine** — hole cards, flop/turn/river, blinds, betting rounds, and a brute-force best-5-of-7 hand evaluator with correct tie-breaking.
- 🎲 **Monte Carlo equity simulator** — on-demand win/tie/lose probability for the human via the **Show Odds** button; also drives the AI's own decisions.
- 🧠 **Bayesian bluff detection** — two-layer model (Beta-Bernoulli long-run frequency + log-odds Naive Bayes for in-hand evidence), run symmetrically for both players.
- 🤖 **Equity-driven AI opponent** — compares its own Monte Carlo equity against pot odds, adjusted by its Bayesian read on the human.
- 🔌 **ML prediction service** — Flask REST API serving Random Forest + ANN hand-category predictions, called from C++ through libcurl.
- 🖥️ **raylib GUI** — animated card dealing, vector-drawn suit icons, info bar, chip icons, and clickable betting controls.
- 🔍 **Debug overlay (`F1`)** — shows the AI's actual computed equity, bluff read, and pot-odds threshold on its last decision.

---

## 🏗️ Architecture

`PokerGame` is the single controller, composing four layers. The ML service is an optional, separate process.

```text
                        ┌────────────────────────┐
                        │        main.cpp        │
                        │  raylib window · 60FPS │
                        └───────────┬────────────┘
                                    │ update() / draw()
                  ┌─────────────────▼──────────────────┐
                  │        PokerGame (controller)      │
                  │  GameState machine · betting · AI  │
                  └──┬───────────────┬──────────────┬──┘
                     │               │              │
          ┌──────────▼───┐   ┌───────▼────────┐  ┌──▼───────────┐
          │ Domain Layer │   │ Analytics Layer│  │  Rendering   │
          │ card, deck,  │   │ MonteCarlo-    │  │  raylib draw │
          │ Hand, Player,│   │ Simulator      │  │  calls, GUI  │
          │ Board, Pot,  │   │ BayesianBluff- │  │              │
          │ Dealer,      │   │ Detector       │  │              │
          │ handEvaluator│   │ (read-only)    │  │              │
          └──────────────┘   └────────────────┘  └──────────────┘
                     │
                     │ updateMLPrediction()  (libcurl, HTTP/JSON)
                     ▼
          ┌──────────────────────────────────────────┐
          │  Flask server (app.py) — optional        │
          │  build_features() → Random Forest / ANN  │
          │  → hand-category probabilities           │
          └──────────────────────────────────────────┘
```

**Design principle:** the analytics layer only *reads* public getters from the domain layer and never mutates game state, so it can be extended or removed without touching the rules engine.

---

## 🛠️ Tech Stack

| Category | Technologies |
|---|---|
| **Game engine / GUI** | C++17, [raylib](https://www.raylib.com/) |
| **C++ ↔ Python transport** | [libcurl](https://curl.se/libcurl/), [nlohmann/json](https://github.com/nlohmann/json) |
| **ML serving** | Python 3, [Flask](https://flask.palletsprojects.com/) |
| **ML models** | scikit-learn (Random Forest), TensorFlow/Keras (ANN) |
| **Model persistence** | joblib, HDF5 (`.h5`) |

---

## 🚀 Getting Started

### Prerequisites

**For the game (required):**

- A C++17 compiler (g++, clang++, or MSVC)
- [raylib](https://www.raylib.com/) ≥ 4.5 — [installation guide](https://github.com/raysan5/raylib/wiki)
- libcurl development headers
- nlohmann/json (single-header library)

**For the ML server (optional):**

- Python 3 with `flask`, `joblib`, `pandas`, `numpy`, `scikit-learn`, `tensorflow`
- Pre-trained model artifacts in the server's working directory:
  `random_forest_model.joblib`, `scaler.joblib`, `label_encoder.joblib`, `ann_model.h5`

> ⚠️ Model artifacts are **not** included in this repository by default. Regenerate them from the training notebooks, or place your own files with these exact names.

### Build the Game (C++)

Clone the repository:

```bash
git clone https://github.com/AnmolShrestha-0009/PokerNexus.git
```

Install dependencies (examples — adapt to your platform):

```bash
# Debian / Ubuntu
sudo apt install libraylib-dev libcurl4-openssl-dev nlohmann-json3-dev

# macOS (Homebrew)
brew install raylib curl nlohmann-json

# Windows (MSYS2 / MinGW-w64)
pacman -S mingw-w64-x86_64-raylib mingw-w64-x86_64-curl mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-gcc
```

Compile. List the sources explicitly (avoid a `*.cpp` glob if any standalone sample files with their own `main()` exist in the folder):

```bash
# Linux
g++ -std=c++17 -O2 \
    main.cpp pokergame.cpp card.cpp deck.cpp hand.cpp board.cpp player.cpp \
    pot.cpp dealer.cpp handevaluator.cpp montecarlo.cpp bayesianbluff.cpp \
    -o poker_game \
    -lraylib -lcurl -lGL -lm -lpthread -ldl -lrt -lX11
```

```bash
# macOS
g++ -std=c++17 -O2 \
    main.cpp pokergame.cpp card.cpp deck.cpp hand.cpp board.cpp player.cpp \
    pot.cpp dealer.cpp handevaluator.cpp montecarlo.cpp bayesianbluff.cpp \
    -o poker_game \
    -lraylib -lcurl -framework CoreVideo -framework IOKit -framework Cocoa -framework OpenGL
```

```bash
# Windows (MSYS2 MinGW-w64 shell)
g++ -std=c++17 -O2 \
    main.cpp pokergame.cpp card.cpp deck.cpp hand.cpp board.cpp player.cpp \
    pot.cpp dealer.cpp handevaluator.cpp montecarlo.cpp bayesianbluff.cpp \
    -o poker_game.exe \
    -lraylib -lcurl -lopengl32 -lgdi32 -lwinmm
```

Run:

```bash
./poker_game        # Linux / macOS
poker_game.exe      # Windows
```

> The game is fully playable **without** the ML server. The ML panel only reports an error if you press its button while no server is reachable.

### Run the ML Server (Optional)

```bash
pip install flask joblib pandas numpy scikit-learn tensorflow
python3 app.py
# listening on http://0.0.0.0:5000
```

The C++ client currently targets `http://127.0.0.1:5000/predict` (hardcoded in `PokerGame::updateMLPrediction()`). Verify the server is up:

```bash
curl http://127.0.0.1:5000/health
```

### Configuration

There are no configuration files. Tunable values live in source:

| Setting | Location |
|---|---|
| ML server URL | `updateMLPrediction()` in `pokergame.cpp` |
| ML request timeout (800 ms) | `CURLOPT_TIMEOUT_MS` in `pokergame.cpp` |
| AI Monte Carlo trials per decision | `estimateEquity(..., 1500)` call in `processAITurn()` |
| Blinds (10 / 20) | `preFlop()` in `pokergame.cpp` |

---

## 💻 Usage

### Controls

| Action | How |
|---|---|
| Check / Call / Raise / Fold | Click the on-screen buttons |
| Adjust raise amount | `-` / `+` buttons beside **Raise** |
| Show win probability | **Show Odds** button (Monte Carlo simulation) |
| Show predicted final hand | **ML Prediction** button (requires the Flask server; disabled on the river) |
| Toggle AI debug overlay | `F1` |
| Start the next hand | **Next Round** button after a hand ends |

### ML Server API

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/health` | Liveness check |
| `POST` | `/predict` | Hand-category probability distribution |

Card notation: rank `2-9, T, J, Q, K, A` + suit `S, H, D, C` (e.g. `"AS"`, `"TD"`).

**Request:**

```bash
curl -X POST http://127.0.0.1:5000/predict \
  -H "Content-Type: application/json" \
  -d '{
        "hole": ["AS", "KH"],
        "community": ["QD", "JC", "TS"],
        "model": "both"
      }'
```

`community` must contain **0, 3, or 4** cards (Preflop / Flop / Turn). `model` is `"rf"`, `"ann"`, or `"both"`.

**Response shape:**

```json
{
  "stage": "Flop",
  "hole": ["AS", "KH"],
  "community": ["QD", "JC", "TS"],
  "predictions": {
    "rf":  { "<Hand Category>": "<NN>%", "...": "..." },
    "ann": { "<Hand Category>": "<NN>%", "...": "..." }
  }
}
```

> ℹ️ **Important:** these percentages are the probability that your *final hand* falls into each **category** (High Card, Pair, Two Pair, …). They are **not** win probabilities. Use **Show Odds** for win/tie/lose equity.

### Debug Overlay

Press `F1` in-game to display the AI's most recent decision inputs:

```text
--- AI decision (last turn) ---
Monte Carlo equity: <value>
Bayesian bluff read (you): <value>
Adjusted equity: <value>
Pot-odds required: <value>
```

These are the same variables `processAITurn()` branches on.

---

## 📂 Project Structure

```text
.
├── main.cpp                  # Entry point: raylib window + game loop
├── pokergame.h / .cpp        # State machine, betting, AI, GUI, ML client
├── card.h / .cpp             # Playing card (suit, rank)
├── deck.h / .cpp             # 52-card deck: build, shuffle, deal
├── hand.h / .cpp             # A player's hole cards
├── board.h / .cpp            # Community cards
├── player.h / .cpp           # Chips, bet state, fold/all-in
├── pot.h / .cpp              # Pot total
├── dealer.h / .cpp           # Deals hole cards and community cards
├── handevaluator.h / .cpp    # Best-5-of-7 evaluation + tie-breakers
├── montecarlo.h / .cpp       # Monte Carlo equity simulator (no raylib dependency)
├── bayesianbluff.h / .cpp    # Bayesian bluff detector (no raylib dependency)
├── app.py                    # Flask ML prediction server
└── poker_winloss_ensemble.ipynb   # Win-probability meta-model notebook
```

---

## ⚠️ Known Limitations

- **Heads-up only** — no multi-way pots or side pots.
- **ML endpoint supports Preflop, Flop, and Turn only** — the river (5 community cards) is not supported by the server's feature pipeline.
- **Blocking ML call** — the HTTP request is synchronous (800 ms timeout); an unreachable server briefly stalls the frame that triggered it. The AI's own decisions do **not** depend on the ML server.
- **Bayesian parameters are hand-tuned**, not fit to real hand-history data, and reset each session.
- **Fixed blinds / positions** — the human always posts the small blind; there is no dealer-button rotation.
- **ML models are forward-looking** — they predict a hand's final category from partial information, so part of their error is irreducible uncertainty (especially preflop).

---

## 🗺️ Roadmap

- [ ] Asynchronous (non-blocking) ML requests
- [ ] Hand-history logger for training and evaluation data
- [ ] Data-fit Bayesian likelihoods (replace hand-tuned parameters)
- [ ] Neural-network equity estimator trained against the Monte Carlo simulator
- [ ] Persist opponent-model parameters across sessions
- [ ] Dealer-button rotation (alternating blinds)
- [ ] Multi-way pots and side-pot handling
- [ ] Learned or game-theoretic AI policy (e.g. CFR-based)
- [ ] Screenshots / demo GIF in this README

---

## 🤝 Contributing

Contributions are welcome. For larger changes, please open an issue first to discuss what you would like to change.

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Commit your changes: `git commit -m "Add my feature"`
4. Push the branch: `git push origin feature/my-feature`
5. Open a Pull Request

---

## 📄 License

Distributed under the **MIT License** *(placeholder — replace with your chosen license and add a matching `LICENSE` file)*.

---

## 🙏 Acknowledgments

- [raylib](https://www.raylib.com/) — graphics and input
- [libcurl](https://curl.se/libcurl/) and [nlohmann/json](https://github.com/nlohmann/json) — C++ HTTP and JSON
- [Flask](https://flask.palletsprojects.com/), [scikit-learn](https://scikit-learn.org/), [TensorFlow](https://www.tensorflow.org/) — ML serving and models
- D. Sklansky, *The Theory of Poker* — equity concepts
- Southey et al., *"Bayes' Bluff: Opponent Modelling in Poker"* (UAI 2005) — inspiration for the Bayesian approach
- Metropolis & Ulam, *"The Monte Carlo Method"* (1949) — basis of the equity simulator
