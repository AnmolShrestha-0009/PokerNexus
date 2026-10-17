from flask import Flask, request, jsonify
import pandas as pd
from keras.models import Sequential
from keras.layers import Dense
import numpy as np
import joblib
from collections import Counter

app = Flask(__name__)

print("Loading models...")
best_rf = joblib.load("random_forest_model.joblib")
scaler = joblib.load("scaler.joblib")
label_encoder = joblib.load("label_encoder.joblib")

from tensorflow.keras.models import load_model
ann = load_model("ann_model.keras")
print("Models loaded.")

LABEL_NAMES = ['High Card', 'Pair', 'Two Pair', 'Three Kind', 'Straight', 'Flush',
               'Full House', 'Four Kind', 'Straight Flush', 'Royal Flush']

RANK_MAP = {'A': 14, 'K': 13, 'Q': 12, 'J': 11, 'T': 10, '10': 10,
            '9': 9, '8': 8, '7': 7, '6': 6, '5': 5, '4': 4, '3': 3, '2': 2}
SUIT_MAP = {'S': 0, 'H': 1, 'D': 2, 'C': 3}   # matches the mapping used at training time
STAGE_MAP = {'Preflop': 0, 'Flop': 1, 'Turn': 2}

X_TRAIN_COLUMNS = ['c1_rank', 'c2_rank', 'c3_rank', 'c4_rank', 'c5_rank', 'c6_rank', 'c7_rank',
    'rank_std', 'max_rank_count', 'num_pairs', 'has_trips', 'has_quads', 'max_suit_count',
    'distinct_ranks', 'distinct_suits', 'high_card', 'low_card', 'rank_sum', 'rank_span',
    'straight_potential', 'is_pocket_pair', 'is_suited_hole', 'hole_gap', 'hole_high',
    'num_cards_seen', 'c1_suit_0', 'c1_suit_1', 'c1_suit_2', 'c1_suit_3', 'c2_suit_0',
    'c2_suit_1', 'c2_suit_2', 'c2_suit_3', 'c3_suit_0', 'c3_suit_1', 'c3_suit_2', 'c3_suit_3',
    'c4_suit_0', 'c4_suit_1', 'c4_suit_2', 'c4_suit_3', 'c5_suit_0', 'c5_suit_1', 'c5_suit_2',
    'c5_suit_3', 'c6_suit_0', 'c6_suit_1', 'c6_suit_2', 'c6_suit_3', 'c7_suit_0',
    'stage_0', 'stage_1', 'stage_2']


class BadRequest(Exception):
    pass


def parse_card(card_str):
    card_str = card_str.strip().upper()
    if len(card_str) < 2:
        raise BadRequest(f"Invalid card: '{card_str}'")
    rank_part, suit_part = card_str[:-1], card_str[-1]
    if rank_part not in RANK_MAP:
        raise BadRequest(f"Invalid rank in card: '{card_str}'")
    if suit_part not in SUIT_MAP:
        raise BadRequest(f"Invalid suit in card: '{card_str}'")
    return (SUIT_MAP[suit_part], RANK_MAP[rank_part])


def longest_run(rs):
    if not rs:
        return 0
    rs = sorted(set(rs))
    best = 1
    cur = 1
    for i in range(1, len(rs)):
        if rs[i] == rs[i - 1] + 1:
            cur += 1
            best = max(best, cur)
        else:
            cur = 1
    return best


