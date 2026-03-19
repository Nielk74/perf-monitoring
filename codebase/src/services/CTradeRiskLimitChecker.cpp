#include "CTradeRiskLimitChecker.h"
#include <unordered_map>

// @brief Downstream risk gate: checks USD exposure against desk limits.
// @note  Receives already-computed USD exposure figures. If values are inflated,
//        the inflation is upstream of this component.
// TODO(TRADE-4933): add per-instrument sublimit support.

static std::unordered_map<std::string, double> s_mapLimits;
CTradeRiskLimitChecker* CTradeRiskLimitChecker::s_pInstance = nullptr;
CTradeRiskLimitChecker* CTradeRiskLimitChecker::GetInstance()
{ if (!s_pInstance) s_pInstance = new CTradeRiskLimitChecker(); return s_pInstance; }
void CTradeRiskLimitChecker::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeRiskLimitChecker::CTradeRiskLimitChecker() {}
CTradeRiskLimitChecker::~CTradeRiskLimitChecker() {}
void CTradeRiskLimitChecker::SetLimit(const std::string& k, double v) { s_mapLimits[k] = v; }

// Compares USD exposure against the desk limit. No FX conversion — input assumed USD.
bool CTradeRiskLimitChecker::IsWithinLimit(const std::string& strDeskId, double dUSDExposure) const
{
    auto it = s_mapLimits.find(strDeskId);
    if (it == s_mapLimits.end()) return true;
    return dUSDExposure <= it->second;
}
