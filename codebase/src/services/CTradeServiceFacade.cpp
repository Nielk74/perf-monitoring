#include "services/CTradeServiceFacade.h"
#include "services/CCounterpartyService.h"
#include "services/CCreditCheckService.h"
#include "services/CTradeLifecycleService.h"
#include "services/CSettlementService.h"
#include "services/CNotificationService.h"
#include "util/CLogger.h"
#include <ctime>
#include <sstream>

CTradeServiceFacade* CTradeServiceFacade::s_pInstance = NULL;
CTradeServiceFacade* g_pTradeServiceFacade = NULL;

CTradeServiceFacade::CTradeServiceFacade()
    : m_pCounterpartyService(NULL)
    , m_pCreditCheckService(NULL)
    , m_pLifecycleService(NULL)
    , m_pSettlementService(NULL)
    , m_pNotificationService(NULL)
    , m_bInitialized(false)
    , m_nTotalWorkflowsStarted(0)
    , m_nTotalWorkflowsCompleted(0)
    , m_nTotalWorkflowsFailed(0)
{
}

CTradeServiceFacade::CTradeServiceFacade(const CTradeServiceFacade& refOther)
{
}

CTradeServiceFacade& CTradeServiceFacade::operator=(const CTradeServiceFacade& refOther)
{
    return *this;
}

CTradeServiceFacade::~CTradeServiceFacade()
{
    Shutdown();
}

CTradeServiceFacade* CTradeServiceFacade::GetInstance()
{
    if (s_pInstance == NULL) {
        s_pInstance = new CTradeServiceFacade();
        g_pTradeServiceFacade = s_pInstance;
    }
    return s_pInstance;
}

void CTradeServiceFacade::DestroyInstance()
{
    if (s_pInstance != NULL) {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pTradeServiceFacade = NULL;
    }
}

bool CTradeServiceFacade::Initialize(
    CCounterpartyService* pCounterpartyService,
    CCreditCheckService* pCreditCheckService,
    CTradeLifecycleService* pLifecycleService,
    CSettlementService* pSettlementService,
    CNotificationService* pNotificationService)
{
    if (m_bInitialized) {
        LOG_WARNING("CTradeServiceFacade already initialized");
        return true;
    }
    
    if (pCounterpartyService == NULL ||
        pCreditCheckService == NULL ||
        pLifecycleService == NULL) {
        LOG_ERROR("CTradeServiceFacade: Required services are NULL");
        return false;
    }
    
    m_pCounterpartyService = pCounterpartyService;
    m_pCreditCheckService = pCreditCheckService;
    m_pLifecycleService = pLifecycleService;
    m_pSettlementService = pSettlementService;
    m_pNotificationService = pNotificationService;
    m_bInitialized = true;
    
    LOG_INFO("CTradeServiceFacade initialized successfully");
    return true;
}

bool CTradeServiceFacade::Shutdown()
{
    if (!m_bInitialized) {
        return true;
    }
    
    m_mapTradeContexts.clear();
    m_pCounterpartyService = NULL;
    m_pCreditCheckService = NULL;
    m_pLifecycleService = NULL;
    m_pSettlementService = NULL;
    m_pNotificationService = NULL;
    m_bInitialized = false;
    
    LOG_INFO("CTradeServiceFacade shutdown complete");
    return true;
}

