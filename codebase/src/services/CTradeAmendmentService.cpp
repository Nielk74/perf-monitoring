#include "CTradeAmendmentService.h"
#include "CTradeLifecycleService.h"
#include "CNotificationService.h"

#include <sstream>
#include <ctime>

#define LOG_INFO(x)

CTradeAmendmentService* CTradeAmendmentService::s_pInstance = nullptr;

CTradeAmendmentService* CTradeAmendmentService::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeAmendmentService();
    }
    return s_pInstance;
}

void CTradeAmendmentService::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeAmendmentService::CTradeAmendmentService()
    : m_pLifecycleService(nullptr)
    , m_pNotificationService(nullptr)
    , m_bInitialized(false)
    , m_nNextAmendmentId(1)
{}

CTradeAmendmentService::~CTradeAmendmentService() {}

bool CTradeAmendmentService::Initialize(
    CTradeLifecycleService* pLifecycleService,
    CNotificationService*   pNotificationService)
{
    if (!pLifecycleService || !pNotificationService) return false;
    m_pLifecycleService     = pLifecycleService;
    m_pNotificationService  = pNotificationService;
    m_bInitialized          = true;
    return true;
}

// Amend the notional of a trade and re-run validation.
// The intent: update the stored trade amount to dNewNotional, then validate.
// If validation passes the amendment is committed; if it fails the trade retains
// its old notional.
ServiceResult<STradeAmendmentRecord> CTradeAmendmentService::AmendTradeNotional(
    const std::string& strTradeId,
    double             dNewNotional,
    const std::string& strAmendedBy,
    const std::string& strReason)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeAmendmentRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "AmendmentService not initialized");
    }

    // Step 1: read current trade data to record old notional
    auto readResult = m_pLifecycleService->GetTradeData(strTradeId);
    if (readResult.IsFailure()) {
        return ServiceResult<STradeAmendmentRecord>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }

    STradeLifecycleData currentData = readResult.GetValue();
    double dOldNotional = currentData.m_dAmount;

    // Step 2: build the amendment record
    STradeAmendmentRecord amendment;
    amendment.m_strTradeId     = strTradeId;
    amendment.m_strAmendmentId = GenerateAmendmentId();
    amendment.m_dOldNotional   = dOldNotional;
    amendment.m_dNewNotional   = dNewNotional;
    amendment.m_strAmendedBy   = strAmendedBy;
    amendment.m_strReason      = strReason;
    amendment.m_nAmendmentTime = (long)time(nullptr);

    // Step 3: validate the trade with the new notional.
    //
    // BUG: ValidateTrade reads the trade amount from m_mapTradeStore, which still
    // holds the OLD notional (dOldNotional) because we haven't written dNewNotional
    // back to the lifecycle store yet. The new amount only exists in our local
    // 'currentData' copy and in the amendment record. Any zero-notional check inside
    // ValidateTradeInternal will therefore run against the OLD value, not dNewNotional.
    auto validateResult = m_pLifecycleService->ValidateTrade(strTradeId);
    if (validateResult.IsFailure()) {
        return ServiceResult<STradeAmendmentRecord>::Failure(
            SERVICES_ERRORCODE_VALIDATION_FAILED,
            "Amendment validation failed: " + validateResult.GetErrorMessage());
    }

    // Step 4: record the amendment and update the local copy's amount.
    // The lifecycle store is never updated with dNewNotional — only the amendment
    // map is updated. Subsequent calls to GetTradeData still return dOldNotional.
    currentData.m_dAmount    = dNewNotional;
    amendment.m_bValidated   = true;
    m_mapAmendments[amendment.m_strAmendmentId] = amendment;

    std::stringstream ss;
    ss << "Trade " << strTradeId << " notional amended from "
       << dOldNotional << " to " << dNewNotional;
    LOG_INFO(ss.str().c_str());

    return ServiceResult<STradeAmendmentRecord>::Success(amendment);
}

ServiceResult<STradeAmendmentRecord> CTradeAmendmentService::GetAmendmentRecord(
    const std::string& strAmendmentId)
{
    auto it = m_mapAmendments.find(strAmendmentId);
    if (it == m_mapAmendments.end()) {
        return ServiceResult<STradeAmendmentRecord>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND, "Amendment not found");
    }
    return ServiceResult<STradeAmendmentRecord>::Success(it->second);
}

std::string CTradeAmendmentService::GenerateAmendmentId()
{
    std::stringstream ss;
    ss << "AMD_" << (long)time(nullptr) << "_" << m_nNextAmendmentId++;
    return ss.str();
}

size_t CTradeAmendmentService::GetAmendmentCount() const
{
    return m_mapAmendments.size();
}
