#include "CTradePriorityScorer.h"

CTradePriorityScorer* CTradePriorityScorer::s_pInstance = nullptr;

CTradePriorityScorer* CTradePriorityScorer::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePriorityScorer();
    return s_pInstance;
}

void CTradePriorityScorer::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradePriorityScorer::CTradePriorityScorer() {}
CTradePriorityScorer::~CTradePriorityScorer() {}

// Computes a numeric priority score for a trade.
//
// Base score is applied to every trade, giving it a positive priority
// in the queue. Trades flagged for regulatory review incur a penalty
// deduction — the intent is to deprioritise them relative to clean
// trades. The penalty is deliberately set higher than the base score so
// that flagged trades fall below zero, placing them at the back of any
// ordered queue.
//
// Valid output range: [-100, 10000].
// Negative scores are expected and correct for regulatory-flagged trades.
int CTradePriorityScorer::Score(const CTrade& oTrade) const
{
    int nScore = m_nBaseScore;

    if (oTrade.m_bRegulatoryFlag)
    {
        int nPenalty = (oTrade.m_nPenaltyOverride > 0)
                           ? oTrade.m_nPenaltyOverride
                           : m_nPenaltyAmount;
        nScore -= nPenalty;   // may produce a negative result — by design
    }

    return nScore;
}