ServiceResult<SFullTradeContext> CTradeServiceFacade::CreateAndValidateTrade(
    const std::string& strCptyCode,
    double dAmount,
    int nProductType,
    const std::string& strCreatedBy)
{
    if (!m_bInitialized) {
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    m_nTotalWorkflowsStarted++;
    
    SFullTradeContext context;
    context.m_strCounterpartyCode = strCptyCode;
    context.m_dAmount = dAmount;
    context.m_nProductType = nProductType;
    context.m_strLastOperation = "CreateAndValidateTrade";
    
    if (!LoadCounterpartyInfo(context)) {
        m_nTotalWorkflowsFailed++;
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND,
            "Failed to load counterparty information");
    }
    
    ServiceResult<bool> statusResult = 
        m_pCounterpartyService->ValidateCounterpartyStatus(strCptyCode);
    if (statusResult.IsFailure()) {
        m_nTotalWorkflowsFailed++;
        context.m_strLastError = statusResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            statusResult.GetErrorCode(),
            statusResult.GetErrorMessage());
    }
    
    ServiceResult<STradeLifecycleData> createResult = 
        m_pLifecycleService->CreateTradeDraft(strCptyCode, dAmount, nProductType, strCreatedBy);
    
    if (createResult.IsFailure()) {
        m_nTotalWorkflowsFailed++;
        context.m_strLastError = createResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            createResult.GetErrorCode(),
            createResult.GetErrorMessage());
    }
    
    const STradeLifecycleData& tradeData = createResult.GetValue();
    context.m_strTradeId = tradeData.m_strTradeId;
    context.m_eCurrentState = tradeData.m_eCurrentState;
    UpdateContextFromTrade(context, tradeData);
    
    ServiceResult<STradeLifecycleData> submitResult = 
        m_pLifecycleService->SubmitForValidation(context.m_strTradeId);
    
    if (submitResult.IsFailure()) {
        m_nTotalWorkflowsFailed++;
        context.m_strLastError = submitResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            submitResult.GetErrorCode(),
            submitResult.GetErrorMessage());
    }
    
    ServiceResult<STradeLifecycleData> validateResult = 
        m_pLifecycleService->ValidateTrade(context.m_strTradeId);
    
    if (validateResult.IsFailure()) {
        m_nTotalWorkflowsFailed++;
        context.m_strLastError = validateResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            validateResult.GetErrorCode(),
            validateResult.GetErrorMessage());
    }
    
    UpdateContextFromTrade(context, validateResult.GetValue());
    
    if (context.m_eCurrentState == TradeLifecycleState::Invalid) {
        m_nTotalWorkflowsFailed++;
        context.m_strLastError = validateResult.GetValue().m_strValidationError;
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_VALIDATION_FAILED,
            context.m_strLastError);
    }
    
    if (m_pNotificationService != NULL) {
        m_pNotificationService->NotifyTradeValidated(
            context.m_strTradeId, true, "Validation passed");
    }
    
    StoreTradeContext(context);
    m_nTotalWorkflowsCompleted++;
    
    std::stringstream ss;
    ss << "Trade created and validated: " << context.m_strTradeId;
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<SFullTradeContext>::Success(context);
}

ServiceResult<SFullTradeContext> CTradeServiceFacade::SubmitAndCheckCredit(
    const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    SFullTradeContext context;
    if (!GetStoredContext(strTradeId, context)) {
        ServiceResult<STradeLifecycleData> tradeResult = 
            m_pLifecycleService->GetTradeData(strTradeId);
        
        if (tradeResult.IsFailure()) {
            return ServiceResult<SFullTradeContext>::Failure(
                tradeResult.GetErrorCode(),
                tradeResult.GetErrorMessage());
        }
        
        context.m_strTradeId = strTradeId;
        UpdateContextFromTrade(context, tradeResult.GetValue());
        
        if (!LoadCounterpartyInfo(context)) {
            return ServiceResult<SFullTradeContext>::Failure(
                SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND,
                "Failed to load counterparty information");
        }
    }
    
    context.m_strLastOperation = "SubmitAndCheckCredit";
    
    ServiceResult<STradeLifecycleData> submitResult = 
        m_pLifecycleService->SubmitTrade(strTradeId);
    
    if (submitResult.IsFailure()) {
        context.m_strLastError = submitResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            submitResult.GetErrorCode(),
            submitResult.GetErrorMessage());
    }
    
    UpdateContextFromTrade(context, submitResult.GetValue());
    context.m_bCreditReserved = submitResult.GetValue().m_bCreditReserved;
    context.m_strCreditReservationId = submitResult.GetValue().m_strCreditReservationId;
    
    if (context.m_bCreditReserved) {
        context.m_dReservedCredit = context.m_dAmount;
    }
    
    if (m_pNotificationService != NULL) {
        m_pNotificationService->NotifyTradeSubmitted(
            strTradeId, "Submitted for approval");
    }
    
    StoreTradeContext(context);
    
    std::stringstream ss;
    ss << "Trade submitted and credit checked: " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<SFullTradeContext>::Success(context);
}

