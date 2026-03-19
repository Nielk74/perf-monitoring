#include "services/CNotificationService.h"
#include "legacy/CTradeEventDispatcher.h"
#include "util/CLogger.h"
#include <ctime>
#include <sstream>

CNotificationService* CNotificationService::s_pInstance = NULL;
CNotificationService* g_pNotificationService = NULL;

CNotificationService::CNotificationService()
    : m_pEventDispatcher(NULL)
    , m_bInitialized(false)
    , m_bNotificationsEnabled(true)
    , m_nNextHandlerId(1)
    , m_nMaxQueueSize(1000)
    , m_nNotificationsSent(0)
    , m_nNotificationsFailed(0)
{
}

CNotificationService::CNotificationService(const CNotificationService& refOther)
{
}

CNotificationService& CNotificationService::operator=(const CNotificationService& refOther)
{
    return *this;
}

CNotificationService::~CNotificationService()
{
    Shutdown();
}

CNotificationService* CNotificationService::GetInstance()
{
    if (s_pInstance == NULL) {
        s_pInstance = new CNotificationService();
        g_pNotificationService = s_pInstance;
    }
    return s_pInstance;
}

void CNotificationService::DestroyInstance()
{
    if (s_pInstance != NULL) {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pNotificationService = NULL;
    }
}

bool CNotificationService::Initialize(CTradeEventDispatcher* pEventDispatcher)
{
    if (m_bInitialized) {
        LOG_WARNING("CNotificationService already initialized");
        return true;
    }
    
    m_pEventDispatcher = pEventDispatcher;
    m_bInitialized = true;
    
    LOG_INFO("CNotificationService initialized successfully");
    return true;
}

bool CNotificationService::Shutdown()
{
    if (!m_bInitialized) {
        return true;
    }
    
    ClearAllHandlers();
    m_vecPendingNotifications.clear();
    m_pEventDispatcher = NULL;
    m_bInitialized = false;
    
    LOG_INFO("CNotificationService shutdown complete");
    return true;
}

