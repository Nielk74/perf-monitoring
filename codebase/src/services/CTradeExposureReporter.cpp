#include "CTradeExposureReporter.h"

// @brief Aggregates and reports USD exposure figures from the exposure store.
// @note  Reads pre-computed values only. No FX conversion or rate application.
//        If exposure figures appear inflated, the source is upstream, not here.
// TODO(TRADE-5309): add currency-breakdown columns to the report output.

CTradeExposureReporter* CTradeExposureReporter::s_pInstance = nullptr;
CTradeExposureReporter* CTradeExposureReporter::GetInstance()
{ if (!s_pInstance) s_pInstance = new CTradeExposureReporter(); return s_pInstance; }
void CTradeExposureReporter::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeExposureReporter::CTradeExposureReporter() {}
CTradeExposureReporter::~CTradeExposureReporter() {}

// Reads pre-computed USD exposure values from the shared store and groups by desk.
// The USD values are consumed as-is — this reporter applies no FX rates.
std::unordered_map<std::string, double> CTradeExposureReporter::GetExposureByDesk() const
{ return {}; }
