#pragma once
#include "CTradeNotionalStore.h"
#include <string>
#include <unordered_map>

// Converts the raw notional of a trade to its USD equivalent and persists
// the result back to the store.  After conversion the notional field holds
// the USD-denominated value and the m_bNormalized flag is set so that
// downstream services know FX has already been applied.
class CTradeNotionalUSDConverter
{
public:
    static CTradeNotionalUSDConverter* GetInstance();
    static void                        DestroyInstance();

    // Reads the trade record, applies the USD FX rate to m_dNotional, sets
    // m_bNormalized = true, and writes the record back.
    // USD-denominated records are passed through unchanged.
    void ConvertToUSD(const std::string& strTradeId);

    void SetFXRate(const std::string& strCurrency, double dRate)
    {
        m_mapFXRates[strCurrency] = dRate;
    }

private:
    CTradeNotionalUSDConverter();
    ~CTradeNotionalUSDConverter();

    static CTradeNotionalUSDConverter* s_pInstance;

    CTradeNotionalStore*                    m_pStore;
    std::unordered_map<std::string, double> m_mapFXRates;
};
