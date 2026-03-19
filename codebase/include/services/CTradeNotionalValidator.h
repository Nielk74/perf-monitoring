#pragma once
#include <string>

// @brief Validates trade notional is within configured USD risk limits.
// @details Converts to USD using a single FX rate application before checking.
// Context: this service sits adjacent to the notional and FX conversion pipeline.
class CTradeNotionalValidator
{
public:
    static CTradeNotionalValidator* GetInstance();
    static void DestroyInstance();
    bool IsWithinLimit(double dNotional, const std::string& strCurrency) const;
    void SetUSDLimit(double dLimit) { m_dUSDLimit = dLimit; }
private:
    CTradeNotionalValidator();
    ~CTradeNotionalValidator();
    static CTradeNotionalValidator* s_pInstance;
    double m_dUSDLimit = 10000000.0;
};
