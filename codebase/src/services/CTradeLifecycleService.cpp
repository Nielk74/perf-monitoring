#include "services/CTradeLifecycleService.h"
#include "services/CCreditCheckService.h"
#include "legacy/CTradeEventDispatcher.h"
#include "util/CLogger.h"
#include <ctime>
#include <sstream>

CTradeLifecycleService* CTradeLifecycleService::s_pInstance = NULL;
CTradeLifecycleService* g_pTradeLifecycleService = NULL;

CTradeLifecycleService::CTradeLifecycleService()
    : m_pCreditCheckService(NULL)
    , m_pEventDispatcher(NULL)
    , m_bInitialized(false)
    , m_nNextTradeId(1)
    , m_nTotalTradesCreated(0)
    , m_nTotalTradesExecuted(0)
    , m_nTotalTradesCancelled(0)
{
}

CTradeLifecycleService::CTradeLifecycleService(const CTradeLifecycleService& refOther)
{
}

CTradeLifecycleService& CTradeLifecycleService::operator=(const CTradeLifecycleService& refOther)
{
    return *this;
}

CTradeLifecycleService::~CTradeLifecycleService()
{
    Shutdown();
}

CTradeLifecycleService* CTradeLifecycleService::GetInstance()
{
    if (s_pInstance == NULL) {
        s_pInstance = new CTradeLifecycleService();
        g_pTradeLifecycleService = s_pInstance;
    }
    return s_pInstance;
}

void CTradeLifecycleService::DestroyInstance()
{
    if (s_pInstance != NULL) {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pTradeLifecycleService = NULL;
    }
}

bool CTradeLifecycleService::Initialize(
    CCreditCheckService* pCreditCheckService, 
    CTradeEventDispatcher* pEventDispatcher)
{
    if (m_bInitialized) {
        LOG_WARNING("CTradeLifecycleService already initialized");
        return true;
    }
    
    if (pCreditCheckService == NULL) {
        LOG_ERROR("CTradeLifecycleService: NULL credit check service");
        return false;
    }
    
    m_pCreditCheckService = pCreditCheckService;
    m_pEventDispatcher = pEventDispatcher;
    m_bInitialized = true;
    
    LOG_INFO("CTradeLifecycleService initialized successfully");
    return true;
}