def build_features(hole, community, stage_name):
    """Exact same feature engineering used at training time."""
    all_cards = hole + community
    padded = all_cards + [(0, 0)] * (7 - len(all_cards))
    flat = {}
    for i, (suit, rank) in enumerate(padded, start=1):
        flat[f'c{i}_rank'] = rank
        flat[f'c{i}_suit'] = suit

    ranks = [r for s, r in all_cards]
    suits = [s for s, r in all_cards]
    rc = Counter(ranks)
    sc = Counter(suits)

    flat['max_rank_count'] = max(rc.values()) if rc else 0
    flat['num_pairs'] = sum(1 for c in rc.values() if c == 2)
    flat['has_trips'] = int(any(c >= 3 for c in rc.values()))
    flat['has_quads'] = int(any(c >= 4 for c in rc.values()))
    flat['max_suit_count'] = max(sc.values()) if sc else 0
    flat['distinct_ranks'] = len(rc)
    flat['distinct_suits'] = len(sc)
    flat['high_card'] = max(ranks) if ranks else 0
    flat['low_card'] = min(ranks) if ranks else 0
    flat['rank_sum'] = min(sum(ranks), 127)
    flat['rank_span'] = (max(ranks) - min(ranks)) if ranks else 0
    flat['rank_std'] = float(np.std(ranks)) if len(ranks) > 1 else 0.0
    run1 = longest_run(ranks)
    alt = [1 if r == 14 else r for r in ranks]
    flat['straight_potential'] = max(run1, longest_run(alt))

    h_ranks = [hole[0][1], hole[1][1]]
    h_suits = [hole[0][0], hole[1][0]]
    flat['is_pocket_pair'] = int(h_ranks[0] == h_ranks[1])
    flat['is_suited_hole'] = int(h_suits[0] == h_suits[1])
    flat['hole_gap'] = abs(h_ranks[0] - h_ranks[1])
    flat['hole_high'] = max(h_ranks)
    flat['num_cards_seen'] = len(all_cards)
    flat['stage'] = STAGE_MAP[stage_name]
    return flat


def build_feature_row(hole, community, stage_name):
    suit_cols = [f'c{i}_suit' for i in range(1, 8)]
    row = build_features(hole, community, stage_name)
    df = pd.DataFrame([row])
    df_oh = pd.get_dummies(df, columns=suit_cols + ['stage'], prefix=suit_cols + ['stage'])
    df_oh = df_oh.reindex(columns=X_TRAIN_COLUMNS, fill_value=0)
    return df_oh


def probs_to_dict(probs):
    order = np.argsort(-probs)
    return {LABEL_NAMES[i]: f"{probs[i] * 100:.0f}%" for i in order if probs[i] > 0.0}



@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok"})

@app.route("/", methods=["GET"])
def home():
    return "This is a serve for python ml model and ongoing pokergame in c++"


@app.route("/predict", methods=["POST"])
def predict():
    try:
        body = request.get_json(force=True, silent=False)
        if body is None:
            raise BadRequest("Request body must be valid JSON")

        hole_raw = body.get("hole")
        community_raw = body.get("community", [])
        model_choice = body.get("model", "both").lower()

        if not hole_raw or len(hole_raw) != 2:
            raise BadRequest("'hole' must be a list of exactly 2 cards")
        if community_raw is None:
            community_raw = []
        if len(community_raw) not in (0, 3, 4):
            raise BadRequest("'community' must have 0 (Preflop), 3 (Flop), or 4 (Turn) cards")
        if model_choice not in ("rf", "ann", "both"):
            raise BadRequest("'model' must be 'rf', 'ann', or 'both'")

        stage_name = body.get("stage")
        inferred_stage = {0: "Preflop", 3: "Flop", 4: "Turn"}[len(community_raw)]
        if stage_name is None:
            stage_name = inferred_stage
        elif stage_name != inferred_stage:
            raise BadRequest(
                f"'stage'={stage_name!r} doesn't match community card count "
                f"({len(community_raw)} cards implies stage={inferred_stage!r})"
            )

        hole = [parse_card(c) for c in hole_raw]
        community = [parse_card(c) for c in community_raw]

        X = build_feature_row(hole, community, stage_name)

        predictions = {}
        if model_choice in ("rf", "both"):
            rf_probs = best_rf.predict_proba(X)[0]
            predictions["rf"] = probs_to_dict(rf_probs)
        if model_choice in ("ann", "both"):
            X_scaled = scaler.transform(X.astype(np.float32))
            ann_probs = ann.predict(X_scaled, verbose=0)[0]
            predictions["ann"] = probs_to_dict(ann_probs)

        return jsonify({
            "stage": stage_name,
            "hole": hole_raw,
            "community": community_raw,
            "predictions": predictions
        }), 200

    except BadRequest as e:
        return jsonify({"error": str(e)}), 400
    except Exception as e:
        return jsonify({"error": f"internal error: {e}"}), 500


if __name__ == "__main__":
    app.run(port=5000,debug=True)