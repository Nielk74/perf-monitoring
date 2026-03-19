#include "services/CTradeServiceFactory.h"
#include "services/CCounterpartyService.h"
#include "services/CCreditCheckService.h"
#include "services/CTradeLifecycleService.h"
#include "services/CSettlementService.h"
#include "services/CNotificationService.h"
#include "services/CTradeServiceFacade.h"
#include "db/CDBConnection.h"
#include "legacy/CTradeEventDispatcher.h"
#include "util/CLogger.h"

CTradeServiceFactory* CTradeServiceFactory::s_pInstance = NULL;

CTradeServiceFactory::CTradeServiceFactory()
    : m_pDbConnection(NULL)
    , m_pEventDispatcher(NULL)
    , m_pCounterpartyService(NULL)
    , m_pCreditCheckService(NULL)
    , m_pTradeLifecycleService(NULL)
    , m_pSettlementService(NULL)
    , m_pNotificationService(NULL)
    , m_pFacade(NULL)
    , m_bInitialized(false)
    , m_bOwnsEventDispatcher(false)
{
}

CTradeServiceFactory::CTradeServiceFactory(const CTradeServiceFactory& refOther)
{
}

CTradeServiceFactory& CTradeServiceFactory::operator=(const CTradeServiceFactory& refOther)
{
    return *this;
}

CTradeServiceFactory::~CTradeServiceFactory()
{
    Shutdown();
}

CTradeServiceFactory* CTradeServiceFactory::GetInstance()
{
    if (s_pInstance == NULL) {
        s_pInstance = new CTradeServiceFactory();
    }
    return s_pInstance;
}

void CTradeServiceFactory::DestroyInstance()
{
    if (s_pInstance != NULL) {
        delete s_pInstance;
        s_pInstance = NULL;
    }
}

bool CTradeServiceFactory::Initialize(CDBConnection* pDbConnection)
{
    return Initialize(pDbConnection, NULL);
}

bool CTradeServiceFactory::Initialize(
    CDBConnection* pDbConnection, 
    CTradeEventDispatcher* pEventDispatcher)
{
    if (m_bInitialized) {
        LOG_WARNING("CTradeServiceFactory already initialized");
        return true;
    }
    
    if (pDbConnection == NULL) {
        LOG_ERROR("CTradeServiceFactory: NULL database connection");
        return false;
    }
    
    m_pDbConnection = pDbConnection;
    m_pEventDispatcher = pEventDispatcher;
    
    if (m_pEventDispatcher == NULL) {
        m_pEventDispatcher = CTradeEventDispatcher::GetInstance();
        m_bOwnsEventDispatcher = true;
    }
    
    if (!CreateServices()) {
        LOG_ERROR("CTradeServiceFactory: Failed to create services");
        CleanupServices();
        return false;
    }
    
    if (!InitializeServices()) {
        LOG_ERROR("CTradeServiceFactory: Failed to initialize services");
        CleanupServices();
        return false;
    }
    
    if (!WireServiceDependencies()) {
        LOG_ERROR("CTradeServiceFactory: Failed to wire service dependencies");
        CleanupServices();
        return false;
    }
    
    m_bInitialized = true;
    
    LOG_INFO("CTradeServiceFactory initialized successfully");
    return true;
}

bool CTradeServiceFactory::Shutdown()
{
    if (!m_bInitialized) {
        return true;
    }
    
    ShutdownServices();
    CleanupServices();
    
    if (m_bOwnsEventDispatcher && m_pEventDispatcher != NULL) {
        CTradeEventDispatcher::DestroyInstance();
        m_pEventDispatcher = NULL;
        m_bOwnsEventDispatcher = false;
    }
    
    m_pDbConnection = NULL;
    m_bInitialized = false;
    
    LOG_INFO("CTradeServiceFactory shutdown complete");
    return true;
}

CCounterpartyService* CTradeServiceFactory::GetCounterpartyService()
{
    return m_pCounterpartyService;
}