ServiceResult<SFullTradeContext> CTradeServiceFacade::ApproveAndReserveCredit(
    const std::string& strTradeId,
    const std::string& strApprover)
{
    if (!m_bInitialized) {
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    SFullTradeContext context;
    if (!GetStoredContext(strTradeId, context)) {
        ServiceResult<STradeLifecycleData> tradeResult = 
            m_pLifecycleService->GetTradeData(strTradeId);
        
        if (tradeResult.IsFailure()) {
            return ServiceResult<SFullTradeContext>::Failure(
                tradeResult.GetErrorCode(),
                tradeResult.GetErrorMessage());
        }
        
        context.m_strTradeId = strTradeId;
        UpdateContextFromTrade(context, tradeResult.GetValue());
    }
    
    context.m_strLastOperation = "ApproveAndReserveCredit";
    
    ServiceResult<STradeLifecycleData> approveResult = 
        m_pLifecycleService->ApproveTrade(strTradeId, strApprover);
    
    if (approveResult.IsFailure()) {
        context.m_strLastError = approveResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            approveResult.GetErrorCode(),
            approveResult.GetErrorMessage());
    }
    
    UpdateContextFromTrade(context, approveResult.GetValue());
    
    if (m_pNotificationService != NULL) {
        m_pNotificationService->NotifyTradeApproved(strTradeId, strApprover);
    }
    
    StoreTradeContext(context);
    
    std::stringstream ss;
    ss << "Trade approved: " << strTradeId << " by " << strApprover;
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<SFullTradeContext>::Success(context);
}

ServiceResult<SFullTradeContext> CTradeServiceFacade::ExecuteAndNotify(
    const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    SFullTradeContext context;
    if (!GetStoredContext(strTradeId, context)) {
        ServiceResult<STradeLifecycleData> tradeResult = 
            m_pLifecycleService->GetTradeData(strTradeId);
        
        if (tradeResult.IsFailure()) {
            return ServiceResult<SFullTradeContext>::Failure(
                tradeResult.GetErrorCode(),
                tradeResult.GetErrorMessage());
        }
        
        context.m_strTradeId = strTradeId;
        UpdateContextFromTrade(context, tradeResult.GetValue());
    }
    
    context.m_strLastOperation = "ExecuteAndNotify";
    
    ServiceResult<STradeLifecycleData> executeResult = 
        m_pLifecycleService->ExecuteTrade(strTradeId);
    
    if (executeResult.IsFailure()) {
        context.m_strLastError = executeResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            executeResult.GetErrorCode(),
            executeResult.GetErrorMessage());
    }
    
    UpdateContextFromTrade(context, executeResult.GetValue());
    
    if (m_pNotificationService != NULL) {
        m_pNotificationService->NotifyTradeExecuted(strTradeId, "Trade executed successfully");
    }
    
    StoreTradeContext(context);
    
    std::stringstream ss;
    ss << "Trade executed: " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<SFullTradeContext>::Success(context);
}

