#include "services/CSettlementService.h"
#include "services/CTradeLifecycleService.h"
#include "legacy/CTradeEventDispatcher.h"
#include "util/CLogger.h"
#include <ctime>
#include <sstream>

CSettlementService* CSettlementService::s_pInstance = NULL;
CSettlementService* g_pSettlementService = NULL;

CSettlementService::CSettlementService()
    : m_pTradeLifecycleService(NULL)
    , m_pEventDispatcher(NULL)
    , m_bInitialized(false)
    , m_nNextSettlementId(1)
    , m_nTotalSettlementsProcessed(0)
    , m_nTotalSettlementsFailed(0)
{
}

CSettlementService::CSettlementService(const CSettlementService& refOther)
{
}

CSettlementService& CSettlementService::operator=(const CSettlementService& refOther)
{
    return *this;
}

CSettlementService::~CSettlementService()
{
    Shutdown();
}

CSettlementService* CSettlementService::GetInstance()
{
    if (s_pInstance == NULL) {
        s_pInstance = new CSettlementService();
        g_pSettlementService = s_pInstance;
    }
    return s_pInstance;
}

void CSettlementService::DestroyInstance()
{
    if (s_pInstance != NULL) {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pSettlementService = NULL;
    }
}

bool CSettlementService::Initialize(
    CTradeLifecycleService* pTradeLifecycleService,
    CTradeEventDispatcher* pEventDispatcher)
{
    if (m_bInitialized) {
        LOG_WARNING("CSettlementService already initialized");
        return true;
    }
    
    if (pTradeLifecycleService == NULL) {
        LOG_ERROR("CSettlementService: NULL trade lifecycle service");
        return false;
    }
    
    m_pTradeLifecycleService = pTradeLifecycleService;
    m_pEventDispatcher = pEventDispatcher;
    m_bInitialized = true;
    
    LOG_INFO("CSettlementService initialized successfully");
    return true;
}

bool CSettlementService::Shutdown()
{
    if (!m_bInitialized) {
        return true;
    }
    
    m_mapSettlementStore.clear();
    m_mapSettlementIdToTradeId.clear();
    m_pTradeLifecycleService = NULL;
    m_pEventDispatcher = NULL;
    m_bInitialized = false;
    
    LOG_INFO("CSettlementService shutdown complete");
    return true;
}

