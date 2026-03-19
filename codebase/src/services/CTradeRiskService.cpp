#include "CTradeRiskService.h"
#include <sstream>

#define LOG_INFO(x)

CTradeRiskService* CTradeRiskService::s_pInstance = nullptr;

CTradeRiskService* CTradeRiskService::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeRiskService();
    }
    return s_pInstance;
}

void CTradeRiskService::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeRiskService::CTradeRiskService()
    : m_bInitialized(false)
{}

CTradeRiskService::~CTradeRiskService() {}

bool CTradeRiskService::Initialize()
{
    m_bInitialized = true;
    return true;
}

void CTradeRiskService::SetRiskLimits(const std::string& strCptyCode,
                                       const SRiskLimitConfig& limits)
{
    m_mapRiskLimits[strCptyCode] = limits;
}

void CTradeRiskService::RecordTradeExposure(const std::string& strCptyCode,
                                             double dAmount)
{
    m_mapCumulativeExposure[strCptyCode] += dAmount;
}

void CTradeRiskService::ReleaseTradeExposure(const std::string& strCptyCode,
                                              double dAmount)
{
    auto it = m_mapCumulativeExposure.find(strCptyCode);
    if (it != m_mapCumulativeExposure.end()) {
        it->second -= dAmount;
        if (it->second < 0.0) it->second = 0.0;
    }
}

// Returns true if the proposed trade is within ALL risk limits:
//   1. The single-trade notional must not exceed m_dSingleTradeLimit
//   2. The new cumulative exposure must not exceed m_dCumulativeLimit
bool CTradeRiskService::IsTradeWithinRiskLimits(const std::string& strCptyCode,
                                                 double dAmount) const
{
    auto limIt = m_mapRiskLimits.find(strCptyCode);
    if (limIt == m_mapRiskLimits.end()) {
        // No limits configured — allow by default
        return true;
    }

    const SRiskLimitConfig& limits = limIt->second;

    // Check 1: single-trade limit
    if (dAmount > limits.m_dSingleTradeLimit) {
        return false;
    }

    // BUG: the cumulative exposure check below uses m_dSingleTradeLimit instead of
    // m_dCumulativeLimit. A counterparty can therefore accumulate unlimited total
    // exposure as long as no single trade exceeds the per-trade cap.
    //
    // Correct check should be:
    //   double dCurrentExposure = 0.0;
    //   auto expIt = m_mapCumulativeExposure.find(strCptyCode);
    //   if (expIt != m_mapCumulativeExposure.end()) dCurrentExposure = expIt->second;
    //   if (dCurrentExposure + dAmount > limits.m_dCumulativeLimit) return false;

    double dCurrentExposure = 0.0;
    auto expIt = m_mapCumulativeExposure.find(strCptyCode);
    if (expIt != m_mapCumulativeExposure.end()) {
        dCurrentExposure = expIt->second;
    }

    // BUG: compares against m_dSingleTradeLimit, not m_dCumulativeLimit
    if (dCurrentExposure + dAmount > limits.m_dSingleTradeLimit) {
        return false;
    }

    return true;
}

double CTradeRiskService::GetCumulativeExposure(const std::string& strCptyCode) const
{
    auto it = m_mapCumulativeExposure.find(strCptyCode);
    if (it != m_mapCumulativeExposure.end()) return it->second;
    return 0.0;
}