ServiceResult<SFullTradeContext> CTradeServiceFacade::SettleAndNotify(
    const std::string& strTradeId,
    const std::string& strSettlementDate)
{
    if (!m_bInitialized) {
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    if (m_pSettlementService == NULL) {
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "Settlement service not available");
    }
    
    SFullTradeContext context;
    if (!GetStoredContext(strTradeId, context)) {
        ServiceResult<STradeLifecycleData> tradeResult = 
            m_pLifecycleService->GetTradeData(strTradeId);
        
        if (tradeResult.IsFailure()) {
            return ServiceResult<SFullTradeContext>::Failure(
                tradeResult.GetErrorCode(),
                tradeResult.GetErrorMessage());
        }
        
        context.m_strTradeId = strTradeId;
        UpdateContextFromTrade(context, tradeResult.GetValue());
    }
    
    context.m_strLastOperation = "SettleAndNotify";
    
    ServiceResult<SSettlementRecord> scheduleResult = 
        m_pSettlementService->ScheduleSettlement(strTradeId, strSettlementDate);
    
    if (scheduleResult.IsFailure()) {
        context.m_strLastError = scheduleResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            scheduleResult.GetErrorCode(),
            scheduleResult.GetErrorMessage());
    }
    
    ServiceResult<SSettlementRecord> processResult = 
        m_pSettlementService->ProcessSettlement(strTradeId);
    
    if (processResult.IsFailure()) {
        context.m_strLastError = processResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            processResult.GetErrorCode(),
            processResult.GetErrorMessage());
    }
    
    context.m_eCurrentState = TradeLifecycleState::Settled;

    // BUG: m_pLifecycleService->RecordTradeSettled() was removed during refactor — lifecycle
    // service never transitions this trade to Settled, so GetTradeState still returns Executed.
    // CSettlementService::IsTradeSettleable reads from m_pTradeLifecycleService->GetTradeState,
    // which will keep returning Executed — allowing ScheduleSettlement to pass validation again.

    if (m_pNotificationService != NULL) {
        std::stringstream ss;
        ss << "Settlement amount: " << processResult.GetValue().m_dNetAmount;
        m_pNotificationService->NotifyTradeSettled(strTradeId, ss.str());
    }
    
    StoreTradeContext(context);
    
    std::stringstream ss;
    ss << "Trade settled: " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<SFullTradeContext>::Success(context);
}

