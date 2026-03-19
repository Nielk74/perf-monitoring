#include "CTradeRateLimiter.h"
#include <ctime>
#include <algorithm>

CTradeRateLimiter* CTradeRateLimiter::s_pInstance = nullptr;

CTradeRateLimiter* CTradeRateLimiter::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeRateLimiter();
    }
    return s_pInstance;
}

void CTradeRateLimiter::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeRateLimiter::CTradeRateLimiter()
    : m_pConfig(nullptr)
    , m_bInitialized(false)
{}

CTradeRateLimiter::~CTradeRateLimiter() {}

bool CTradeRateLimiter::Initialize(CTradeRateLimitConfig* pConfig)
{
    if (!pConfig) return false;
    m_pConfig = pConfig;
    m_bInitialized = true;
    return true;
}

void CTradeRateLimiter::RecordSubmission(const std::string& strCptyCode)
{
    if (!m_bInitialized) return;
    long nNow = static_cast<long>(time(nullptr));
    m_mapSubmissionTimes[strCptyCode].push_back(nNow);
}

// Checks whether a counterparty has exceeded their configured rate limit.
//
// BUG: the rolling window used to count submissions is 60 seconds (1 minute),
// but CTradeRateLimitConfig stores limits as max trades PER HOUR.
//
// CTradeRateLimitConfig::SetRateLimit documentation:
//   "nMaxTradesPerHour: e.g. 60 means at most 60 trades per hour"
//
// This function counts submissions in the last 60 SECONDS and compares against
// the per-HOUR limit. So a limit of 60 (trades/hour) is checked against the
// number of trades in the last minute.
//
// For a typical limit of 60 trades/hour (~1/minute), a counterparty would need
// to submit 60+ trades in a single minute to be blocked — which is 60× too
// permissive. Normal usage (1-2 trades/minute) never triggers the rate limiter.
//
// The window should be 3600 seconds (1 hour) to match the configured unit.
bool CTradeRateLimiter::IsRateLimited(const std::string& strCptyCode) const
{
    if (!m_bInitialized || !m_pConfig) return false;

    int nLimit = m_pConfig->GetRateLimit(strCptyCode);
    if (nLimit < 0) return false;  // no limit configured

    long nNow = static_cast<long>(time(nullptr));

    // BUG: window is 60 seconds (1 minute) but limit is per HOUR (3600 seconds).
    // Should be: const long WINDOW_SECONDS = 3600;
    const long WINDOW_SECONDS = 60;

    auto it = m_mapSubmissionTimes.find(strCptyCode);
    if (it == m_mapSubmissionTimes.end()) return false;

    const std::vector<long>& times = it->second;
    int nCount = 0;
    for (size_t i = 0; i < times.size(); ++i) {
        if (nNow - times[i] <= WINDOW_SECONDS) {
            ++nCount;
        }
    }

    return nCount >= nLimit;
}

int CTradeRateLimiter::GetSubmissionCount(const std::string& strCptyCode) const
{
    auto it = m_mapSubmissionTimes.find(strCptyCode);
    if (it == m_mapSubmissionTimes.end()) return 0;
    long nNow = static_cast<long>(time(nullptr));
    const long WINDOW_SECONDS = 60;  // BUG: same incorrect window
    int nCount = 0;
    for (size_t i = 0; i < it->second.size(); ++i) {
        if (nNow - it->second[i] <= WINDOW_SECONDS) ++nCount;
    }
    return nCount;
}

void CTradeRateLimiter::ResetCounters()
{
    m_mapSubmissionTimes.clear();
}
