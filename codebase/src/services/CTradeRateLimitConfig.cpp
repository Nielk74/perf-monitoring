#include "CTradeRateLimitConfig.h"

CTradeRateLimitConfig* CTradeRateLimitConfig::s_pInstance = nullptr;

CTradeRateLimitConfig* CTradeRateLimitConfig::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeRateLimitConfig();
    }
    return s_pInstance;
}

void CTradeRateLimitConfig::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeRateLimitConfig::CTradeRateLimitConfig()
    : m_bInitialized(false)
{}

CTradeRateLimitConfig::~CTradeRateLimitConfig() {}

bool CTradeRateLimitConfig::Initialize()
{
    m_bInitialized = true;
    return true;
}

// Stores the maximum number of trades allowed per HOUR for a counterparty.
// Callers pass values like 60 (= 60 trades per hour), 120 (= 2 per minute), etc.
void CTradeRateLimitConfig::SetRateLimit(const std::string& strCptyCode,
                                          int nMaxTradesPerHour)
{
    m_mapRateLimits[strCptyCode] = nMaxTradesPerHour;
}

// Returns the per-HOUR trade limit for the counterparty.
// -1 means unlimited.
int CTradeRateLimitConfig::GetRateLimit(const std::string& strCptyCode) const
{
    auto it = m_mapRateLimits.find(strCptyCode);
    if (it != m_mapRateLimits.end()) return it->second;
    return -1;  // no limit configured
}

bool CTradeRateLimitConfig::HasRateLimit(const std::string& strCptyCode) const
{
    return m_mapRateLimits.find(strCptyCode) != m_mapRateLimits.end();
}