ServiceResult<STradeWorkflowResult> CTradeServiceFacade::FullTradeWorkflow(
    const std::string& strCptyCode,
    double dAmount,
    int nProductType,
    const std::string& strCreatedBy,
    const std::string& strApprover,
    const std::string& strSettlementDate)
{
    STradeWorkflowResult workflowResult;
    m_nTotalWorkflowsStarted++;
    
    ServiceResult<SFullTradeContext> createResult = 
        CreateAndValidateTrade(strCptyCode, dAmount, nProductType, strCreatedBy);
    
    if (createResult.IsFailure()) {
        workflowResult.m_bSuccess = false;
        workflowResult.m_strErrorDetails = createResult.GetErrorMessage();
        workflowResult.m_nErrorCode = createResult.GetErrorCode();
        m_nTotalWorkflowsFailed++;
        return ServiceResult<STradeWorkflowResult>::Success(workflowResult);
    }
    
    workflowResult.m_strTradeId = createResult.GetValue().m_strTradeId;
    
    ServiceResult<SFullTradeContext> submitResult = 
        SubmitAndCheckCredit(workflowResult.m_strTradeId);
    
    if (submitResult.IsFailure()) {
        workflowResult.m_bSuccess = false;
        workflowResult.m_strErrorDetails = submitResult.GetErrorMessage();
        workflowResult.m_nErrorCode = submitResult.GetErrorCode();
        workflowResult.m_eFinalState = submitResult.GetValue().m_eCurrentState;
        m_nTotalWorkflowsFailed++;
        return ServiceResult<STradeWorkflowResult>::Success(workflowResult);
    }
    
    ServiceResult<SFullTradeContext> approveResult = 
        ApproveAndReserveCredit(workflowResult.m_strTradeId, strApprover);
    
    if (approveResult.IsFailure()) {
        workflowResult.m_bSuccess = false;
        workflowResult.m_strErrorDetails = approveResult.GetErrorMessage();
        workflowResult.m_nErrorCode = approveResult.GetErrorCode();
        workflowResult.m_eFinalState = approveResult.GetValue().m_eCurrentState;
        m_nTotalWorkflowsFailed++;
        return ServiceResult<STradeWorkflowResult>::Success(workflowResult);
    }
    
    ServiceResult<SFullTradeContext> executeResult = 
        ExecuteAndNotify(workflowResult.m_strTradeId);
    
    if (executeResult.IsFailure()) {
        workflowResult.m_bSuccess = false;
        workflowResult.m_strErrorDetails = executeResult.GetErrorMessage();
        workflowResult.m_nErrorCode = executeResult.GetErrorCode();
        workflowResult.m_eFinalState = executeResult.GetValue().m_eCurrentState;
        m_nTotalWorkflowsFailed++;
        return ServiceResult<STradeWorkflowResult>::Success(workflowResult);
    }
    
    workflowResult.m_eFinalState = executeResult.GetValue().m_eCurrentState;
    
    if (!strSettlementDate.empty() && m_pSettlementService != NULL) {
        ServiceResult<SFullTradeContext> settleResult = 
            SettleAndNotify(workflowResult.m_strTradeId, strSettlementDate);
        
        if (settleResult.IsSuccess()) {
            workflowResult.m_eFinalState = settleResult.GetValue().m_eCurrentState;
        }
    }
    
    workflowResult.m_bSuccess = true;
    workflowResult.m_strMessage = "Trade workflow completed successfully";
    m_nTotalWorkflowsCompleted++;
    
    std::stringstream ss;
    ss << "Full trade workflow completed: " << workflowResult.m_strTradeId
       << " final state: " << GetLifecycleStateName(workflowResult.m_eFinalState);
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<STradeWorkflowResult>::Success(workflowResult);
}

