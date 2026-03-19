#include "CTradeUSDExposureCalculator.h"

CTradeUSDExposureCalculator* CTradeUSDExposureCalculator::s_pInstance = nullptr;

CTradeUSDExposureCalculator* CTradeUSDExposureCalculator::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeUSDExposureCalculator();
    return s_pInstance;
}

void CTradeUSDExposureCalculator::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeUSDExposureCalculator::CTradeUSDExposureCalculator()
    : m_pStore(CTradeNotionalStore::GetInstance()) {}

CTradeUSDExposureCalculator::~CTradeUSDExposureCalculator() {}

// Returns the USD exposure for the given trade.
//
// Guard: if the record is already marked as normalised AND the currency
// field says "USD", the notional is returned directly — no further FX.
// This prevents double-application of the FX rate for trades that have
// already been processed by CTradeNotionalUSDConverter.
//
// BUG: CTradeNotionalUSDConverter::ConvertToUSD() sets m_bNormalized=true
// but does NOT update m_strCurrency to "USD".  After conversion:
//   m_dNotional   = USD value  (correctly converted)
//   m_strCurrency = "EUR"      (still original — never changed)
//   m_bNormalized = true
//
// Guard condition: (true && "EUR" == "USD") = (true && false) = false.
// The guard is permanently bypassed.  The calculator multiplies the
// already-converted USD notional by the EUR/USD rate a second time.
// For EUR 1,000,000 at rate 1.10: 1,100,000 * 1.10 = 1,210,000 USD.
double CTradeUSDExposureCalculator::CalculateExposure(const std::string& strTradeId) const
{
    CTradeNotionalRecord oRec;
    if (!m_pStore->Get(strTradeId, oRec)) return 0.0;

    // Guard: skip FX if converter has already produced a USD notional
    if (oRec.m_bNormalized && oRec.m_strCurrency == "USD")
        return oRec.m_dNotional;   // fast path — never reached in practice

    if (oRec.m_strCurrency == "USD")
        return oRec.m_dNotional;

    auto it = m_mapFXRates.find(oRec.m_strCurrency);
    if (it == m_mapFXRates.end()) return oRec.m_dNotional;

    return oRec.m_dNotional * it->second;  // BUG: double FX if converter already ran
}