ServiceResult<double> CSettlementService::CalculateSettlementAmount(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<double>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(strTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<double>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    const STradeLifecycleData& tradeData = tradeResult.GetValue();
    
    if (tradeData.m_eCurrentState != TradeLifecycleState::Executed) {
        return ServiceResult<double>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Trade must be EXECUTED for settlement calculation");
    }
    
    double dSettlementAmount = tradeData.m_dAmount;
    
    if (tradeData.m_nProductType == PRODUCTTYPE_DERIVATIVE ||
        tradeData.m_nProductType == PRODUCTTYPE_SWAP) {
        dSettlementAmount *= 0.98;
    }
    
    std::stringstream ss;
    ss << "Calculated settlement amount for trade " << strTradeId 
       << ": " << dSettlementAmount;
    LOG_DEBUG(ss.str().c_str());
    
    return ServiceResult<double>::Success(dSettlementAmount);
}

ServiceResult<double> CSettlementService::CalculateSettlementAmount(long nTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<double>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(nTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<double>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    return CalculateSettlementAmount(tradeResult.GetValue().m_strTradeId);
}

ServiceResult<SSettlementRecord> CSettlementService::ScheduleSettlement(
    const std::string& strTradeId,
    const std::string& strSettlementDate)
{
    if (!m_bInitialized) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    if (!ValidateTradeForSettlement(strTradeId)) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Trade is not valid for settlement");
    }
    
    ServiceResult<double> amountResult = CalculateSettlementAmount(strTradeId);
    if (amountResult.IsFailure()) {
        return ServiceResult<SSettlementRecord>::Failure(
            amountResult.GetErrorCode(),
            amountResult.GetErrorMessage());
    }
    
    SSettlementRecord record;
    record.m_nSettlementId = GenerateNumericSettlementId();
    record.m_strTradeId = strTradeId;
    record.m_dSettlementAmount = amountResult.GetValue();
    record.m_dNetAmount = record.m_dSettlementAmount;
    record.m_strSettlementDate = strSettlementDate;
    record.m_strSettlementCurrency = "USD";
    record.m_nSettlementStatus = SETTLEMENT_STATUS_SCHEDULED;
    record.m_nCreatedTime = static_cast<long>(std::time(NULL));
    record.m_strSettlementReference = GenerateSettlementId();
    
    CalculateNetAmount(record);
    
    StoreSettlementData(record);
    
    std::stringstream ss;
    ss << "Settlement scheduled for trade " << strTradeId 
       << " on " << strSettlementDate 
       << " amount: " << record.m_dNetAmount;
    LOG_INFO(ss.str().c_str());
    
    DispatchSettlementEvent(record, 1);
    
    return ServiceResult<SSettlementRecord>::Success(record);
}

ServiceResult<SSettlementRecord> CSettlementService::ScheduleSettlement(
    long nTradeId,
    const std::string& strSettlementDate)
{
    if (!m_bInitialized || m_pTradeLifecycleService == NULL) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(nTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<SSettlementRecord>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    return ScheduleSettlement(tradeResult.GetValue().m_strTradeId, strSettlementDate);
}

ServiceResult<SSettlementRecord> CSettlementService::ProcessSettlement(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    SSettlementRecord record;
    if (!GetSettlementFromStore(strTradeId, record)) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            "No settlement found for trade: " + strTradeId);
    }
    
    if (record.m_nSettlementStatus != SETTLEMENT_STATUS_SCHEDULED &&
        record.m_nSettlementStatus != SETTLEMENT_STATUS_PENDING) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Settlement is not in scheduled or pending state");
    }
    
    UpdateSettlementStatus(record, SETTLEMENT_STATUS_PROCESSING);
    StoreSettlementData(record);
    
    bool bSuccess = true;
    
    if (bSuccess) {
        UpdateSettlementStatus(record, SETTLEMENT_STATUS_COMPLETED);
        record.m_nProcessedTime = static_cast<long>(std::time(NULL));
        m_nTotalSettlementsProcessed++;
        
        std::stringstream ss;
        ss << "Settlement processed for trade " << strTradeId 
           << " amount: " << record.m_dNetAmount;
        LOG_INFO(ss.str().c_str());
        
        DispatchSettlementEvent(record, 3);
    } else {
        UpdateSettlementStatus(record, SETTLEMENT_STATUS_FAILED);
        m_nTotalSettlementsFailed++;
        
        std::stringstream ss;
        ss << "Settlement failed for trade " << strTradeId;
        LOG_ERROR(ss.str().c_str());
        
        DispatchSettlementEvent(record, 4);
        
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SETTLEMENT_FAILED,
            "Settlement processing failed");
    }
    
    StoreSettlementData(record);
    
    return ServiceResult<SSettlementRecord>::Success(record);
}

ServiceResult<SSettlementRecord> CSettlementService::ProcessSettlement(long nTradeId)
{
    if (!m_bInitialized || m_pTradeLifecycleService == NULL) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(nTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<SSettlementRecord>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    return ProcessSettlement(tradeResult.GetValue().m_strTradeId);
}

ServiceResult<SSettlementRecord> CSettlementService::CancelSettlement(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    SSettlementRecord record;
    if (!GetSettlementFromStore(strTradeId, record)) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            "No settlement found for trade: " + strTradeId);
    }
    
    if (record.m_nSettlementStatus == SETTLEMENT_STATUS_COMPLETED) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_INVALID_TRADE_STATE,
            "Cannot cancel completed settlement");
    }
    
    UpdateSettlementStatus(record, SETTLEMENT_STATUS_CANCELLED);
    StoreSettlementData(record);
    
    std::stringstream ss;
    ss << "Settlement cancelled for trade " << strTradeId;
    LOG_WARNING(ss.str().c_str());
    
    DispatchSettlementEvent(record, 5);
    
    return ServiceResult<SSettlementRecord>::Success(record);
}

