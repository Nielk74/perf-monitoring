#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

// Stores and retrieves rate limits for counterparties.
// Rate limits are expressed as maximum number of trades per HOUR.
// A value of 60 means the counterparty may submit at most 60 trades per hour.
class CTradeRateLimitConfig
{
public:
    static CTradeRateLimitConfig* GetInstance();
    static void DestroyInstance();

    bool Initialize();

    // Set the maximum number of trades per HOUR for a counterparty.
    // nMaxTradesPerHour: e.g. 60 means at most 60 trades per hour.
    void SetRateLimit(const std::string& strCptyCode, int nMaxTradesPerHour);

    // Returns the configured max trades PER HOUR for a counterparty.
    // Returns -1 if no limit is configured (unlimited).
    int GetRateLimit(const std::string& strCptyCode) const;

    // Returns true if a rate limit is configured for this counterparty.
    bool HasRateLimit(const std::string& strCptyCode) const;

private:
    CTradeRateLimitConfig();
    ~CTradeRateLimitConfig();
    CTradeRateLimitConfig(const CTradeRateLimitConfig&);
    CTradeRateLimitConfig& operator=(const CTradeRateLimitConfig&);

    // Maps counterparty code → max trades per HOUR
    std::map<std::string, int> m_mapRateLimits;

    bool m_bInitialized;
    static CTradeRateLimitConfig* s_pInstance;
};
