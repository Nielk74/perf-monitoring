#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <memory>
#include "ServiceCommon.h"

class CDBConnection;
class CCounterpartyService;
class CCreditCheckService;
class CTradeLifecycleService;
class CSettlementService;
class CNotificationService;
class CTradeServiceFacade;
class CTradeEventDispatcher;

class CTradeServiceFactory
{
public:
    static CTradeServiceFactory* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(CDBConnection* pDbConnection);
    bool Initialize(CDBConnection* pDbConnection, CTradeEventDispatcher* pEventDispatcher);
    bool Shutdown();
    
    CCounterpartyService* GetCounterpartyService();
    CCreditCheckService* GetCreditCheckService();
    CTradeLifecycleService* GetTradeLifecycleService();
    CSettlementService* GetSettlementService();
    CNotificationService* GetNotificationService();
    CTradeServiceFacade* GetTradeServiceFacade();
    CTradeEventDispatcher* GetEventDispatcher();
    
    bool IsInitialized() const { return m_bInitialized; }
    
    bool AreAllServicesAvailable() const;
    bool ValidateServiceDependencies() const;
    
    void SetDbConnection(CDBConnection* pDbConnection);
    CDBConnection* GetDbConnection() { return m_pDbConnection; }
    
    void SetEventDispatcher(CTradeEventDispatcher* pEventDispatcher);
    
    void ResetAllServices();
    void RefreshAllServices();

private:
    CTradeServiceFactory();
    CTradeServiceFactory(const CTradeServiceFactory& refOther);
    CTradeServiceFactory& operator=(const CTradeServiceFactory& refOther);
    ~CTradeServiceFactory();
    
    bool CreateServices();
    bool InitializeServices();
    bool WireServiceDependencies();
    bool ShutdownServices();
    void CleanupServices();
    
    CDBConnection* m_pDbConnection;
    CTradeEventDispatcher* m_pEventDispatcher;
    CCounterpartyService* m_pCounterpartyService;
    CCreditCheckService* m_pCreditCheckService;
    CTradeLifecycleService* m_pTradeLifecycleService;
    CSettlementService* m_pSettlementService;
    CNotificationService* m_pNotificationService;
    CTradeServiceFacade* m_pFacade;
    bool m_bInitialized;
    bool m_bOwnsEventDispatcher;
    
    static CTradeServiceFactory* s_pInstance;
};

#define GET_COUNTERPARTY_SERVICE() CTradeServiceFactory::GetInstance()->GetCounterpartyService()
#define GET_CREDIT_CHECK_SERVICE() CTradeServiceFactory::GetInstance()->GetCreditCheckService()
#define GET_TRADE_LIFECYCLE_SERVICE() CTradeServiceFactory::GetInstance()->GetTradeLifecycleService()
#define GET_SETTLEMENT_SERVICE() CTradeServiceFactory::GetInstance()->GetSettlementService()
#define GET_NOTIFICATION_SERVICE() CTradeServiceFactory::GetInstance()->GetNotificationService()
#define GET_TRADE_SERVICE_FACADE() CTradeServiceFactory::GetInstance()->GetTradeServiceFacade()