ServiceResult<SSettlementRecord> CSettlementService::CancelSettlement(long nTradeId)
{
    if (!m_bInitialized || m_pTradeLifecycleService == NULL) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(nTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<SSettlementRecord>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    return CancelSettlement(tradeResult.GetValue().m_strTradeId);
}

ServiceResult<int> CSettlementService::GetSettlementStatus(const std::string& strTradeId)
{
    SSettlementRecord record;
    if (!GetSettlementFromStore(strTradeId, record)) {
        return ServiceResult<int>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            "Settlement not found for trade: " + strTradeId);
    }
    
    return ServiceResult<int>::Success(record.m_nSettlementStatus);
}

ServiceResult<int> CSettlementService::GetSettlementStatus(long nTradeId)
{
    if (!m_bInitialized || m_pTradeLifecycleService == NULL) {
        return ServiceResult<int>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(nTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<int>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    return GetSettlementStatus(tradeResult.GetValue().m_strTradeId);
}

ServiceResult<SSettlementRecord> CSettlementService::GetSettlementDetails(const std::string& strTradeId)
{
    SSettlementRecord record;
    if (!GetSettlementFromStore(strTradeId, record)) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            "Settlement not found for trade: " + strTradeId);
    }
    
    return ServiceResult<SSettlementRecord>::Success(record);
}

ServiceResult<SSettlementRecord> CSettlementService::GetSettlementDetails(long nTradeId)
{
    if (!m_bInitialized || m_pTradeLifecycleService == NULL) {
        return ServiceResult<SSettlementRecord>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(nTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<SSettlementRecord>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    return GetSettlementDetails(tradeResult.GetValue().m_strTradeId);
}

ServiceResult<bool> CSettlementService::IsTradeSettleable(const std::string& strTradeId)
{
    if (!m_bInitialized || m_pTradeLifecycleService == NULL) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<TradeLifecycleState> stateResult = 
        m_pTradeLifecycleService->GetTradeState(strTradeId);
    
    if (stateResult.IsFailure()) {
        return ServiceResult<bool>::Failure(
            stateResult.GetErrorCode(),
            stateResult.GetErrorMessage());
    }
    
    bool bSettleable = (stateResult.GetValue() == TradeLifecycleState::Executed);
    
    return ServiceResult<bool>::Success(bSettleable);
}

ServiceResult<bool> CSettlementService::IsTradeSettleable(long nTradeId)
{
    if (!m_bInitialized || m_pTradeLifecycleService == NULL) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "SettlementService not initialized");
    }
    
    ServiceResult<STradeLifecycleData> tradeResult = 
        m_pTradeLifecycleService->GetTradeData(nTradeId);
    
    if (tradeResult.IsFailure()) {
        return ServiceResult<bool>::Failure(
            tradeResult.GetErrorCode(),
            tradeResult.GetErrorMessage());
    }
    
    return IsTradeSettleable(tradeResult.GetValue().m_strTradeId);
}

size_t CSettlementService::GetPendingSettlementCount() const
{
    return GetSettlementCountByStatus(SETTLEMENT_STATUS_PENDING) +
           GetSettlementCountByStatus(SETTLEMENT_STATUS_SCHEDULED);
}

size_t CSettlementService::GetSettlementCountByStatus(int nStatus) const
{
    size_t nCount = 0;
    for (std::map<std::string, SSettlementRecord>::const_iterator it = 
         m_mapSettlementStore.begin(); it != m_mapSettlementStore.end(); ++it) {
        if (it->second.m_nSettlementStatus == nStatus) {
            nCount++;
        }
    }
    return nCount;
}