CCreditCheckService* CTradeServiceFactory::GetCreditCheckService()
{
    return m_pCreditCheckService;
}

CTradeLifecycleService* CTradeServiceFactory::GetTradeLifecycleService()
{
    return m_pTradeLifecycleService;
}

CSettlementService* CTradeServiceFactory::GetSettlementService()
{
    return m_pSettlementService;
}

CNotificationService* CTradeServiceFactory::GetNotificationService()
{
    return m_pNotificationService;
}

CTradeServiceFacade* CTradeServiceFactory::GetTradeServiceFacade()
{
    return m_pFacade;
}

CTradeEventDispatcher* CTradeServiceFactory::GetEventDispatcher()
{
    return m_pEventDispatcher;
}

bool CTradeServiceFactory::AreAllServicesAvailable() const
{
    return m_pCounterpartyService != NULL &&
           m_pCreditCheckService != NULL &&
           m_pTradeLifecycleService != NULL &&
           m_pSettlementService != NULL &&
           m_pNotificationService != NULL &&
           m_pFacade != NULL;
}

bool CTradeServiceFactory::ValidateServiceDependencies() const
{
    if (m_pCounterpartyService == NULL) {
        LOG_ERROR("CounterpartyService is NULL");
        return false;
    }
    
    if (m_pCreditCheckService == NULL) {
        LOG_ERROR("CreditCheckService is NULL");
        return false;
    }
    
    if (m_pCreditCheckService->GetCounterpartyService() != m_pCounterpartyService) {
        LOG_ERROR("CreditCheckService not properly wired to CounterpartyService");
        return false;
    }
    
    if (m_pTradeLifecycleService == NULL) {
        LOG_ERROR("TradeLifecycleService is NULL");
        return false;
    }
    
    if (m_pTradeLifecycleService->GetCreditCheckService() != m_pCreditCheckService) {
        LOG_ERROR("TradeLifecycleService not properly wired to CreditCheckService");
        return false;
    }
    
    if (m_pSettlementService == NULL) {
        LOG_ERROR("SettlementService is NULL");
        return false;
    }
    
    if (m_pSettlementService->GetTradeLifecycleService() != m_pTradeLifecycleService) {
        LOG_ERROR("SettlementService not properly wired to TradeLifecycleService");
        return false;
    }
    
    if (m_pFacade == NULL) {
        LOG_ERROR("TradeServiceFacade is NULL");
        return false;
    }
    
    return true;
}

void CTradeServiceFactory::SetDbConnection(CDBConnection* pDbConnection)
{
    m_pDbConnection = pDbConnection;
}

void CTradeServiceFactory::SetEventDispatcher(CTradeEventDispatcher* pEventDispatcher)
{
    m_pEventDispatcher = pEventDispatcher;
}

void CTradeServiceFactory::ResetAllServices()
{
    if (!m_bInitialized) {
        return;
    }
    
    ShutdownServices();
    
    if (m_pCounterpartyService != NULL) {
        m_pCounterpartyService->ClearCache();
    }
    
    if (m_pCreditCheckService != NULL) {
        m_pCreditCheckService->ClearAllReservations();
    }
    
    LOG_INFO("All services reset");
}

void CTradeServiceFactory::RefreshAllServices()
{
    if (!m_bInitialized) {
        return;
    }
    
    if (m_pCounterpartyService != NULL) {
        m_pCounterpartyService->RefreshCache();
    }
    
    LOG_INFO("All services refreshed");
}

