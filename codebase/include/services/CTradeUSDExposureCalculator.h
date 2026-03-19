#pragma once
#include "CTradeNotionalStore.h"
#include <string>
#include <unordered_map>

// Computes the USD exposure for a trade.
// Contains a guard that skips FX application for trades that have already
// been converted to USD by CTradeNotionalUSDConverter.
class CTradeUSDExposureCalculator
{
public:
    static CTradeUSDExposureCalculator* GetInstance();
    static void                         DestroyInstance();

    // Returns the USD exposure for the given trade.
    // If the record is already normalised AND currency is already "USD",
    // the stored notional is returned directly (no further FX applied).
    // Otherwise the FX rate is applied to convert to USD.
    double CalculateExposure(const std::string& strTradeId) const;

    void SetFXRate(const std::string& strCurrency, double dRate)
    {
        m_mapFXRates[strCurrency] = dRate;
    }

private:
    CTradeUSDExposureCalculator();
    ~CTradeUSDExposureCalculator();

    static CTradeUSDExposureCalculator* s_pInstance;

    CTradeNotionalStore*                    m_pStore;
    std::unordered_map<std::string, double> m_mapFXRates;
};
