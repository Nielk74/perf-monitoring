#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include <vector>
#include "ServiceCommon.h"
#include "CTradeLifecycleService.h"
#include "CSettlementService.h"

struct SSettlementFinalRecord {
    std::string m_strTradeId;
    std::string m_strSettlementRef;    // the settlement reference/ID
    double      m_dFinalAmount;
    std::string m_strFinalizedBy;
    bool        m_bFinalized;

    SSettlementFinalRecord()
        : m_dFinalAmount(0.0)
        , m_bFinalized(false)
    {}
};

// Finalizes settlement for a batch of trades.
// After each trade settles, this service records the final settlement details
// and marks the trade as finalized in its internal ledger.
class CSettlementFinalizationService
{
public:
    static CSettlementFinalizationService* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeLifecycleService* pLifecycleService,
                    CSettlementService* pSettlementService);

    // Finalize settlement for a single trade.
    // Reads the trade's settlement info from CSettlementService, records
    // the final amount and reference, then updates the internal ledger.
    ServiceResult<SSettlementFinalRecord> FinalizeSettlement(
        const std::string& strTradeId,
        const std::string& strFinalizedBy);

    // Finalize a batch of trades. Returns the IDs of trades that failed.
    std::vector<std::string> BatchFinalizeSettlements(
        const std::vector<std::string>& vecTradeIds,
        const std::string& strFinalizedBy);

    // Returns true if the trade has been finalized.
    bool IsFinalized(const std::string& strTradeId) const;

    // Returns the finalized record for a trade, or failure if not found.
    ServiceResult<SSettlementFinalRecord> GetFinalRecord(
        const std::string& strTradeId) const;

    size_t GetFinalizedCount() const { return m_mapFinalRecords.size(); }

private:
    CSettlementFinalizationService();
    ~CSettlementFinalizationService();
    CSettlementFinalizationService(const CSettlementFinalizationService&);
    CSettlementFinalizationService& operator=(const CSettlementFinalizationService&);

    // Ledger keyed by trade ID
    std::map<std::string, SSettlementFinalRecord> m_mapFinalRecords;

    CTradeLifecycleService* m_pLifecycleService;
    CSettlementService*     m_pSettlementService;
    bool m_bInitialized;

    static CSettlementFinalizationService* s_pInstance;
};
