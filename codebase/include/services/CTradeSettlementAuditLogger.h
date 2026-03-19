#pragma once
#include "CTradeSettlementConfig.h"
#include <string>

// Writes the active settlement configuration to the audit trail so that
// operations staff can verify deadline settings without accessing the
// config store directly.
class CTradeSettlementAuditLogger
{
public:
    static CTradeSettlementAuditLogger* GetInstance();
    static void                         DestroyInstance();

    // Logs the current deadline as a human-readable HH:MM string.
    // Example: offset 1020 is logged as "Settlement deadline: 17:00".
    std::string FormatDeadline() const;

private:
    CTradeSettlementAuditLogger();
    ~CTradeSettlementAuditLogger();

    static CTradeSettlementAuditLogger* s_pInstance;

    CTradeSettlementConfig* m_pConfig;
};
