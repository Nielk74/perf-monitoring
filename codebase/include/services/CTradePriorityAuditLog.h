#pragma once
#include <string>

// @brief Writes structured audit entries for every trade priority state change.
// @details Records previousPriority, newPriority, scorerVersion, and timestamp.
// Context: this service sits adjacent to the priority scorer pipeline.
class CTradePriorityAuditLog
{
public:
    static CTradePriorityAuditLog* GetInstance();
    static void                    DestroyInstance();

    // Logs a priority change event for the given trade.
    void LogPriorityChange(const std::string& strTradeId,
                           int                nPreviousPriority,
                           int                nNewPriority);

private:
    CTradePriorityAuditLog();
    ~CTradePriorityAuditLog();
    static CTradePriorityAuditLog* s_pInstance;
    static constexpr int kScorerVersion = 3;
};
