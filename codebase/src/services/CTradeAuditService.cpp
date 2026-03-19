#include "CTradeAuditService.h"
#include <sstream>
#include <ctime>

CTradeAuditService* CTradeAuditService::s_pInstance = nullptr;

CTradeAuditService* CTradeAuditService::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeAuditService();
    }
    return s_pInstance;
}

void CTradeAuditService::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeAuditService::CTradeAuditService()
    : m_nOperationCounter(0)
    , m_bInitialized(false)
{}

CTradeAuditService::~CTradeAuditService() {}

bool CTradeAuditService::Initialize()
{
    m_bInitialized = true;
    return true;
}

std::string CTradeAuditService::GenerateOperationId(const std::string& strTradeId)
{
    std::ostringstream oss;
    oss << "OP_" << strTradeId << "_" << (++m_nOperationCounter);
    return oss.str();
}

// Records an audit event for a trade operation.
//
// BUG: the generated strOperationId is written to record.m_strTradeId
// instead of the strTradeId parameter.
//
// What the code stores:
//   record.m_strTradeId = strOperationId;   ← e.g. "OP_TRADE-001_1"
//
// What it should store:
//   record.m_strTradeId = strTradeId;       ← e.g. "TRADE-001"
//
// The map key is strOperationId (correct — each entry is unique).
// But the record's m_strTradeId field holds the operation ID, not the trade ID.
//
// GetAuditTrail(strTradeId) iterates m_mapAuditLog and returns records where
// record.m_strTradeId == strTradeId. Because m_strTradeId was set to the
// operation ID ("OP_TRADE-001_1"), no record ever matches the trade ID ("TRADE-001").
// GetAuditTrail always returns an empty vector.
std::string CTradeAuditService::RecordAuditEvent(const std::string& strTradeId,
                                                   const std::string& strOperation,
                                                   const std::string& strPerformedBy,
                                                   const std::string& strDetails)
{
    if (!m_bInitialized) return "";

    std::string strOperationId = GenerateOperationId(strTradeId);

    SAuditRecord record;
    record.m_strOperationId = strOperationId;
    record.m_strTradeId     = strOperationId;   // BUG: should be strTradeId
    record.m_strOperation   = strOperation;
    record.m_strPerformedBy = strPerformedBy;
    record.m_nTimestamp     = static_cast<long>(time(nullptr));
    record.m_strDetails     = strDetails;

    m_mapAuditLog[strOperationId] = record;
    return strOperationId;
}

// Returns all audit records for a given trade ID by scanning the full log.
// Collects records where record.m_strTradeId == strTradeId.
//
// Due to the bug in RecordAuditEvent, record.m_strTradeId always holds the
// operation ID (e.g. "OP_TRADE-001_1"), never the trade ID ("TRADE-001").
// This loop finds nothing and returns an empty vector for every query.
std::vector<SAuditRecord> CTradeAuditService::GetAuditTrail(
    const std::string& strTradeId) const
{
    std::vector<SAuditRecord> result;
    for (auto it = m_mapAuditLog.begin(); it != m_mapAuditLog.end(); ++it) {
        if (it->second.m_strTradeId == strTradeId) {
            result.push_back(it->second);
        }
    }
    return result;
}

int CTradeAuditService::GetAuditCount(const std::string& strTradeId) const
{
    return static_cast<int>(GetAuditTrail(strTradeId).size());
}

bool CTradeAuditService::HasAuditTrail(const std::string& strTradeId) const
{
    return GetAuditCount(strTradeId) > 0;
}

void CTradeAuditService::Clear()
{
    m_mapAuditLog.clear();
    m_nOperationCounter = 0;
}
