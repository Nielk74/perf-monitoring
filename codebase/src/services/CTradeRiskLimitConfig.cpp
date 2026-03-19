#include "CTradeRiskLimitConfig.h"

CTradeRiskLimitConfig* CTradeRiskLimitConfig::s_pInstance = nullptr;

CTradeRiskLimitConfig* CTradeRiskLimitConfig::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeRiskLimitConfig();
    return s_pInstance;
}

void CTradeRiskLimitConfig::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeRiskLimitConfig::CTradeRiskLimitConfig() {}
CTradeRiskLimitConfig::~CTradeRiskLimitConfig() {}

// Stores limits in ABSOLUTE USD.
// SetLimit("VANILLA_EQUITY", 100000.0) = $100,000 maximum.
void CTradeRiskLimitConfig::SetLimit(const std::string& strTradeType,
                                      double dLimitAbsoluteUSD)
{
    m_mapLimits[strTradeType] = dLimitAbsoluteUSD;
}

double CTradeRiskLimitConfig::GetLimit(const std::string& strTradeType) const
{
    std::map<std::string, double>::const_iterator it = m_mapLimits.find(strTradeType);
    if (it == m_mapLimits.end()) return 0.0;
    return it->second;
}
