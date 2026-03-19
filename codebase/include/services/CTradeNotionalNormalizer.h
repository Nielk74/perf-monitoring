#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"

// Normalizes raw trade notional amounts before they are stored or checked.
// Converts absolute USD amounts to a normalized unit for internal use.
//
// Normalization scale: divides by 1,000,000 — stores in MILLIONS of USD.
// Example: 50,000,000 USD raw → 50.0 normalized
//          100,000 USD raw     → 0.1 normalized
//
// This service has no direct dependency on CTradeRiskLimitConfig or CTradeRiskChecker.
class CTradeNotionalNormalizer
{
public:
    static CTradeNotionalNormalizer* GetInstance();
    static void DestroyInstance();

    // Normalize a raw notional amount to the internal unit (millions USD).
    // dRawNotional: absolute USD amount (e.g., 50000000.0 for $50M)
    // Returns: normalized value (e.g., 50.0 for $50M)
    double Normalize(double dRawNotional) const;

    // Convert a normalized value back to absolute USD.
    double Denormalize(double dNormalizedNotional) const;

private:
    CTradeNotionalNormalizer();
    ~CTradeNotionalNormalizer();
    CTradeNotionalNormalizer(const CTradeNotionalNormalizer&);
    CTradeNotionalNormalizer& operator=(const CTradeNotionalNormalizer&);

    static CTradeNotionalNormalizer* s_pInstance;
};
