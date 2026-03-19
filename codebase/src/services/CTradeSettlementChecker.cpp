#include "CTradeSettlementChecker.h"

CTradeSettlementChecker* CTradeSettlementChecker::s_pInstance = nullptr;

CTradeSettlementChecker* CTradeSettlementChecker::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeSettlementChecker();
    return s_pInstance;
}

void CTradeSettlementChecker::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeSettlementChecker::CTradeSettlementChecker()
    : m_pConfig(CTradeSettlementConfig::GetInstance()),
      m_pNormalizer(CTradeTimestampNormalizer::GetInstance()) {}

CTradeSettlementChecker::~CTradeSettlementChecker() {}

// Returns true if the trade timestamp falls before the settlement deadline.
//
// The intra-day offset is obtained from CTradeTimestampNormalizer and
// compared against the deadline offset from CTradeSettlementConfig.
//
// BUG: dIntraDayOffset is in SECONDS (0–86399) while nDeadlineOffset is
// in MINUTES (0–1439).  After the first 1020 real seconds past midnight
// (i.e. 17 minutes past midnight), dIntraDayOffset >= nDeadlineOffset
// and every subsequent trade is rejected regardless of actual trade time.
bool CTradeSettlementChecker::IsBeforeDeadline(std::time_t tTradeTimestamp) const
{
    double dIntraDayOffset = m_pNormalizer->ComputeIntraDayOffset(tTradeTimestamp);
    int    nDeadlineOffset = m_pConfig->GetDeadlineOffset();

    return dIntraDayOffset < static_cast<double>(nDeadlineOffset);
}
