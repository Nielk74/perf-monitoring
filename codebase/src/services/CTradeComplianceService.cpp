#include "CTradeComplianceService.h"

CTradeComplianceService* CTradeComplianceService::s_pInstance = nullptr;

CTradeComplianceService* CTradeComplianceService::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeComplianceService();
    }
    return s_pInstance;
}

void CTradeComplianceService::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeComplianceService::CTradeComplianceService()
    : m_pLifecycleService(nullptr)
    , m_pCounterpartyService(nullptr)
    , m_bInitialized(false)
{}

CTradeComplianceService::~CTradeComplianceService() {}

bool CTradeComplianceService::Initialize(CTradeLifecycleService* pLifecycleService,
                                          CCounterpartyService* pCounterpartyService)
{
    if (!pLifecycleService || !pCounterpartyService) return false;
    m_pLifecycleService    = pLifecycleService;
    m_pCounterpartyService = pCounterpartyService;
    m_bInitialized = true;
    return true;
}

// Adds a counterparty CODE to the sanctions set.
// Example: AddSanctionedCounterparty("CPTY_BAD")
// The set is checked during RunComplianceCheck.
void CTradeComplianceService::AddSanctionedCounterparty(const std::string& strCptyCode)
{
    m_setSanctionedCodes.insert(strCptyCode);
}

// Runs sanctions screening for a trade.
//
// BUG: RunComplianceCheck passes cptyData.m_strName to IsCounterpartySanctioned
// instead of the counterparty CODE (m_strCounterpartyCode or cptyData.m_strCode).
//
// AddSanctionedCounterparty stores CODES — e.g. "CPTY_BAD".
// m_setSanctionedCodes = { "CPTY_BAD" }
//
// But the sanctions check here uses the counterparty's display NAME:
//   IsCounterpartySanctioned(cptyData.m_strName)  ← "Bad Corp International Ltd"
//
// That string is never in m_setSanctionedCodes, so all sanctioned counterparties
// pass the compliance check. No trade is ever rejected by sanctions screening.
ServiceResult<SComplianceCheckResult> CTradeComplianceService::RunComplianceCheck(
    const std::string& strTradeId)
{
    SComplianceCheckResult result;
    result.m_strTradeId = strTradeId;

    if (!m_bInitialized) {
        result.m_bPassed = false;
        result.m_strFailureReason = "ComplianceService not initialized";
        return ServiceResult<SComplianceCheckResult>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE, result.m_strFailureReason);
    }

    // Step 1: fetch the trade to get the counterparty code
    auto dataResult = m_pLifecycleService->GetTradeData(strTradeId);
    if (dataResult.IsFailure()) {
        result.m_bPassed = false;
        result.m_strFailureReason = "Trade not found";
        return ServiceResult<SComplianceCheckResult>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND, result.m_strFailureReason);
    }

    const STradeLifecycleData& tradeData = dataResult.GetValue();
    const std::string& strCptyCode = tradeData.m_strCounterpartyCode;

    // Step 2: load the full counterparty record to get its display name
    auto cptyResult = m_pCounterpartyService->LoadCounterpartyByCode(strCptyCode);
    if (cptyResult.IsFailure()) {
        // If we can't load the counterparty, fail safe (block the trade)
        result.m_bPassed = false;
        result.m_bCounterpartyFound = false;
        result.m_strFailureReason = "Counterparty not found: " + strCptyCode;
        return ServiceResult<SComplianceCheckResult>::Success(result);
    }

    const SCounterpartyData& cptyData = cptyResult.GetValue();
    result.m_bCounterpartyFound = true;

    // Step 3: check against sanctions list
    // BUG: passes cptyData.m_strName (e.g. "Bad Corp International Ltd") to
    //      IsCounterpartySanctioned, but m_setSanctionedCodes contains CODES
    //      (e.g. "CPTY_BAD"). The name never matches the code, so this check
    //      always returns false — no counterparty is ever flagged as sanctioned.
    //
    // Correct call: IsCounterpartySanctioned(strCptyCode)
    //           or: IsCounterpartySanctioned(cptyData.m_strCode)
    if (IsCounterpartySanctioned(cptyData.m_strName)) {
        result.m_bPassed     = false;
        result.m_bSanctioned = true;
        result.m_strFailureReason = "Counterparty is sanctioned: " + strCptyCode;
        return ServiceResult<SComplianceCheckResult>::Success(result);
    }

    result.m_bPassed = true;
    return ServiceResult<SComplianceCheckResult>::Success(result);
}

bool CTradeComplianceService::IsCounterpartySanctioned(const std::string& strCptyCode) const
{
    return m_setSanctionedCodes.find(strCptyCode) != m_setSanctionedCodes.end();
}
