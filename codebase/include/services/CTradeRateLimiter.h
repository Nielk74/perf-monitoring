#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include <vector>
#include "ServiceCommon.h"
#include "CTradeRateLimitConfig.h"

// Enforces per-counterparty trade rate limits at submission time.
// Call RecordSubmission when a trade is submitted.
// Call IsRateLimited before allowing a new submission.
class CTradeRateLimiter
{
public:
    static CTradeRateLimiter* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeRateLimitConfig* pConfig);

    // Record that a trade was just submitted by this counterparty.
    // Updates the rolling window counter.
    void RecordSubmission(const std::string& strCptyCode);

    // Returns true if the counterparty has exceeded their configured rate limit.
    // Compares the number of submissions in the last rolling window against
    // the configured limit from CTradeRateLimitConfig.
    bool IsRateLimited(const std::string& strCptyCode) const;

    // Returns the number of submissions recorded in the current window.
    int GetSubmissionCount(const std::string& strCptyCode) const;

    void ResetCounters();

private:
    CTradeRateLimiter();
    ~CTradeRateLimiter();
    CTradeRateLimiter(const CTradeRateLimiter&);
    CTradeRateLimiter& operator=(const CTradeRateLimiter&);

    // Maps counterparty code → list of submission timestamps (seconds since epoch)
    std::map<std::string, std::vector<long> > m_mapSubmissionTimes;

    CTradeRateLimitConfig* m_pConfig;
    bool m_bInitialized;

    static CTradeRateLimiter* s_pInstance;
};
