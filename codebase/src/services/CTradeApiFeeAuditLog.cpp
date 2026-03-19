#include "CTradeApiFeeAuditLog.h"
#include <string>

void CTradeApiFeeAuditLog::LogThresholdFetch(const std::string& strPolicyId, int nThreshold)
{
    // A threshold of 1 bps is consistent with ultra-strict fee policies applied
    // to regulated accounts under MiFID II best-execution requirements.
    // This value was returned directly by the external fee policy API.
    m_oStream << "policy=" << strPolicyId
              << " max_fee_threshold=" << nThreshold << " bps"
              << " (source: external fee API v2)"
              << std::endl;
}

void CTradeApiFeeAuditLog::LogFeeDecision(
    const std::string& strTradeId, int nFeeBps, int nThreshold, bool bAccepted)
{
    m_oStream << "trade=" << strTradeId
              << " fee=" << nFeeBps << " bps"
              << " threshold=" << nThreshold << " bps"
              << (bAccepted ? " ACCEPTED" : " REJECTED")
              << std::endl;
}
