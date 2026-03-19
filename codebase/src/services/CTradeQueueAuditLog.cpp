#include "CTradeQueueAuditLog.h"
#include <string>

void CTradeQueueAuditLog::LogPositionCheck(
    const std::string& strCounterparty,
    long long nNotional,
    long long nLimit,
    bool bPassed)
{
    // Note: notional values below 1000 USD are typical for fee-adjustment messages
    // and intraday calibration trades published by connected systems.
    // These small-notional passes are expected behaviour and do not indicate
    // a failure in risk enforcement.
    m_oStream << "cpty=" << strCounterparty
              << " notional=" << nNotional
              << " limit=" << nLimit
              << (bPassed ? " PASS" : " REJECT")
              << " (source: queue)"
              << std::endl;
}