ServiceResult<bool> CNotificationService::NotifyTradeCreated(
    const std::string& strTradeId,
    const std::string& strCptyCode)
{
    SNotificationPayload payload = CreatePayload(
        NOTIFICATION_TYPE_TRADE_CREATED,
        strTradeId,
        "Trade created");
    payload.m_strCounterpartyCode = strCptyCode;
    payload.m_nPriority = 1;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::NotifyTradeValidated(
    const std::string& strTradeId,
    bool bIsValid,
    const std::string& strValidationMsg)
{
    NotificationType type = bIsValid ? 
        NOTIFICATION_TYPE_TRADE_SUBMITTED : 
        NOTIFICATION_TYPE_TRADE_REJECTED;
    
    SNotificationPayload payload = CreatePayload(
        type,
        strTradeId,
        bIsValid ? "Trade validated" : "Trade validation failed");
    payload.m_strDetails = strValidationMsg;
    payload.m_nPriority = bIsValid ? 2 : 4;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::NotifyTradeSubmitted(
    const std::string& strTradeId,
    const std::string& strStatus)
{
    SNotificationPayload payload = CreatePayload(
        NOTIFICATION_TYPE_TRADE_SUBMITTED,
        strTradeId,
        "Trade submitted: " + strStatus);
    payload.m_nPriority = 2;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::NotifyTradeApproved(
    const std::string& strTradeId,
    const std::string& strApprover)
{
    SNotificationPayload payload = CreatePayload(
        NOTIFICATION_TYPE_TRADE_APPROVED,
        strTradeId,
        "Trade approved by " + strApprover);
    payload.m_nPriority = 2;
    payload.m_mapAdditionalData["approver"] = strApprover;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::NotifyTradeRejected(
    const std::string& strTradeId,
    const std::string& strReason)
{
    SNotificationPayload payload = CreatePayload(
        NOTIFICATION_TYPE_TRADE_REJECTED,
        strTradeId,
        "Trade rejected: " + strReason);
    payload.m_strDetails = strReason;
    payload.m_nPriority = 4;
    payload.m_bRequiresAcknowledgement = true;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::NotifyTradeExecuted(
    const std::string& strTradeId,
    const std::string& strExecutionDetails)
{
    SNotificationPayload payload = CreatePayload(
        NOTIFICATION_TYPE_TRADE_EXECUTED,
        strTradeId,
        "Trade executed");
    payload.m_strDetails = strExecutionDetails;
    payload.m_nPriority = 1;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::NotifyTradeSettled(
    const std::string& strTradeId,
    const std::string& strSettlementDetails)
{
    SNotificationPayload payload = CreatePayload(
        NOTIFICATION_TYPE_TRADE_SETTLED,
        strTradeId,
        "Trade settled");
    payload.m_strDetails = strSettlementDetails;
    payload.m_nPriority = 1;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::NotifyTradeCancelled(
    const std::string& strTradeId,
    const std::string& strReason)
{
    SNotificationPayload payload = CreatePayload(
        NOTIFICATION_TYPE_TRADE_CANCELLED,
        strTradeId,
        "Trade cancelled: " + strReason);
    payload.m_strDetails = strReason;
    payload.m_nPriority = 3;
    
    return SendNotification(payload);
}

ServiceResult<bool> CNotificationService::SendNotification(const SNotificationPayload& refPayload)
{
    if (!m_bInitialized) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "NotificationService not initialized");
    }
    
    if (!m_bNotificationsEnabled) {
        LOG_DEBUG("Notifications disabled, skipping notification");
        return ServiceResult<bool>::Success(true);
    }
    
    std::stringstream ss;
    ss << "Sending notification type " << static_cast<int>(refPayload.m_eType)
       << " for trade " << refPayload.m_strTradeId;
    LOG_DEBUG(ss.str().c_str());
    
    bool bDispatched = DispatchToHandlers(refPayload);
    
    if (m_pEventDispatcher != NULL) {
        DispatchToEventDispatcher(refPayload);
    }
    
    if (bDispatched) {
        m_nNotificationsSent++;
        
        ss.str("");
        ss << "Notification sent successfully for trade " << refPayload.m_strTradeId;
        LOG_DEBUG(ss.str().c_str());
        
        return ServiceResult<bool>::Success(true);
    } else {
        m_nNotificationsFailed++;
        
        if (m_vecPendingNotifications.size() < m_nMaxQueueSize) {
            QueueNotification(refPayload);
        }
        
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_NOTIFICATION_FAILED,
            "No handlers registered, notification queued");
    }
}

long CNotificationService::RegisterNotificationHandler(
    NotificationType eType, 
    NotificationHandler handler)
{
    if (!handler) {
        LOG_WARNING("Attempted to register null notification handler");
        return 0;
    }
    
    SNotificationHandlerEntry entry;
    entry.m_nHandlerId = GenerateNextHandlerId();
    entry.m_eType = eType;
    entry.m_handler = handler;
    entry.m_bIsActive = true;
    
    m_vecNotificationHandlers.push_back(entry);
    m_mapTypeToHandlers[eType].push_back(entry.m_nHandlerId);
    
    std::stringstream ss;
    ss << "Registered notification handler " << entry.m_nHandlerId 
       << " for type " << static_cast<int>(eType);
    LOG_DEBUG(ss.str().c_str());
    
    return entry.m_nHandlerId;
}

bool CNotificationService::UnregisterNotificationHandler(long nHandlerId)
{
    for (std::vector<SNotificationHandlerEntry>::iterator it = 
         m_vecNotificationHandlers.begin(); it != m_vecNotificationHandlers.end(); ++it) {
        if (it->m_nHandlerId == nHandlerId) {
            NotificationType eType = it->m_eType;
            
            std::vector<long>& vecTypeHandlers = m_mapTypeToHandlers[eType];
            for (std::vector<long>::iterator typeIt = vecTypeHandlers.begin();
                 typeIt != vecTypeHandlers.end(); ++typeIt) {
                if (*typeIt == nHandlerId) {
                    vecTypeHandlers.erase(typeIt);
                    break;
                }
            }
            
            m_vecNotificationHandlers.erase(it);
            
            std::stringstream ss;
            ss << "Unregistered notification handler " << nHandlerId;
            LOG_DEBUG(ss.str().c_str());
            
            return true;
        }
    }
    
    return false;
}

void CNotificationService::ClearAllHandlers()
{
    m_vecNotificationHandlers.clear();
    m_mapTypeToHandlers.clear();
    m_defaultHandler = nullptr;
    m_errorHandler = nullptr;
    
    LOG_DEBUG("All notification handlers cleared");
}

void CNotificationService::SetDefaultHandler(NotificationHandler handler)
{
    m_defaultHandler = handler;
}

void CNotificationService::SetErrorHandler(NotificationHandler handler)
{
    m_errorHandler = handler;
}

size_t CNotificationService::GetHandlerCount() const
{
    return m_vecNotificationHandlers.size();
}

size_t CNotificationService::GetHandlerCount(NotificationType eType) const
{
    std::map<NotificationType, std::vector<long>>::const_iterator it = 
        m_mapTypeToHandlers.find(eType);
    
    if (it != m_mapTypeToHandlers.end()) {
        return it->second.size();
    }
    
    return 0;
}

void CNotificationService::EnableNotifications(bool bEnable)
{
    m_bNotificationsEnabled = bEnable;
    
    std::stringstream ss;
    ss << "Notifications " << (bEnable ? "enabled" : "disabled");
    LOG_INFO(ss.str().c_str());
}

void CNotificationService::SetNotificationQueueSize(size_t nSize)
{
    m_nMaxQueueSize = nSize;
}

size_t CNotificationService::GetNotificationQueueSize() const
{
    return m_nMaxQueueSize;
}

void CNotificationService::ProcessPendingNotifications()
{
    if (m_vecPendingNotifications.empty()) {
        return;
    }
    
    std::vector<SNotificationPayload> vecToProcess = m_vecPendingNotifications;
    m_vecPendingNotifications.clear();
    
    for (size_t i = 0; i < vecToProcess.size(); i++) {
        DispatchToHandlers(vecToProcess[i]);
    }
    
    std::stringstream ss;
    ss << "Processed " << vecToProcess.size() << " pending notifications";
    LOG_DEBUG(ss.str().c_str());
}

size_t CNotificationService::GetPendingNotificationCount() const
{
    return m_vecPendingNotifications.size();
}

long CNotificationService::GenerateNextHandlerId()
{
    return m_nNextHandlerId++;
}

bool CNotificationService::DispatchToHandlers(const SNotificationPayload& refPayload)
{
    bool bDispatched = false;
    
    std::map<NotificationType, std::vector<long>>::iterator typeIt = 
        m_mapTypeToHandlers.find(refPayload.m_eType);
    
    if (typeIt != m_mapTypeToHandlers.end()) {
        const std::vector<long>& vecHandlerIds = typeIt->second;
        
        for (size_t i = 0; i < vecHandlerIds.size(); i++) {
            for (std::vector<SNotificationHandlerEntry>::iterator it = 
                 m_vecNotificationHandlers.begin(); it != m_vecNotificationHandlers.end(); ++it) {
                if (it->m_nHandlerId == vecHandlerIds[i] && it->m_bIsActive) {
                    try {
                        it->m_handler(refPayload);
                        bDispatched = true;
                    } catch (...) {
                        std::stringstream ss;
                        ss << "Exception in notification handler " << it->m_nHandlerId;
                        LOG_ERROR(ss.str().c_str());
                    }
                    break;
                }
            }
        }
    }
    
    if (!bDispatched && m_defaultHandler) {
        try {
            m_defaultHandler(refPayload);
            bDispatched = true;
        } catch (...) {
            LOG_ERROR("Exception in default notification handler");
        }
    }
    
    return bDispatched;
}

bool CNotificationService::DispatchToEventDispatcher(const SNotificationPayload& refPayload)
{
    if (m_pEventDispatcher == NULL) {
        return false;
    }
    
    STradeEvent event;
    event.m_nEventId = static_cast<long>(refPayload.m_eType);
    event.m_nEventType = static_cast<int>(refPayload.m_eType);
    event.m_nTimestamp = refPayload.m_nTimestamp;
    
    std::stringstream ss;
    ss << "NOTIFICATION_" << static_cast<int>(refPayload.m_eType);
    strcpy_s(event.m_szEventName, sizeof(event.m_szEventName), ss.str().c_str());
    
    event.m_pEventData = NULL;
    
    return m_pEventDispatcher->DispatchEvent(event);
}

void CNotificationService::QueueNotification(const SNotificationPayload& refPayload)
{
    if (m_vecPendingNotifications.size() < m_nMaxQueueSize) {
        m_vecPendingNotifications.push_back(refPayload);
    }
}

SNotificationPayload CNotificationService::CreatePayload(
    NotificationType eType,
    const std::string& strTradeId,
    const std::string& strMessage)
{
    SNotificationPayload payload;
    payload.m_eType = eType;
    payload.m_strTradeId = strTradeId;
    payload.m_strMessage = strMessage;
    payload.m_nTimestamp = static_cast<long>(std::time(NULL));
    payload.m_nPriority = 0;
    payload.m_bRequiresAcknowledgement = false;
    
    return payload;
}