bool CTradeLifecycleService::Shutdown()
{
    if (!m_bInitialized) {
        return true;
    }
    
    m_mapTradeStore.clear();
    m_mapNumericIdToTradeId.clear();
    m_pCreditCheckService = NULL;
    m_pEventDispatcher = NULL;
    m_bInitialized = false;
    
    LOG_INFO("CTradeLifecycleService shutdown complete");
    return true;
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::CreateTradeDraft(
    const std::string& strCptyCode,
    double dAmount,
    int nProductType,
    const std::string& strCreatedBy)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    if (strCptyCode.empty()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_VALIDATION_FAILED,
            "Counterparty code is required");
    }
    
    if (dAmount <= 0) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_VALIDATION_FAILED,
            "Amount must be positive");
    }
    
    STradeLifecycleData tradeData;
    tradeData.m_strTradeId = GenerateTradeId();
    tradeData.m_nTradeId = GenerateNumericTradeId();
    tradeData.m_strCounterpartyCode = strCptyCode;
    tradeData.m_dAmount = dAmount;
    tradeData.m_nProductType = nProductType;
    tradeData.m_eCurrentState = TradeLifecycleState::Draft;
    tradeData.m_strCreatedBy = strCreatedBy;
    tradeData.m_nCreatedTime = static_cast<long>(std::time(NULL));
    tradeData.m_nLastUpdatedTime = tradeData.m_nCreatedTime;
    tradeData.m_bCreditReserved = false;
    
    StoreTradeData(tradeData);
    m_nTotalTradesCreated++;
    
    std::stringstream ss;
    ss << "Trade created: " << tradeData.m_strTradeId 
       << " for counterparty " << strCptyCode 
       << " amount " << dAmount;
    LOG_INFO(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, TradeLifecycleState::Draft);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::SubmitForValidation(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    if (tradeData.m_eCurrentState != TradeLifecycleState::Draft) {
        std::stringstream ss;
        ss << "Invalid state transition from " << GetLifecycleStateName(tradeData.m_eCurrentState)
           << " to PENDING_VALIDATION";
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            ss.str());
    }
    
    TradeLifecycleState oldState = tradeData.m_eCurrentState;
    UpdateTradeState(tradeData, TradeLifecycleState::PendingValidation);
    StoreTradeData(tradeData);
    
    std::stringstream ss;
    ss << "Trade submitted for validation: " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, oldState);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::SubmitForValidation(long nTradeId)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return SubmitForValidation(it->second);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::ValidateTrade(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    if (tradeData.m_eCurrentState != TradeLifecycleState::PendingValidation) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Trade must be in PENDING_VALIDATION state");
    }
    
    TradeLifecycleState oldState = tradeData.m_eCurrentState;
    
    bool bIsValid = ValidateTradeInternal(tradeData);
    
    if (bIsValid) {
        UpdateTradeState(tradeData, TradeLifecycleState::Validated);
        tradeData.m_vecValidationMessages.push_back("Validation passed");
    } else {
        UpdateTradeState(tradeData, TradeLifecycleState::Invalid);
        tradeData.m_vecValidationMessages.push_back("Validation failed: " + tradeData.m_strValidationError);
    }
    
    StoreTradeData(tradeData);
    
    std::stringstream ss;
    ss << "Trade validation " << (bIsValid ? "passed" : "failed") << ": " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, oldState);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::ValidateTrade(long nTradeId)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return ValidateTrade(it->second);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::SubmitTrade(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    if (tradeData.m_eCurrentState != TradeLifecycleState::Validated) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Trade must be VALIDATED before submission");
    }
    
    TradeLifecycleState oldState = tradeData.m_eCurrentState;
    
    if (!ReserveCreditForTrade(tradeData)) {
        tradeData.m_strValidationError = "Failed to reserve credit";
        UpdateTradeState(tradeData, TradeLifecycleState::Failed);
        StoreTradeData(tradeData);
        
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_CREDIT_RESERVATION_FAILED,
            "Failed to reserve credit for trade");
    }
    
    UpdateTradeState(tradeData, TradeLifecycleState::PendingApproval);
    StoreTradeData(tradeData);
    
    std::stringstream ss;
    ss << "Trade submitted for approval: " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, oldState);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::SubmitTrade(long nTradeId)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return SubmitTrade(it->second);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::ApproveTrade(
    const std::string& strTradeId, 
    const std::string& strApprover)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    if (tradeData.m_eCurrentState != TradeLifecycleState::PendingApproval) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Trade must be in PENDING_APPROVAL state");
    }
    
    TradeLifecycleState oldState = tradeData.m_eCurrentState;
    UpdateTradeState(tradeData, TradeLifecycleState::Approved);
    tradeData.m_strLastUpdatedBy = strApprover;
    StoreTradeData(tradeData);
    
    std::stringstream ss;
    ss << "Trade approved by " << strApprover << ": " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, oldState);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::ApproveTrade(
    long nTradeId, 
    const std::string& strApprover)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return ApproveTrade(it->second, strApprover);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::RejectTrade(
    const std::string& strTradeId, 
    const std::string& strReason)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    TradeLifecycleState oldState = tradeData.m_eCurrentState;
    
    ReleaseCreditForTrade(tradeData);
    
    UpdateTradeState(tradeData, TradeLifecycleState::Rejected);
    tradeData.m_strRejectionReason = strReason;
    StoreTradeData(tradeData);
    
    std::stringstream ss;
    ss << "Trade rejected: " << strTradeId << " - " << strReason;
    LOG_WARNING(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, oldState);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::RejectTrade(
    long nTradeId, 
    const std::string& strReason)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return RejectTrade(it->second, strReason);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::ExecuteTrade(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    if (tradeData.m_eCurrentState != TradeLifecycleState::Approved) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Trade must be APPROVED before execution");
    }
    
    TradeLifecycleState oldState = tradeData.m_eCurrentState;
    UpdateTradeState(tradeData, TradeLifecycleState::Executed);
    StoreTradeData(tradeData);
    
    m_nTotalTradesExecuted++;
    
    std::stringstream ss;
    ss << "Trade executed: " << strTradeId;
    LOG_INFO(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, oldState);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::ExecuteTrade(long nTradeId)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return ExecuteTrade(it->second);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::CancelTrade(
    const std::string& strTradeId, 
    const std::string& strReason)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "TradeLifecycleService not initialized");
    }
    
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    if (IsTradeInTerminalState(strTradeId)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Cannot cancel trade in terminal state");
    }
    
    TradeLifecycleState oldState = tradeData.m_eCurrentState;
    
    ReleaseCreditForTrade(tradeData);
    
    UpdateTradeState(tradeData, TradeLifecycleState::Cancelled);
    tradeData.m_strRejectionReason = strReason;
    StoreTradeData(tradeData);
    
    m_nTotalTradesCancelled++;
    
    std::stringstream ss;
    ss << "Trade cancelled: " << strTradeId << " - " << strReason;
    LOG_WARNING(ss.str().c_str());
    
    DispatchStateChangeEvent(tradeData, oldState);
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::CancelTrade(
    long nTradeId, 
    const std::string& strReason)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return CancelTrade(it->second, strReason);
}

