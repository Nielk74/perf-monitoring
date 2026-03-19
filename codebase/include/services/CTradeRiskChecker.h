#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradeNotionalNormalizer.h"
#include "CTradeRiskLimitConfig.h"

// Checks whether a trade's notional exceeds the configured risk limit.
//
// Uses CTradeNotionalNormalizer to prepare the notional value, then
// compares against CTradeRiskLimitConfig.
//
// BUG: the normalizer returns values in MILLIONS of USD (e.g., 50.0 for $50M),
// but GetLimit() returns limits in ABSOLUTE USD (e.g., 100000.0 for $100K).
// Comparing 50.0 > 100000.0 → always false → every trade passes, no matter how large.
// A $50M trade against a $100K limit returns "within limit".
//
// The three services (Normalizer, LimitConfig, RiskChecker) have no direct
// call chain between Normalizer and LimitConfig — the mismatch is only visible
// by reading all three and comparing their unit assumptions.
class CTradeRiskChecker
{
public:
    static CTradeRiskChecker* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeNotionalNormalizer* pNormalizer,
                    CTradeRiskLimitConfig* pLimitConfig);

    // Returns true if the trade is within risk limits, false if it exceeds them.
    bool IsWithinRiskLimit(const std::string& strTradeType,
                           double dRawNotional) const;

private:
    CTradeRiskChecker();
    ~CTradeRiskChecker();
    CTradeRiskChecker(const CTradeRiskChecker&);
    CTradeRiskChecker& operator=(const CTradeRiskChecker&);

    CTradeNotionalNormalizer* m_pNormalizer;
    CTradeRiskLimitConfig*    m_pLimitConfig;
    bool m_bInitialized;
    static CTradeRiskChecker* s_pInstance;
};