bool CTradeServiceFactory::CreateServices()
{
    m_pCounterpartyService = CCounterpartyService::GetInstance();
    if (m_pCounterpartyService == NULL) {
        LOG_ERROR("Failed to create CounterpartyService");
        return false;
    }
    
    m_pCreditCheckService = CCreditCheckService::GetInstance();
    if (m_pCreditCheckService == NULL) {
        LOG_ERROR("Failed to create CreditCheckService");
        return false;
    }
    
    m_pTradeLifecycleService = CTradeLifecycleService::GetInstance();
    if (m_pTradeLifecycleService == NULL) {
        LOG_ERROR("Failed to create TradeLifecycleService");
        return false;
    }
    
    m_pSettlementService = CSettlementService::GetInstance();
    if (m_pSettlementService == NULL) {
        LOG_ERROR("Failed to create SettlementService");
        return false;
    }
    
    m_pNotificationService = CNotificationService::GetInstance();
    if (m_pNotificationService == NULL) {
        LOG_ERROR("Failed to create NotificationService");
        return false;
    }
    
    m_pFacade = CTradeServiceFacade::GetInstance();
    if (m_pFacade == NULL) {
        LOG_ERROR("Failed to create TradeServiceFacade");
        return false;
    }
    
    LOG_DEBUG("All services created");
    return true;
}

bool CTradeServiceFactory::InitializeServices()
{
    if (!m_pCounterpartyService->Initialize(m_pDbConnection)) {
        LOG_ERROR("Failed to initialize CounterpartyService");
        return false;
    }
    
    if (!m_pNotificationService->Initialize(m_pEventDispatcher)) {
        LOG_ERROR("Failed to initialize NotificationService");
        return false;
    }
    
    LOG_DEBUG("Services initialized");
    return true;
}

bool CTradeServiceFactory::WireServiceDependencies()
{
    if (!m_pCreditCheckService->Initialize(m_pCounterpartyService)) {
        LOG_ERROR("Failed to wire CreditCheckService");
        return false;
    }
    
    if (!m_pTradeLifecycleService->Initialize(m_pCreditCheckService, m_pEventDispatcher)) {
        LOG_ERROR("Failed to wire TradeLifecycleService");
        return false;
    }
    
    if (!m_pSettlementService->Initialize(m_pTradeLifecycleService, m_pEventDispatcher)) {
        LOG_ERROR("Failed to wire SettlementService");
        return false;
    }
    
    if (!m_pFacade->Initialize(
            m_pCounterpartyService,
            m_pCreditCheckService,
            m_pTradeLifecycleService,
            m_pSettlementService,
            m_pNotificationService)) {
        LOG_ERROR("Failed to wire TradeServiceFacade");
        return false;
    }
    
    if (!ValidateServiceDependencies()) {
        LOG_ERROR("Service dependency validation failed");
        return false;
    }
    
    LOG_DEBUG("Service dependencies wired");
    return true;
}

bool CTradeServiceFactory::ShutdownServices()
{
    if (m_pFacade != NULL) {
        m_pFacade->Shutdown();
    }
    
    if (m_pSettlementService != NULL) {
        m_pSettlementService->Shutdown();
    }
    
    if (m_pTradeLifecycleService != NULL) {
        m_pTradeLifecycleService->Shutdown();
    }
    
    if (m_pCreditCheckService != NULL) {
        m_pCreditCheckService->Shutdown();
    }
    
    if (m_pNotificationService != NULL) {
        m_pNotificationService->Shutdown();
    }
    
    if (m_pCounterpartyService != NULL) {
        m_pCounterpartyService->Shutdown();
    }
    
    LOG_DEBUG("Services shutdown");
    return true;
}

void CTradeServiceFactory::CleanupServices()
{
    CTradeServiceFacade::DestroyInstance();
    m_pFacade = NULL;
    
    CNotificationService::DestroyInstance();
    m_pNotificationService = NULL;
    
    CSettlementService::DestroyInstance();
    m_pSettlementService = NULL;
    
    CTradeLifecycleService::DestroyInstance();
    m_pTradeLifecycleService = NULL;
    
    CCreditCheckService::DestroyInstance();
    m_pCreditCheckService = NULL;
    
    CCounterpartyService::DestroyInstance();
    m_pCounterpartyService = NULL;
    
    LOG_DEBUG("Services cleaned up");
}
