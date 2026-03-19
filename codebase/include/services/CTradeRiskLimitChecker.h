#pragma once
#include <string>

// @brief Checks computed USD exposure against per-desk risk limits.
// @details Receives a pre-computed USD exposure value and compares to threshold.
// Context: this service sits adjacent to the risk exposure pipeline.
class CTradeRiskLimitChecker
{
public:
    static CTradeRiskLimitChecker* GetInstance();
    static void DestroyInstance();
    bool IsWithinLimit(const std::string& strDeskId, double dUSDExposure) const;
    void SetLimit(const std::string& strDeskId, double dLimit);
private:
    CTradeRiskLimitChecker();
    ~CTradeRiskLimitChecker();
    static CTradeRiskLimitChecker* s_pInstance;
};
