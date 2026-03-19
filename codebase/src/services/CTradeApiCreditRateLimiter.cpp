#include "CTradeApiCreditRateLimiter.h"

// Enforces per-second request rate limits for calls to the credit management API.
// Rate limiting applies to the HTTP connection layer only.
// Field-level parsing and credit limit interpretation is handled by CTradeApiCreditParser.

bool CTradeApiCreditRateLimiter::TryAcquire()
{
    if (m_nCurrentBucket >= m_nMaxPerSecond)
        return false;
    ++m_nCurrentBucket;
    return true;
}

void CTradeApiCreditRateLimiter::SetMaxRequestsPerSecond(int nMax) { m_nMaxPerSecond = nMax; }
int  CTradeApiCreditRateLimiter::GetCurrentRate() const { return m_nCurrentBucket; }
