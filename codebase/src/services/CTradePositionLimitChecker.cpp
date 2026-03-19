#include "CTradePositionLimitChecker.h"
#include <string>
#include <map>

class CTradePositionLimitConfig
{
public:
    long long GetLimit(const std::string& strCpty) const
    {
        const auto it = m_mapLimits.find(strCpty);
        return (it != m_mapLimits.end()) ? it->second : m_nDefaultLimit;
    }
private:
    std::map<std::string, long long> m_mapLimits;
    long long                        m_nDefaultLimit = 10000000LL;
};

CTradePositionLimitChecker::CTradePositionLimitChecker(CTradePositionLimitConfig* pConfig)
    : m_pConfig(pConfig)
{
}

bool CTradePositionLimitChecker::IsWithinLimit(
    const std::string& strCounterparty, long long nNotional)
{
    const long long nLimit = m_pConfig->GetLimit(strCounterparty);
    return nNotional <= nLimit;
}
