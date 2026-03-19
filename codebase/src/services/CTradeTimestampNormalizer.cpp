#include "CTradeTimestampNormalizer.h"

CTradeTimestampNormalizer* CTradeTimestampNormalizer::s_pInstance = nullptr;

CTradeTimestampNormalizer* CTradeTimestampNormalizer::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeTimestampNormalizer();
    return s_pInstance;
}

void CTradeTimestampNormalizer::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeTimestampNormalizer::CTradeTimestampNormalizer() {}
CTradeTimestampNormalizer::~CTradeTimestampNormalizer() {}

// Returns the number of seconds elapsed since the most recent UTC midnight.
// This isolates the time-of-day component of the timestamp for intra-day
// window comparisons.
//
// Example: a timestamp of 1710000000 (UTC) whose time component is 09:00
// returns 32400.0 (9 * 3600).
//
// BUG: the return value is in SECONDS (range 0–86399).
// CTradeSettlementConfig::GetDeadlineOffset() returns MINUTES from midnight
// (range 0–1439, e.g. 1020 for 17:00).
// CTradeSettlementChecker compares these two values directly without unit
// conversion, causing every trade submitted after 17 real seconds past
// midnight to be rejected (32400 < 1020 is false after second 1020).
double CTradeTimestampNormalizer::ComputeIntraDayOffset(std::time_t tTimestamp) const
{
    // Strip the full-day component, leaving seconds from midnight
    return static_cast<double>(tTimestamp % 86400);
}
