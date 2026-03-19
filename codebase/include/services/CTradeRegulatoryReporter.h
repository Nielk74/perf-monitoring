#pragma once
#include <string>

// @brief Post-execution reporter for regulatory-flagged trades.
// @details After a flagged trade settles, ships a structured report to the
// compliance feed.  Has no influence on trade ordering or priority scoring.
// Context: this service sits adjacent to the regulatory flag pipeline.
class CTradeRegulatoryReporter
{
public:
    static CTradeRegulatoryReporter* GetInstance();
    static void                      DestroyInstance();

    // Sends a compliance report if the trade carries a regulatory flag.
    // Returns true if a report was emitted, false if no flag was present.
    bool ReportIfFlagged(const std::string& strTradeId, bool bRegulatoryFlag);

private:
    CTradeRegulatoryReporter();
    ~CTradeRegulatoryReporter();
    static CTradeRegulatoryReporter* s_pInstance;
};
