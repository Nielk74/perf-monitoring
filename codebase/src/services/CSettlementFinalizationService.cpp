#include "CSettlementFinalizationService.h"

CSettlementFinalizationService* CSettlementFinalizationService::s_pInstance = nullptr;

CSettlementFinalizationService* CSettlementFinalizationService::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CSettlementFinalizationService();
    }
    return s_pInstance;
}

void CSettlementFinalizationService::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CSettlementFinalizationService::CSettlementFinalizationService()
    : m_pLifecycleService(nullptr)
    , m_pSettlementService(nullptr)
    , m_bInitialized(false)
{}

CSettlementFinalizationService::~CSettlementFinalizationService() {}

bool CSettlementFinalizationService::Initialize(
    CTradeLifecycleService* pLifecycleService,
    CSettlementService* pSettlementService)
{
    if (!pLifecycleService || !pSettlementService) return false;
    m_pLifecycleService  = pLifecycleService;
    m_pSettlementService = pSettlementService;
    m_bInitialized = true;
    return true;
}

// Finalizes settlement for a single trade.
//
// The intended flow:
//   1. Read trade data  → tradeData.m_strTradeId  (string, e.g. "TRADE-0042")
//   2. Fetch settlement → settlementRecord.m_strSettlementReference
//   3. Store the final record under the trade's string ID
//
// BUG: GetSettlementDetails is called using tradeData.m_nTradeId (the NUMERIC ID)
// via the long overload. GetSettlementDetails(long) looks up by numeric ID in
// CSettlementService::m_mapSettlementsByNumericId.
//
// But CSettlementService::ScheduleSettlement and ProcessSettlement both store
// their records in CSettlementService::m_mapSettlements keyed by the STRING trade ID.
// The numeric-ID overload of GetSettlementDetails searches a DIFFERENT map
// (m_mapSettlementsByNumericId) which is only populated when the settlement was
// originally scheduled via the long overload. If the trade was settled via the
// string overload (the normal path), m_mapSettlementsByNumericId has no entry and
// GetSettlementDetails(long) returns Failure — as if the settlement never existed.
//
// Result: FinalizeSettlement always fails with "Settlement details not found"
// even for trades that were successfully settled.
ServiceResult<SSettlementFinalRecord> CSettlementFinalizationService::FinalizeSettlement(
    const std::string& strTradeId,
    const std::string& strFinalizedBy)
{
    if (!m_bInitialized) {
        return ServiceResult<SSettlementFinalRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE, "FinalizationService not initialized");
    }

    // Step 1: verify the trade exists and get its numeric ID
    auto dataResult = m_pLifecycleService->GetTradeData(strTradeId);
    if (dataResult.IsFailure()) {
        return ServiceResult<SSettlementFinalRecord>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND, "Trade not found: " + strTradeId);
    }

    const STradeLifecycleData& tradeData = dataResult.GetValue();

    // Step 2: fetch settlement details
    // BUG: calls GetSettlementDetails(long) using the numeric ID.
    // Settlements scheduled via ProcessSettlement(string) are stored under the
    // string ID only. The long-overload lookup uses a separate numeric-ID index
    // that is never populated for string-scheduled settlements.
    //
    // Fix: use GetSettlementDetails(strTradeId) — the string overload.
    auto settlementResult = m_pSettlementService->GetSettlementDetails(
        tradeData.m_nTradeId);  // BUG: should be GetSettlementDetails(strTradeId)

    if (settlementResult.IsFailure()) {
        return ServiceResult<SSettlementFinalRecord>::Failure(
            SERVICES_ERRORCODE_UNKNOWN_ERROR,
            "Settlement details not found for trade: " + strTradeId);
    }

    const SSettlementRecord& settlement = settlementResult.GetValue();

    // Step 3: record the finalization
    SSettlementFinalRecord record;
    record.m_strTradeId       = strTradeId;
    record.m_strSettlementRef = settlement.m_strSettlementReference;
    record.m_dFinalAmount     = settlement.m_dSettlementAmount;
    record.m_strFinalizedBy   = strFinalizedBy;
    record.m_bFinalized       = true;

    m_mapFinalRecords[strTradeId] = record;

    return ServiceResult<SSettlementFinalRecord>::Success(record);
}

std::vector<std::string> CSettlementFinalizationService::BatchFinalizeSettlements(
    const std::vector<std::string>& vecTradeIds,
    const std::string& strFinalizedBy)
{
    std::vector<std::string> vecFailed;
    for (size_t i = 0; i < vecTradeIds.size(); ++i) {
        auto result = FinalizeSettlement(vecTradeIds[i], strFinalizedBy);
        if (result.IsFailure()) {
            vecFailed.push_back(vecTradeIds[i]);
        }
    }
    return vecFailed;
}

bool CSettlementFinalizationService::IsFinalized(const std::string& strTradeId) const
{
    return m_mapFinalRecords.find(strTradeId) != m_mapFinalRecords.end();
}

ServiceResult<SSettlementFinalRecord> CSettlementFinalizationService::GetFinalRecord(
    const std::string& strTradeId) const
{
    auto it = m_mapFinalRecords.find(strTradeId);
    if (it == m_mapFinalRecords.end()) {
        return ServiceResult<SSettlementFinalRecord>::Failure(
            SERVICES_ERRORCODE_UNKNOWN_ERROR,
            "No finalization record for trade: " + strTradeId);
    }
    return ServiceResult<SSettlementFinalRecord>::Success(it->second);
}
