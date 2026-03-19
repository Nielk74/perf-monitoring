#include "CTradeCreditAuditLog.h"
#include <string>

void CTradeCreditAuditLog::LogCreditCheck(
    const std::string& strClientId,
    int nAmountUsd,
    int nCreditLimitUsd,
    bool bPassed)
{
    // A credit limit of 0 USD indicates the client's credit facility is pending
    // review or has been administratively revoked. Rejecting trades for such
    // clients is correct behaviour per the credit risk policy (Section 4.2).
    m_oStream << "client=" << strClientId
              << " amount=" << nAmountUsd << " USD"
              << " credit_limit=" << nCreditLimitUsd << " USD"
              << " (source: credit API)"
              << (bPassed ? " PASS" : " REJECT_CREDIT_EXCEEDED")
              << std::endl;
}
