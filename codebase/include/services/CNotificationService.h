#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "ServiceCommon.h"

class CTradeEventDispatcher;

enum NotificationType {
    NOTIFICATION_TYPE_TRADE_CREATED = 1,
    NOTIFICATION_TYPE_TRADE_VALIDATED = 2,
    NOTIFICATION_TYPE_TRADE_SUBMITTED = 3,
    NOTIFICATION_TYPE_TRADE_APPROVED = 4,
    NOTIFICATION_TYPE_TRADE_REJECTED = 5,
    NOTIFICATION_TYPE_TRADE_EXECUTED = 6,
    NOTIFICATION_TYPE_TRADE_SETTLED = 7,
    NOTIFICATION_TYPE_TRADE_CANCELLED = 8,
    NOTIFICATION_TYPE_CREDIT_WARNING = 9,
    NOTIFICATION_TYPE_SETTLEMENT_DUE = 10
};

struct SNotificationPayload {
    NotificationType m_eType;
    std::string m_strTradeId;
    std::string m_strCounterpartyCode;
    std::string m_strMessage;
    std::string m_strDetails;
    long m_nTimestamp;
    int m_nPriority;
    bool m_bRequiresAcknowledgement;
    std::map<std::string, std::string> m_mapAdditionalData;
    
    SNotificationPayload()
        : m_eType(NOTIFICATION_TYPE_TRADE_CREATED)
        , m_nTimestamp(0)
        , m_nPriority(0)
        , m_bRequiresAcknowledgement(false)
    {}
};

typedef std::function<void(const SNotificationPayload&)> NotificationHandler;

struct SNotificationHandlerEntry {
    long m_nHandlerId;
    NotificationType m_eType;
    NotificationHandler m_handler;
    bool m_bIsActive;
    
    SNotificationHandlerEntry()
        : m_nHandlerId(0)
        , m_eType(NOTIFICATION_TYPE_TRADE_CREATED)
        , m_bIsActive(true)
    {}
};

class CNotificationService
{
public:
    static CNotificationService* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(CTradeEventDispatcher* pEventDispatcher);
    bool Shutdown();
    
    ServiceResult<bool> NotifyTradeCreated(const std::string& strTradeId,
                                            const std::string& strCptyCode);
    
    ServiceResult<bool> NotifyTradeValidated(const std::string& strTradeId,
                                              bool bIsValid,
                                              const std::string& strValidationMsg);
    
    ServiceResult<bool> NotifyTradeSubmitted(const std::string& strTradeId,
                                              const std::string& strStatus);
    
    ServiceResult<bool> NotifyTradeApproved(const std::string& strTradeId,
                                             const std::string& strApprover);
    
    ServiceResult<bool> NotifyTradeRejected(const std::string& strTradeId,
                                             const std::string& strReason);
    
    ServiceResult<bool> NotifyTradeExecuted(const std::string& strTradeId,
                                             const std::string& strExecutionDetails);
    
    ServiceResult<bool> NotifyTradeSettled(const std::string& strTradeId,
                                            const std::string& strSettlementDetails);
    
    ServiceResult<bool> NotifyTradeCancelled(const std::string& strTradeId,
                                              const std::string& strReason);
    
    ServiceResult<bool> SendNotification(const SNotificationPayload& refPayload);
    
    long RegisterNotificationHandler(NotificationType eType, NotificationHandler handler);
    bool UnregisterNotificationHandler(long nHandlerId);
    void ClearAllHandlers();
    
    void SetDefaultHandler(NotificationHandler handler);
    void SetErrorHandler(NotificationHandler handler);
    
    size_t GetHandlerCount() const;
    size_t GetHandlerCount(NotificationType eType) const;
    
    bool IsInitialized() const { return m_bInitialized; }
    
    void EnableNotifications(bool bEnable);
    bool AreNotificationsEnabled() const { return m_bNotificationsEnabled; }
    
    void SetNotificationQueueSize(size_t nSize);
    size_t GetNotificationQueueSize() const;
    
    void ProcessPendingNotifications();
    size_t GetPendingNotificationCount() const;
    
    CTradeEventDispatcher* GetEventDispatcher() { return m_pEventDispatcher; }

private:
    CNotificationService();
    CNotificationService(const CNotificationService& refOther);
    CNotificationService& operator=(const CNotificationService& refOther);
    ~CNotificationService();
    
    long GenerateNextHandlerId();
    bool DispatchToHandlers(const SNotificationPayload& refPayload);
    bool DispatchToEventDispatcher(const SNotificationPayload& refPayload);
    void QueueNotification(const SNotificationPayload& refPayload);
    SNotificationPayload CreatePayload(NotificationType eType,
                                        const std::string& strTradeId,
                                        const std::string& strMessage);
    
    std::vector<SNotificationHandlerEntry> m_vecNotificationHandlers;
    std::vector<SNotificationPayload> m_vecPendingNotifications;
    std::map<NotificationType, std::vector<long>> m_mapTypeToHandlers;
    CTradeEventDispatcher* m_pEventDispatcher;
    NotificationHandler m_defaultHandler;
    NotificationHandler m_errorHandler;
    bool m_bInitialized;
    bool m_bNotificationsEnabled;
    long m_nNextHandlerId;
    size_t m_nMaxQueueSize;
    long m_nNotificationsSent;
    long m_nNotificationsFailed;
    
    static CNotificationService* s_pInstance;
};

extern CNotificationService* g_pNotificationService;
