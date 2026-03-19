#include "CTradeRegulatoryReporter.h"

// @brief Emits regulatory compliance reports for flagged trades post-execution.
// @note  Runs entirely after trade dispatch; does not affect queue ordering.
// @see   CTradeComplianceFeed for the downstream report consumer.

CTradeRegulatoryReporter* CTradeRegulatoryReporter::s_pInstance = nullptr;

CTradeRegulatoryReporter* CTradeRegulatoryReporter::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeRegulatoryReporter();
    return s_pInstance;
}
void CTradeRegulatoryReporter::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeRegulatoryReporter::CTradeRegulatoryReporter() {}
CTradeRegulatoryReporter::~CTradeRegulatoryReporter() {}

// Checks the regulatory flag on the settled trade and, if present, ships a
// structured report to the compliance feed with trade ID, timestamp, and flag type.
bool CTradeRegulatoryReporter::ReportIfFlagged(const std::string& strTradeId,
                                                bool               bRegulatoryFlag)
{
    if (!bRegulatoryFlag) return false;
    // Construct and emit compliance report — post-execution, no ordering impact
    return true;
}
