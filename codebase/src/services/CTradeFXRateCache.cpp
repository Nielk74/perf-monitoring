#include "CTradeFXRateCache.h"

// @brief Live FX rate cache. Rates stored as direct multipliers.
// @note  The cache stores and returns correct rates. If exposure figures appear
//        inflated by roughly the FX rate squared, the issue is not the stored
//        rate values but how many times callers apply them.
// TODO(TRADE-4712): add rate-staleness detection and forced refresh.

CTradeFXRateCache* CTradeFXRateCache::s_pInstance = nullptr;
CTradeFXRateCache* CTradeFXRateCache::GetInstance()
{ if (!s_pInstance) s_pInstance = new CTradeFXRateCache(); return s_pInstance; }
void CTradeFXRateCache::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeFXRateCache::CTradeFXRateCache() {}
CTradeFXRateCache::~CTradeFXRateCache() {}

double CTradeFXRateCache::GetRate(const std::string& strFrom, const std::string& strTo) const
{
    if (strFrom == strTo) return 1.0;
    auto it = m_mapRates.find(strFrom + "/" + strTo);
    return (it != m_mapRates.end()) ? it->second : 1.0;
}
void CTradeFXRateCache::UpdateRate(const std::string& f, const std::string& t, double d)
{ m_mapRates[f + "/" + t] = d; }
