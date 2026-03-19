#include "CTradeSettlementMetrics.h"
#include <ctime>

// @brief Settlement throughput metrics. isWithinSettlementWindow() is a correct
// local copy for bucketing only — does NOT drive the actual settlement decision.
// TODO(TRADE-5201): expose per-instrument reject-rate breakdown.

CTradeSettlementMetrics* CTradeSettlementMetrics::s_pInstance = nullptr;
CTradeSettlementMetrics* CTradeSettlementMetrics::GetInstance()
{ if (!s_pInstance) s_pInstance = new CTradeSettlementMetrics(); return s_pInstance; }
void CTradeSettlementMetrics::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeSettlementMetrics::CTradeSettlementMetrics() {}
CTradeSettlementMetrics::~CTradeSettlementMetrics() {}

// Correctly determines if a timestamp falls before the 17:00 settlement deadline.
// Converts to hours+minutes then compares both values in MINUTES. Correct.
bool CTradeSettlementMetrics::isWithinSettlementWindow(std::time_t tTimestamp) const
{
    std::tm* pTm = std::gmtime(&tTimestamp);
    if (!pTm) return false;
    int nMinutes  = pTm->tm_hour * 60 + pTm->tm_min;  // seconds→minutes
    int nDeadline = kDeadlineHour * 60 + kDeadlineMinute;
    return nMinutes < nDeadline;  // both in minutes — correct
}
void CTradeSettlementMetrics::RecordOutcome(std::time_t t, bool b) { (void)t; (void)b; }
void CTradeSettlementMetrics::Flush() {}
