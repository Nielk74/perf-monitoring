#pragma once
#include <string>
#include <unordered_map>

// @brief In-memory cache of FX rates from the live market data feed.
// @details GetRate("EUR","USD") returns 1.10 when EUR/USD is 1.10.
// Context: this service sits adjacent to the FX rate and currency conversion pipeline.
class CTradeFXRateCache
{
public:
    static CTradeFXRateCache* GetInstance();
    static void DestroyInstance();
    double GetRate(const std::string& strFrom, const std::string& strTo) const;
    void UpdateRate(const std::string& strFrom, const std::string& strTo, double dRate);
private:
    CTradeFXRateCache();
    ~CTradeFXRateCache();
    static CTradeFXRateCache* s_pInstance;
    std::unordered_map<std::string, double> m_mapRates;
};