ServiceResult<TradeLifecycleState> CTradeLifecycleService::GetTradeState(const std::string& strTradeId)
{
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<TradeLifecycleState>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    return ServiceResult<TradeLifecycleState>::Success(tradeData.m_eCurrentState);
}

ServiceResult<TradeLifecycleState> CTradeLifecycleService::GetTradeState(long nTradeId)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<TradeLifecycleState>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return GetTradeState(it->second);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::GetTradeData(const std::string& strTradeId)
{
    STradeLifecycleData tradeData;
    if (!GetTradeFromStore(strTradeId, tradeData)) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade not found: " + strTradeId);
    }
    
    return ServiceResult<STradeLifecycleData>::Success(tradeData);
}

ServiceResult<STradeLifecycleData> CTradeLifecycleService::GetTradeData(long nTradeId)
{
    std::map<long, std::string>::iterator it = m_mapNumericIdToTradeId.find(nTradeId);
    if (it == m_mapNumericIdToTradeId.end()) {
        return ServiceResult<STradeLifecycleData>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND,
            "Trade ID not found");
    }
    return GetTradeData(it->second);
}

bool CTradeLifecycleService::IsValidTransition(TradeLifecycleState fromState, TradeLifecycleState toState) const
{
    switch (fromState) {
        case TradeLifecycleState::Draft:
            return toState == TradeLifecycleState::PendingValidation ||
                   toState == TradeLifecycleState::Cancelled;
        
        case TradeLifecycleState::PendingValidation:
            return toState == TradeLifecycleState::Validated ||
                   toState == TradeLifecycleState::Invalid ||
                   toState == TradeLifecycleState::Cancelled;
        
        case TradeLifecycleState::Validated:
            return toState == TradeLifecycleState::PendingApproval ||
                   toState == TradeLifecycleState::Cancelled;
        
        case TradeLifecycleState::Invalid:
            return toState == TradeLifecycleState::Cancelled;
        
        case TradeLifecycleState::PendingApproval:
            return toState == TradeLifecycleState::Approved ||
                   toState == TradeLifecycleState::Rejected ||
                   toState == TradeLifecycleState::Cancelled;
        
        case TradeLifecycleState::Approved:
            return toState == TradeLifecycleState::Executed ||
                   toState == TradeLifecycleState::Cancelled;
        
        case TradeLifecycleState::Rejected:
            return false;
        
        case TradeLifecycleState::Executed:
            return toState == TradeLifecycleState::Settled ||
                   toState == TradeLifecycleState::Failed;
        
        case TradeLifecycleState::Settled:
        case TradeLifecycleState::Cancelled:
        case TradeLifecycleState::Failed:
            return false;
        
        default:
            return false;
    }
}

bool CTradeLifecycleService::CanExecuteTrade(const std::string& strTradeId)
{
    ServiceResult<TradeLifecycleState> stateResult = GetTradeState(strTradeId);
    if (stateResult.IsFailure()) {
        return false;
    }
    
    return stateResult.GetValue() == TradeLifecycleState::Approved;
}

bool CTradeLifecycleService::IsTradeInTerminalState(const std::string& strTradeId)
{
    ServiceResult<TradeLifecycleState> stateResult = GetTradeState(strTradeId);
    if (stateResult.IsFailure()) {
        return true;
    }
    
    TradeLifecycleState state = stateResult.GetValue();
    return state == TradeLifecycleState::Settled ||
           state == TradeLifecycleState::Cancelled ||
           state == TradeLifecycleState::Rejected ||
           state == TradeLifecycleState::Failed;
}

size_t CTradeLifecycleService::GetTradeCount() const
{
    return m_mapTradeStore.size();
}

size_t CTradeLifecycleService::GetTradeCountByState(TradeLifecycleState state) const
{
    size_t nCount = 0;
    for (std::map<std::string, STradeLifecycleData>::const_iterator it = 
         m_mapTradeStore.begin(); it != m_mapTradeStore.end(); ++it) {
        if (it->second.m_eCurrentState == state) {
            nCount++;
        }
    }
    return nCount;
}

