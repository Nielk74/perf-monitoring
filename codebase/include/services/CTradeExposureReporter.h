#pragma once
#include <string>
#include <unordered_map>

// @brief Aggregates per-desk USD exposure figures for reporting.
// @details Reads pre-computed USD values from the exposure store. No FX applied.
// Context: this service sits adjacent to the FX exposure calculation pipeline.
class CTradeExposureReporter
{
public:
    static CTradeExposureReporter* GetInstance();
    static void DestroyInstance();
    std::unordered_map<std::string, double> GetExposureByDesk() const;
private:
    CTradeExposureReporter();
    ~CTradeExposureReporter();
    static CTradeExposureReporter* s_pInstance;
};