void CSettlementService::ProcessScheduledSettlements()
{
    std::vector<std::string> vecToProcess;
    
    for (std::map<std::string, SSettlementRecord>::iterator it = 
         m_mapSettlementStore.begin(); it != m_mapSettlementStore.end(); ++it) {
        if (it->second.m_nSettlementStatus == SETTLEMENT_STATUS_SCHEDULED) {
            vecToProcess.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < vecToProcess.size(); i++) {
        ProcessSettlement(vecToProcess[i]);
    }
    
    std::stringstream ss;
    ss << "Processed " << vecToProcess.size() << " scheduled settlements";
    LOG_INFO(ss.str().c_str());
}

void CSettlementService::ClearProcessedSettlements()
{
    std::vector<std::string> vecToRemove;
    
    for (std::map<std::string, SSettlementRecord>::iterator it = 
         m_mapSettlementStore.begin(); it != m_mapSettlementStore.end(); ++it) {
        if (it->second.m_nSettlementStatus == SETTLEMENT_STATUS_COMPLETED ||
            it->second.m_nSettlementStatus == SETTLEMENT_STATUS_CANCELLED) {
            vecToRemove.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < vecToRemove.size(); i++) {
        m_mapSettlementStore.erase(vecToRemove[i]);
    }
    
    std::stringstream ss;
    ss << "Cleared " << vecToRemove.size() << " processed settlements";
    LOG_DEBUG(ss.str().c_str());
}

std::string CSettlementService::GenerateSettlementId()
{
    std::stringstream ss;
    ss << "SET_" << std::time(NULL) << "_" << m_nNextSettlementId;
    return ss.str();
}

long CSettlementService::GenerateNumericSettlementId()
{
    return m_nNextSettlementId++;
}

bool CSettlementService::ValidateTradeForSettlement(const std::string& strTradeId)
{
    if (m_pTradeLifecycleService == NULL) {
        return false;
    }
    
    ServiceResult<bool> settleableResult = IsTradeSettleable(strTradeId);
    return settleableResult.IsSuccess() && settleableResult.GetValue();
}

bool CSettlementService::CalculateNetAmount(SSettlementRecord& refRecord)
{
    refRecord.m_dNetAmount = refRecord.m_dSettlementAmount;
    
    return true;
}

bool CSettlementService::UpdateSettlementStatus(SSettlementRecord& refRecord, int nNewStatus)
{
    refRecord.m_nSettlementStatus = nNewStatus;
    return true;
}

bool CSettlementService::GetSettlementFromStore(const std::string& strTradeId, SSettlementRecord& refRecord)
{
    std::map<std::string, SSettlementRecord>::iterator it = 
        m_mapSettlementStore.find(strTradeId);
    
    if (it != m_mapSettlementStore.end()) {
        refRecord = it->second;
        return true;
    }
    
    return false;
}

void CSettlementService::StoreSettlementData(const SSettlementRecord& refRecord)
{
    m_mapSettlementStore[refRecord.m_strTradeId] = refRecord;
    m_mapSettlementIdToTradeId[refRecord.m_nSettlementId] = refRecord.m_strTradeId;
}

bool CSettlementService::DispatchSettlementEvent(const SSettlementRecord& refRecord, int nEventType)
{
    if (m_pEventDispatcher == NULL) {
        return false;
    }
    
    STradeEvent event;
    event.m_nEventId = refRecord.m_nSettlementId;
    event.m_nTradeId = 0;
    event.m_nTimestamp = static_cast<long>(std::time(NULL));
    event.m_nEventType = nEventType;
    
    std::stringstream ss;
    ss << "SETTLEMENT_EVENT:" << nEventType << "_" << refRecord.m_strTradeId;
    strcpy_s(event.m_szEventName, sizeof(event.m_szEventName), ss.str().c_str());
    
    event.m_pEventData = NULL;
    
    return m_pEventDispatcher->DispatchEvent(event);
}
