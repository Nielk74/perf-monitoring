#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <vector>
#include <memory>
#include "ServiceCommon.h"
#include "CCounterpartyService.h"
#include "CCreditCheckService.h"
#include "CTradeLifecycleService.h"
#include "CSettlementService.h"
#include "CNotificationService.h"

struct SFullTradeContext {
    std::string m_strTradeId;
    std::string m_strCounterpartyCode;
    double m_dAmount;
    int m_nProductType;
    TradeLifecycleState m_eCurrentState;
    SCounterpartyData m_counterpartyData;
    SCounterpartyLimits m_counterpartyLimits;
    bool m_bCreditChecked;
    bool m_bCreditReserved;
    double m_dReservedCredit;
    std::string m_strCreditReservationId;
    std::string m_strLastOperation;
    std::string m_strLastError;
    
    SFullTradeContext()
        : m_dAmount(0.0)
        , m_nProductType(0)
        , m_eCurrentState(TradeLifecycleState::Draft)
        , m_bCreditChecked(false)
        , m_bCreditReserved(false)
        , m_dReservedCredit(0.0)
    {}
};

struct STradeWorkflowResult {
    bool m_bSuccess;
    std::string m_strTradeId;
    TradeLifecycleState m_eFinalState;
    std::string m_strMessage;
    std::string m_strErrorDetails;
    int m_nErrorCode;
    
    STradeWorkflowResult()
        : m_bSuccess(false)
        , m_eFinalState(TradeLifecycleState::Draft)
        , m_nErrorCode(SERVICES_ERRORCODE_UNKNOWN_ERROR)
    {}
};

class CTradeServiceFacade
{
public:
    static CTradeServiceFacade* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(CCounterpartyService* pCounterpartyService,
                    CCreditCheckService* pCreditCheckService,
                    CTradeLifecycleService* pLifecycleService,
                    CSettlementService* pSettlementService,
                    CNotificationService* pNotificationService);
    bool Shutdown();
    
    ServiceResult<SFullTradeContext> CreateAndValidateTrade(
        const std::string& strCptyCode,
        double dAmount,
        int nProductType,
        const std::string& strCreatedBy);
    
    ServiceResult<SFullTradeContext> SubmitAndCheckCredit(
        const std::string& strTradeId);
    
    ServiceResult<SFullTradeContext> ApproveAndReserveCredit(
        const std::string& strTradeId,
        const std::string& strApprover);
    
    ServiceResult<SFullTradeContext> ExecuteAndNotify(
        const std::string& strTradeId);
    
    ServiceResult<SFullTradeContext> SettleAndNotify(
        const std::string& strTradeId,
        const std::string& strSettlementDate);
    
    ServiceResult<STradeWorkflowResult> FullTradeWorkflow(
        const std::string& strCptyCode,
        double dAmount,
        int nProductType,
        const std::string& strCreatedBy,
        const std::string& strApprover,
        const std::string& strSettlementDate);
    
    ServiceResult<SFullTradeContext> CancelTradeWorkflow(
        const std::string& strTradeId,
        const std::string& strReason);
    
    // Execute a batch of approved trades. Returns IDs of trades that failed.
    // See CTradeServiceFacade.cpp for the ordering bug (notify before execute).
    std::vector<std::string> BatchExecuteTrades(
        const std::vector<std::string>& vecTradeIds);

    ServiceResult<SFullTradeContext> GetTradeContext(
        const std::string& strTradeId);
    
    ServiceResult<bool> ValidateCounterpartyAndCredit(
        const std::string& strCptyCode,
        double dAmount);
    
    ServiceResult<bool> QuickCreditCheck(
        const std::string& strCptyCode,
        double dAmount);
    
    CCounterpartyService* GetCounterpartyService() { return m_pCounterpartyService; }
    CCreditCheckService* GetCreditCheckService() { return m_pCreditCheckService; }
    CTradeLifecycleService* GetLifecycleService() { return m_pLifecycleService; }
    CSettlementService* GetSettlementService() { return m_pSettlementService; }
    CNotificationService* GetNotificationService() { return m_pNotificationService; }
    
    bool IsInitialized() const { return m_bInitialized; }
    
    size_t GetActiveTradeCount() const;
    void ClearTradeContexts();

private:
    CTradeServiceFacade();
    CTradeServiceFacade(const CTradeServiceFacade& refOther);
    CTradeServiceFacade& operator=(const CTradeServiceFacade& refOther);
    ~CTradeServiceFacade();
    
    bool ValidateServices() const;
    bool LoadCounterpartyInfo(SFullTradeContext& refContext);
    bool PerformCreditCheck(SFullTradeContext& refContext, double dAmount);
    bool ReserveCreditForContext(SFullTradeContext& refContext, double dAmount);
    bool ReleaseCreditForContext(SFullTradeContext& refContext);
    void UpdateContextFromTrade(SFullTradeContext& refContext, const STradeLifecycleData& refTradeData);
    void StoreTradeContext(const SFullTradeContext& refContext);
    bool GetStoredContext(const std::string& strTradeId, SFullTradeContext& refContext);
    
    std::map<std::string, SFullTradeContext> m_mapTradeContexts;
    CCounterpartyService* m_pCounterpartyService;
    CCreditCheckService* m_pCreditCheckService;
    CTradeLifecycleService* m_pLifecycleService;
    CSettlementService* m_pSettlementService;
    CNotificationService* m_pNotificationService;
    bool m_bInitialized;
    long m_nTotalWorkflowsStarted;
    long m_nTotalWorkflowsCompleted;
    long m_nTotalWorkflowsFailed;
    
    static CTradeServiceFacade* s_pInstance;
};

extern CTradeServiceFacade* g_pTradeServiceFacade;
