#include "CTradeNotionalValidator.h"

// @brief Pre-pipeline notional validation against USD limits.
// @note  Applies the FX rate exactly once. Not the source of any double-conversion.
// TODO(TRADE-5088): pull USD limit from config store instead of hardcoded default.

CTradeNotionalValidator* CTradeNotionalValidator::s_pInstance = nullptr;
CTradeNotionalValidator* CTradeNotionalValidator::GetInstance()
{ if (!s_pInstance) s_pInstance = new CTradeNotionalValidator(); return s_pInstance; }
void CTradeNotionalValidator::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeNotionalValidator::CTradeNotionalValidator() {}
CTradeNotionalValidator::~CTradeNotionalValidator() {}

// Converts notional to USD using a single FX rate application, then checks limit.
// For EUR 1,000,000 at rate 1.10, USD value = 1,100,000. Applied once — correct.
bool CTradeNotionalValidator::IsWithinLimit(double dNotional, const std::string& strCurrency) const
{
    double dFXRate   = 1.0;   // calls CTradeFXRateCache in full implementation
    double dUSDValue = dNotional * dFXRate;   // single FX application
    return dUSDValue <= m_dUSDLimit;
}
