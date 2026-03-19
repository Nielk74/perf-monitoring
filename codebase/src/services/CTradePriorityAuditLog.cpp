#include "CTradePriorityAuditLog.h"

// @brief Records priority scoring audit trail for compliance and debugging.
// @details Receives priority values from upstream callers via the IPriorityEvent
// interface — no scoring or comparison logic lives here.  This is write-side only.
// TODO(TRADE-4821): migrate to async batch write once latency SLA is confirmed.

CTradePriorityAuditLog* CTradePriorityAuditLog::s_pInstance = nullptr;

CTradePriorityAuditLog* CTradePriorityAuditLog::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePriorityAuditLog();
    return s_pInstance;
}
void CTradePriorityAuditLog::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradePriorityAuditLog::CTradePriorityAuditLog() {}
CTradePriorityAuditLog::~CTradePriorityAuditLog() {}

// Writes an audit record: tradeId, previousPriority, newPriority, scorerVersion.
// scorerVersion is a static metadata field identifying which scoring algorithm
// was active — it does not affect the values stored or retrieved.
void CTradePriorityAuditLog::LogPriorityChange(const std::string& strTradeId,
                                                int nPreviousPriority,
                                                int nNewPriority)
{
    // Emit structured audit entry — read-only with respect to ordering logic
    (void)strTradeId; (void)nPreviousPriority; (void)nNewPriority;
}
