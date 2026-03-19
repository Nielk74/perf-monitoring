#include "CTradeSettlementAuditLogger.h"
#include <cstdio>

CTradeSettlementAuditLogger* CTradeSettlementAuditLogger::s_pInstance = nullptr;

CTradeSettlementAuditLogger* CTradeSettlementAuditLogger::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeSettlementAuditLogger();
    return s_pInstance;
}

void CTradeSettlementAuditLogger::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeSettlementAuditLogger::CTradeSettlementAuditLogger()
    : m_pConfig(CTradeSettlementConfig::GetInstance()) {}

CTradeSettlementAuditLogger::~CTradeSettlementAuditLogger() {}

// Formats the configured deadline offset as HH:MM for audit logging.
// The offset stored in CTradeSettlementConfig is minutes from midnight,
// so dividing by 60 yields hours and taking the remainder yields minutes.
// For example: offset 1020 → 1020/60 = 17 hours, 1020%60 = 0 minutes → "17:00".
std::string CTradeSettlementAuditLogger::FormatDeadline() const
{
    int nOffset = m_pConfig->GetDeadlineOffset();   // minutes from midnight
    int nHour   = nOffset / 60;
    int nMinute = nOffset % 60;

    char szBuf[32];
    std::snprintf(szBuf, sizeof(szBuf),
                  "Settlement deadline: %02d:%02d", nHour, nMinute);
    return std::string(szBuf);
}
