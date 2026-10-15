#ifndef BAYESIANBLUFF_H
#define BAYESIANBLUFF_H

struct BluffEvidence {
    double betToPotRatio;
    int streetIndex;
    bool boardIsScary;
    int consecutiveBarrels;
};

class BayesianBluffDetector {
private:
    double alpha_;
    double beta_;
    double priorBluffProb_;
    double currentLogOdds_;
    bool handInProgress_;

    double logLikelihoodRatio_BetSize(double betToPotRatio) const;
    double logLikelihoodRatio_BoardTexture(bool boardIsScary) const;
    double logLikelihoodRatio_Barrel(int consecutiveBarrels) const;

    static double sigmoid(double logOdds);
    static double logit(double probability);

public:
    BayesianBluffDetector();
    void startNewHand();
    double observeAggression(const BluffEvidence& evidence);
    void recordShowdownReveal(bool wasBluff);
    double getCurrentBluffProbability() const;
    double getLongRunBluffRate() const;
    int getObservationCount() const;
};

#endif
