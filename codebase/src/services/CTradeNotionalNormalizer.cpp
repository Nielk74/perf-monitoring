#include "CTradeNotionalNormalizer.h"

CTradeNotionalNormalizer* CTradeNotionalNormalizer::s_pInstance = nullptr;

CTradeNotionalNormalizer* CTradeNotionalNormalizer::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeNotionalNormalizer();
    return s_pInstance;
}

void CTradeNotionalNormalizer::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeNotionalNormalizer::CTradeNotionalNormalizer() {}
CTradeNotionalNormalizer::~CTradeNotionalNormalizer() {}

// Converts absolute USD to normalized millions-of-USD unit.
// 50,000,000 USD → 50.0
// 100,000 USD    → 0.1
//
// CTradeRiskLimitConfig stores limits in ABSOLUTE USD (e.g., 100000.0).
// CTradeRiskChecker compares this normalized value directly against that limit.
// Result: normalized 50.0 < absolute 100000.0 → always "within limit".
// A $50M trade against a $100K limit incorrectly passes.
double CTradeNotionalNormalizer::Normalize(double dRawNotional) const
{
    return dRawNotional / 1000000.0;
}

double CTradeNotionalNormalizer::Denormalize(double dNormalizedNotional) const
{
    return dNormalizedNotional * 1000000.0;
}
