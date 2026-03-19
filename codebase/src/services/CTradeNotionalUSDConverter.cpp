#include "CTradeNotionalUSDConverter.h"

CTradeNotionalUSDConverter* CTradeNotionalUSDConverter::s_pInstance = nullptr;

CTradeNotionalUSDConverter* CTradeNotionalUSDConverter::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeNotionalUSDConverter();
    return s_pInstance;
}

void CTradeNotionalUSDConverter::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeNotionalUSDConverter::CTradeNotionalUSDConverter()
    : m_pStore(CTradeNotionalStore::GetInstance()) {}

CTradeNotionalUSDConverter::~CTradeNotionalUSDConverter() {}

// Converts the stored notional to USD and writes the result back.
//
// After this call:
//   oRec.m_dNotional   = original notional × FX rate  (now USD-denominated)
//   oRec.m_bNormalized = true                          (signals conversion done)
//
// BUG: oRec.m_strCurrency is NOT updated to "USD".
// It still holds the original currency code (e.g. "EUR").
// The comment below describes the design intent — but the consequence is
// that any downstream service checking m_strCurrency to guard against
// double-conversion will see the original currency and re-apply the FX rate.
void CTradeNotionalUSDConverter::ConvertToUSD(const std::string& strTradeId)
{
    CTradeNotionalRecord oRec;
    if (!m_pStore->Get(strTradeId, oRec)) return;
    if (oRec.m_bNormalized)              return;  // idempotent

    if (oRec.m_strCurrency != "USD")
    {
        auto it = m_mapFXRates.find(oRec.m_strCurrency);
        if (it != m_mapFXRates.end())
            oRec.m_dNotional *= it->second;   // convert to USD

        // m_strCurrency is preserved as the original trade currency for
        // audit and reporting purposes.  Downstream consumers must inspect
        // m_bNormalized rather than m_strCurrency to determine whether FX
        // has already been applied.
    }

    oRec.m_bNormalized = true;
    m_pStore->Set(strTradeId, oRec);
}
