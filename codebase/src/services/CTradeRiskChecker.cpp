#include "CTradeRiskChecker.h"

CTradeRiskChecker* CTradeRiskChecker::s_pInstance = nullptr;

CTradeRiskChecker* CTradeRiskChecker::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeRiskChecker();
    return s_pInstance;
}

void CTradeRiskChecker::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeRiskChecker::CTradeRiskChecker()
    : m_pNormalizer(nullptr)
    , m_pLimitConfig(nullptr)
    , m_bInitialized(false)
{}

CTradeRiskChecker::~CTradeRiskChecker() {}

bool CTradeRiskChecker::Initialize(CTradeNotionalNormalizer* pNormalizer,
                                    CTradeRiskLimitConfig* pLimitConfig)
{
    if (!pNormalizer || !pLimitConfig) return false;
    m_pNormalizer  = pNormalizer;
    m_pLimitConfig = pLimitConfig;
    m_bInitialized = true;
    return true;
}

// Returns true if the trade notional is within the configured risk limit.
//
// Step 1: normalize the raw notional via CTradeNotionalNormalizer::Normalize()
//         Result is in MILLIONS of USD (e.g., 50,000,000 → 50.0)
//
// Step 2: retrieve the limit via CTradeRiskLimitConfig::GetLimit()
//         Result is in ABSOLUTE USD (e.g., 100,000)
//
// Step 3: compare dNormalized > dLimit
//         50.0 > 100000.0 → false → "within limit" → trade passes
//
// BUG: units are incompatible. A $50M trade against a $100K limit always passes
// because 50.0 (millions) < 100000.0 (absolute). Every trade is approved.
//
// Fix: either compare dRawNotional directly against dLimit (skip normalization
// for the comparison), or multiply dNormalized by 1,000,000 before comparing.
bool CTradeRiskChecker::IsWithinRiskLimit(const std::string& strTradeType,
                                           double dRawNotional) const
{
    if (!m_bInitialized) return true;

    // Normalize: converts $50,000,000 to 50.0 (millions)
    double dNormalized = m_pNormalizer->Normalize(dRawNotional);

    // Limit: returns absolute USD value, e.g., 100000.0 for $100K limit
    double dLimit = m_pLimitConfig->GetLimit(strTradeType);
    if (dLimit <= 0.0) return true;  // no limit configured

    // BUG: comparing millions (50.0) against absolute USD (100000.0)
    // 50.0 > 100000.0 is false → returns true (within limit) for any trade
    return dNormalized <= dLimit;
}