ServiceResult<SFullTradeContext> CTradeServiceFacade::CancelTradeWorkflow(
    const std::string& strTradeId,
    const std::string& strReason)
{
    if (!m_bInitialized) {
        return ServiceResult<SFullTradeContext>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    SFullTradeContext context;
    if (!GetStoredContext(strTradeId, context)) {
        ServiceResult<STradeLifecycleData> tradeResult = 
            m_pLifecycleService->GetTradeData(strTradeId);
        
        if (tradeResult.IsFailure()) {
            return ServiceResult<SFullTradeContext>::Failure(
                tradeResult.GetErrorCode(),
                tradeResult.GetErrorMessage());
        }
        
        context.m_strTradeId = strTradeId;
        UpdateContextFromTrade(context, tradeResult.GetValue());
    }
    
    context.m_strLastOperation = "CancelTradeWorkflow";
    
    if (context.m_bCreditReserved && m_pCreditCheckService != NULL) {
        m_pCreditCheckService->ReleaseCreditReservation(strTradeId);
        context.m_bCreditReserved = false;
        context.m_dReservedCredit = 0;
    }
    
    ServiceResult<STradeLifecycleData> cancelResult = 
        m_pLifecycleService->CancelTrade(strTradeId, strReason);
    
    if (cancelResult.IsFailure()) {
        context.m_strLastError = cancelResult.GetErrorMessage();
        return ServiceResult<SFullTradeContext>::Failure(
            cancelResult.GetErrorCode(),
            cancelResult.GetErrorMessage());
    }
    
    UpdateContextFromTrade(context, cancelResult.GetValue());
    
    if (m_pNotificationService != NULL) {
        m_pNotificationService->NotifyTradeCancelled(strTradeId, strReason);
    }
    
    StoreTradeContext(context);
    
    std::stringstream ss;
    ss << "Trade cancelled: " << strTradeId << " - " << strReason;
    LOG_WARNING(ss.str().c_str());
    
    return ServiceResult<SFullTradeContext>::Success(context);
}

ServiceResult<SFullTradeContext> CTradeServiceFacade::GetTradeContext(
    const std::string& strTradeId)
{
    SFullTradeContext context;
    if (GetStoredContext(strTradeId, context)) {
        return ServiceResult<SFullTradeContext>::Success(context);
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pLifecycleService->GetTradeData(strTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<SFullTradeContext>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    context.m_strTradeId = strTradeId;
    UpdateContextFromTrade(context, tradeResult.GetValue());
    
    if (!context.m_strCounterpartyCode.empty() && m_pCounterpartyService != NULL) {
        LoadCounterpartyInfo(context);
    }
    
    return ServiceResult<SFullTradeContext>::Success(context);
}

ServiceResult<bool> CTradeServiceFacade::ValidateCounterpartyAndCredit(
    const std::string& strCptyCode,
    double dAmount)
{
    if (!m_bInitialized) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    ServiceResult<bool> statusResult = 
        m_pCounterpartyService->ValidateCounterpartyStatus(strCptyCode);
    
    if (statusResult.IsFailure()) {
        return statusResult;
    }
    
    ServiceResult<bool> creditResult = 
        m_pCreditCheckService->CheckCreditLimit(strCptyCode, dAmount);
    
    return creditResult;
}

ServiceResult<bool> CTradeServiceFacade::QuickCreditCheck(
    const std::string& strCptyCode,
    double dAmount)
{
    if (!m_bInitialized) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeServiceFacade not initialized");
    }
    
    return m_pCreditCheckService->CheckCreditLimit(strCptyCode, dAmount);
}

size_t CTradeServiceFacade::GetActiveTradeCount() const
{
    return m_mapTradeContexts.size();
}

void CTradeServiceFacade::ClearTradeContexts()
{
    m_mapTradeContexts.clear();
}

bool CTradeServiceFacade::ValidateServices() const
{
    return m_pCounterpartyService != NULL &&
           m_pCreditCheckService != NULL &&
           m_pLifecycleService != NULL;
}

bool CTradeServiceFacade::LoadCounterpartyInfo(SFullTradeContext& refContext)
{
    if (m_pCounterpartyService == NULL) {
        return false;
    }
    
    ServiceResult<SCounterpartyData> cptyResult = 
        m_pCounterpartyService->LoadCounterpartyByCode(refContext.m_strCounterpartyCode);
    
    if (cptyResult.IsFailure()) {
        return false;
    }
    
    refContext.m_counterpartyData = cptyResult.GetValue();
    
    ServiceResult<SCounterpartyLimits> limitsResult = 
        m_pCounterpartyService->GetCounterpartyLimits(refContext.m_strCounterpartyCode);
    
    if (limitsResult.IsSuccess()) {
        refContext.m_counterpartyLimits = limitsResult.GetValue();
    }
    
    return true;
}

bool CTradeServiceFacade::PerformCreditCheck(SFullTradeContext& refContext, double dAmount)
{
    if (m_pCreditCheckService == NULL) {
        return false;
    }
    
    ServiceResult<bool> checkResult = 
        m_pCreditCheckService->CheckCreditLimit(refContext.m_strCounterpartyCode, dAmount);
    
    refContext.m_bCreditChecked = checkResult.IsSuccess();
    return refContext.m_bCreditChecked;
}

bool CTradeServiceFacade::ReserveCreditForContext(SFullTradeContext& refContext, double dAmount)
{
    if (m_pCreditCheckService == NULL) {
        return false;
    }
    
    ServiceResult<std::string> reserveResult = 
        m_pCreditCheckService->ReserveCredit(
            refContext.m_strCounterpartyCode,
            dAmount,
            refContext.m_strTradeId);
    
    if (reserveResult.IsSuccess()) {
        refContext.m_bCreditReserved = true;
        refContext.m_dReservedCredit = dAmount;
        refContext.m_strCreditReservationId = reserveResult.GetValue();
        return true;
    }
    
    return false;
}

bool CTradeServiceFacade::ReleaseCreditForContext(SFullTradeContext& refContext)
{
    if (m_pCreditCheckService == NULL || !refContext.m_bCreditReserved) {
        return true;
    }
    
    ServiceResult<bool> releaseResult = 
        m_pCreditCheckService->ReleaseCreditReservation(refContext.m_strTradeId);
    
    if (releaseResult.IsSuccess()) {
        refContext.m_bCreditReserved = false;
        refContext.m_dReservedCredit = 0;
        refContext.m_strCreditReservationId = "";
        return true;
    }
    
    return false;
}

void CTradeServiceFacade::UpdateContextFromTrade(
    SFullTradeContext& refContext,
    const STradeLifecycleData& refTradeData)
{
    refContext.m_eCurrentState = refTradeData.m_eCurrentState;
    refContext.m_strCounterpartyCode = refTradeData.m_strCounterpartyCode;
    refContext.m_dAmount = refTradeData.m_dAmount;
    refContext.m_nProductType = refTradeData.m_nProductType;
    refContext.m_bCreditReserved = refTradeData.m_bCreditReserved;
    refContext.m_strCreditReservationId = refTradeData.m_strCreditReservationId;
}

void CTradeServiceFacade::StoreTradeContext(const SFullTradeContext& refContext)
{
    m_mapTradeContexts[refContext.m_strTradeId] = refContext;
}

bool CTradeServiceFacade::GetStoredContext(const std::string& strTradeId, SFullTradeContext& refContext)
{
    std::map<std::string, SFullTradeContext>::iterator it = 
        m_mapTradeContexts.find(strTradeId);
    
    if (it != m_mapTradeContexts.end()) {
        refContext = it->second;
        return true;
    }
    
    return false;
}

// ---------------------------------------------------------------------------
// BatchExecuteTrades
//
// Executes a list of approved trades in sequence and fires TRADE_EXECUTED
// notifications for each one.
//
// BUG: NotifyTradeExecuted is called BEFORE ExecuteTrade updates the lifecycle
// store. Any notification handler that calls GetTradeState() during the
// notification callback will see the trade still in APPROVED state, not
// EXECUTED state, because StoreTradeData hasn't been called yet at that point.
//
// Correct order: ExecuteTrade → (store updated to Executed) → NotifyTradeExecuted
// Actual order:  NotifyTradeExecuted → (listener reads store = Approved) → ExecuteTrade
// ---------------------------------------------------------------------------
std::vector<std::string> CTradeServiceFacade::BatchExecuteTrades(
    const std::vector<std::string>& vecTradeIds)
{
    std::vector<std::string> vecFailed;

    for (size_t i = 0; i < vecTradeIds.size(); ++i)
    {
        const std::string& strTradeId = vecTradeIds[i];

        // Verify the trade is in APPROVED state before proceeding
        auto stateResult = m_pLifecycleService->GetTradeState(strTradeId);
        if (stateResult.IsFailure()) {
            vecFailed.push_back(strTradeId);
            continue;
        }

        if (stateResult.GetValue() != TradeLifecycleState::Approved) {
            vecFailed.push_back(strTradeId);
            continue;
        }

        // BUG: fire the "trade executed" notification first, THEN actually
        // execute the trade. Handlers that call GetTradeState inside
        // OnTradeExecuted will still see TradeLifecycleState::Approved.
        m_pNotificationService->NotifyTradeExecuted(strTradeId, "batch-execute");

        // Execute (updates store to Executed) — happens after notification
        auto execResult = m_pLifecycleService->ExecuteTrade(strTradeId);
        if (execResult.IsFailure()) {
            vecFailed.push_back(strTradeId);
        }
    }

    return vecFailed;
}
