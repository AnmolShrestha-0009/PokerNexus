#include "bayesianbluff.h"
#include <cmath>
#include <algorithm>

BayesianBluffDetector::BayesianBluffDetector()
    : alpha_(3.0), beta_(7.0),
      priorBluffProb_(0.3),
      currentLogOdds_(0.0),
      handInProgress_(false)
{
}

double BayesianBluffDetector::sigmoid(double logOdds) {
    return 1.0 / (1.0 + std::exp(-logOdds));
}

double BayesianBluffDetector::logit(double p) {
    p = std::max(1e-6, std::min(1.0 - 1e-6, p));
    return std::log(p / (1.0 - p));
}

void BayesianBluffDetector::startNewHand() {
    priorBluffProb_ = alpha_ / (alpha_ + beta_);
    currentLogOdds_ = logit(priorBluffProb_);
    handInProgress_ = true;
}

double BayesianBluffDetector::logLikelihoodRatio_BetSize(double r) const {
    const double valueCenter = 0.65;
    const double bluffCenterSmall = 0.25;
    const double bluffCenterLarge = 1.5;
    const double sigma = 0.35;

    double dValue = r - valueCenter;
    double dBluff = std::min(std::fabs(r - bluffCenterSmall), std::fabs(r - bluffCenterLarge));

    double logLikeValue = -(dValue * dValue) / (2.0 * sigma * sigma);
    double logLikeBluff = -(dBluff * dBluff) / (2.0 * sigma * sigma);

    return logLikeBluff - logLikeValue;
}

double BayesianBluffDetector::logLikelihoodRatio_BoardTexture(bool boardIsScary) const {
    return boardIsScary ? std::log(1.6) : std::log(1.0 / 1.3);
}

double BayesianBluffDetector::logLikelihoodRatio_Barrel(int consecutiveBarrels) const {
    if (consecutiveBarrels <= 1) return 0.0;
    if (consecutiveBarrels == 2) return std::log(0.85);
    return std::log(1.15);
}

double BayesianBluffDetector::observeAggression(const BluffEvidence& evidence) {
    if (!handInProgress_) {
        startNewHand();
    }

    double llr = 0.0;
    llr += logLikelihoodRatio_BetSize(evidence.betToPotRatio);
    llr += logLikelihoodRatio_BoardTexture(evidence.boardIsScary);
    llr += logLikelihoodRatio_Barrel(evidence.consecutiveBarrels);
    currentLogOdds_ += llr;

    return sigmoid(currentLogOdds_);
}

void BayesianBluffDetector::recordShowdownReveal(bool wasBluff) {
    if (wasBluff) alpha_ += 1.0;
    else          beta_  += 1.0;
    handInProgress_ = false;
}

double BayesianBluffDetector::getCurrentBluffProbability() const {
    return sigmoid(currentLogOdds_);
}

double BayesianBluffDetector::getLongRunBluffRate() const {
    return alpha_ / (alpha_ + beta_);
}

int BayesianBluffDetector::getObservationCount() const {
    return static_cast<int>(std::round((alpha_ - 3.0) + (beta_ - 7.0)));
}
