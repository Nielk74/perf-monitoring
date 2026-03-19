#pragma once
#include <string>

struct CTrade
{
    std::string m_strTradeId;
    std::string m_strCounterparty;
    double      m_dNotional        = 0.0;
    bool        m_bRegulatoryFlag  = false;
    int         m_nPenaltyOverride = 0;   // 0 = use default penalty schedule
};

class CTradePriorityScorer
{
public:
    static CTradePriorityScorer* GetInstance();
    static void                  DestroyInstance();

    // Returns a priority score for the given trade.
    // Higher values indicate higher processing priority.
    // Range is typically [0, 10000]; regulatory-flagged trades receive a
    // penalty deduction and may score negative (down to -100).
    int Score(const CTrade& oTrade) const;

    void SetBaseScore(int nBase)       { m_nBaseScore = nBase; }
    void SetPenaltyAmount(int nAmount) { m_nPenaltyAmount = nAmount; }

private:
    CTradePriorityScorer();
    ~CTradePriorityScorer();

    static CTradePriorityScorer* s_pInstance;

    int m_nBaseScore     = 500;
    int m_nPenaltyAmount = 600;   // default penalty exceeds default base → negatives possible
};