std::string CTradeLifecycleService::GenerateTradeId()
{
    std::stringstream ss;
    ss << "TRD_" << std::time(NULL) << "_" << m_nNextTradeId;
    return ss.str();
}

long CTradeLifecycleService::GenerateNumericTradeId()
{
    return m_nNextTradeId++;
}

bool CTradeLifecycleService::ValidateTradeInternal(STradeLifecycleData& refData)
{
    if (refData.m_strCounterpartyCode.empty()) {
        refData.m_strValidationError = "Counterparty code is empty";
        return false;
    }
    
    if (refData.m_dAmount <= 0) {
        refData.m_strValidationError = "Amount must be positive";
        return false;
    }
    
    if (refData.m_nProductType <= 0) {
        refData.m_strValidationError = "Invalid product type";
        return false;
    }
    
    if (m_pCreditCheckService != NULL) {
        ServiceResult<bool> creditResult = 
            m_pCreditCheckService->CheckCreditLimit(refData.m_strCounterpartyCode, refData.m_dAmount);
        
        if (creditResult.IsFailure()) {
            refData.m_strValidationError = "Credit check failed: " + creditResult.GetErrorMessage();
            return false;
        }
    }
    
    return true;
}

bool CTradeLifecycleService::ReserveCreditForTrade(STradeLifecycleData& refData)
{
    if (m_pCreditCheckService == NULL) {
        return true;
    }
    
    if (refData.m_bCreditReserved) {
        return true;
    }
    
    ServiceResult<std::string> reserveResult = 
        m_pCreditCheckService->ReserveCredit(
            refData.m_strCounterpartyCode,
            refData.m_dAmount,
            refData.m_strTradeId);
    
    if (reserveResult.IsSuccess()) {
        refData.m_bCreditReserved = true;
        refData.m_strCreditReservationId = reserveResult.GetValue();
        return true;
    }
    
    return false;
}

bool CTradeLifecycleService::ReleaseCreditForTrade(STradeLifecycleData& refData)
{
    if (m_pCreditCheckService == NULL) {
        return true;
    }
    
    if (!refData.m_bCreditReserved) {
        return true;
    }
    
    // BUG: passes reservation ID string instead of trade ID string —
    // ReleaseCreditReservation(const string&) looks up by trade ID in m_mapCreditReservations,
    // so this lookup always fails and the counterparty reserved total is never decremented
    ServiceResult<bool> releaseResult =
        m_pCreditCheckService->ReleaseCreditReservation(refData.m_strCreditReservationId);
    
    if (releaseResult.IsSuccess()) {
        refData.m_bCreditReserved = false;
        refData.m_strCreditReservationId = "";
        return true;
    }
    
    return false;
}

void CTradeLifecycleService::UpdateTradeState(STradeLifecycleData& refData, TradeLifecycleState newState)
{
    refData.m_eCurrentState = newState;
    refData.m_nLastUpdatedTime = static_cast<long>(std::time(NULL));
}

bool CTradeLifecycleService::DispatchStateChangeEvent(
    const STradeLifecycleData& refData, 
    TradeLifecycleState oldState)
{
    if (m_pEventDispatcher == NULL) {
        return false;
    }
    
    STradeEvent event;
    event.m_nEventId = refData.m_nTradeId;
    event.m_nTradeId = refData.m_nTradeId;
    event.m_nTimestamp = static_cast<long>(std::time(NULL));
    
    std::stringstream ss;
    ss << "TRADE_STATE_CHANGE:" << GetLifecycleStateName(oldState) 
       << "->" << GetLifecycleStateName(refData.m_eCurrentState);
    strcpy_s(event.m_szEventName, sizeof(event.m_szEventName), ss.str().c_str());
    
    event.m_pEventData = NULL;
    
    return m_pEventDispatcher->DispatchEvent(event);
}

bool CTradeLifecycleService::GetTradeFromStore(const std::string& strTradeId, STradeLifecycleData& refData)
{
    std::map<std::string, STradeLifecycleData>::iterator it = m_mapTradeStore.find(strTradeId);
    if (it != m_mapTradeStore.end()) {
        refData = it->second;
        return true;
    }
    return false;
}

void CTradeLifecycleService::StoreTradeData(const STradeLifecycleData& refData)
{
    m_mapTradeStore[refData.m_strTradeId] = refData;
    m_mapNumericIdToTradeId[refData.m_nTradeId] = refData.m_strTradeId;
}
