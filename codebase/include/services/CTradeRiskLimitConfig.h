#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

// Stores risk limits per trade type.
//
// Limit values are configured in ABSOLUTE USD (not normalized).
// Example: SetLimit("VANILLA_EQUITY", 100000.0) means maximum $100,000 USD.
//
// This service has no direct dependency on CTradeNotionalNormalizer or CTradeRiskChecker.
class CTradeRiskLimitConfig
{
public:
    static CTradeRiskLimitConfig* GetInstance();
    static void DestroyInstance();

    // Configure the maximum notional for a trade type.
    // dLimitAbsoluteUSD: maximum allowed notional in ABSOLUTE USD.
    //   e.g., 100000.0 means a $100,000 limit.
    void SetLimit(const std::string& strTradeType, double dLimitAbsoluteUSD);

    // Returns the configured limit in absolute USD.
    // Returns 0.0 if no limit is configured for this trade type.
    double GetLimit(const std::string& strTradeType) const;

private:
    CTradeRiskLimitConfig();
    ~CTradeRiskLimitConfig();
    CTradeRiskLimitConfig(const CTradeRiskLimitConfig&);
    CTradeRiskLimitConfig& operator=(const CTradeRiskLimitConfig&);

    std::map<std::string, double> m_mapLimits;
    static CTradeRiskLimitConfig* s_pInstance;
};
