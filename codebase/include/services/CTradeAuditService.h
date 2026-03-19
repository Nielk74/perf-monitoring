#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include <vector>
#include "ServiceCommon.h"

struct SAuditRecord {
    std::string m_strOperationId;   // unique ID for this audit entry, e.g. "OP_TRADE-001_1"
    std::string m_strTradeId;       // the trade this operation belongs to
    std::string m_strOperation;     // e.g. "CREATE", "SUBMIT", "APPROVE", "CANCEL"
    std::string m_strPerformedBy;
    long        m_nTimestamp;
    std::string m_strDetails;

    SAuditRecord() : m_nTimestamp(0) {}
};

// Records and retrieves an immutable audit trail for each trade.
// Every state-changing operation should call RecordAuditEvent.
// GetAuditTrail returns all operations for a given trade in insertion order.
class CTradeAuditService
{
public:
    static CTradeAuditService* GetInstance();
    static void DestroyInstance();

    bool Initialize();

    // Record an audit event for a trade operation.
    // strTradeId: the trade being operated on
    // strOperation: short operation name ("CREATE", "APPROVE", etc.)
    // strPerformedBy: user or system that performed the action
    // strDetails: optional free-text detail
    // Returns the generated operation ID for this entry.
    std::string RecordAuditEvent(const std::string& strTradeId,
                                 const std::string& strOperation,
                                 const std::string& strPerformedBy,
                                 const std::string& strDetails = "");

    // Returns all audit records for a given trade, in insertion order.
    // Returns an empty vector if no records exist for the trade.
    std::vector<SAuditRecord> GetAuditTrail(const std::string& strTradeId) const;

    // Returns the number of audit entries recorded for a trade.
    int GetAuditCount(const std::string& strTradeId) const;

    // Returns true if any audit record exists for the given trade.
    bool HasAuditTrail(const std::string& strTradeId) const;

    size_t GetTotalRecordCount() const { return m_mapAuditLog.size(); }

    void Clear();

private:
    CTradeAuditService();
    ~CTradeAuditService();
    CTradeAuditService(const CTradeAuditService&);
    CTradeAuditService& operator=(const CTradeAuditService&);

    std::string GenerateOperationId(const std::string& strTradeId);

    // Audit log keyed by operation ID — each entry is one audit record.
    // GetAuditTrail scans this map to find all records for a given trade ID.
    std::map<std::string, SAuditRecord> m_mapAuditLog;

    long m_nOperationCounter;
    bool m_bInitialized;

    static CTradeAuditService* s_pInstance;
};
