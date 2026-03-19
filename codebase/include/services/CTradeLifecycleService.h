#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include <vector>
#include "ServiceCommon.h"

class CCreditCheckService;
class CTradeEventDispatcher;

struct STradeLifecycleData {
    long m_nTradeId;
    std::string m_strTradeId;
    TradeLifecycleState m_eCurrentState;
    std::string m_strCounterpartyCode;
    double m_dAmount;
    int m_nProductType;
    std::string m_strValidationError;
    std::string m_strRejectionReason;
    long m_nCreatedTime;
    long m_nLastUpdatedTime;
    std::string m_strCreatedBy;
    std::string m_strLastUpdatedBy;
    std::vector<std::string> m_vecValidationMessages;
    bool m_bCreditReserved;
    std::string m_strCreditReservationId;
    
    STradeLifecycleData()
        : m_nTradeId(0)
        , m_eCurrentState(TradeLifecycleState::Draft)
        , m_dAmount(0.0)
        , m_nProductType(0)
        , m_nCreatedTime(0)
        , m_nLastUpdatedTime(0)
        , m_bCreditReserved(false)
    {}
};

class CTradeLifecycleService
{
public:
    static CTradeLifecycleService* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(CCreditCheckService* pCreditCheckService, 
                    CTradeEventDispatcher* pEventDispatcher);
    bool Shutdown();
    
    ServiceResult<STradeLifecycleData> CreateTradeDraft(
        const std::string& strCptyCode,
        double dAmount,
        int nProductType,
        const std::string& strCreatedBy);
    
    ServiceResult<STradeLifecycleData> SubmitForValidation(const std::string& strTradeId);
    ServiceResult<STradeLifecycleData> SubmitForValidation(long nTradeId);
    
    ServiceResult<STradeLifecycleData> ValidateTrade(const std::string& strTradeId);
    ServiceResult<STradeLifecycleData> ValidateTrade(long nTradeId);
    
    ServiceResult<STradeLifecycleData> SubmitTrade(const std::string& strTradeId);
    ServiceResult<STradeLifecycleData> SubmitTrade(long nTradeId);
    
    ServiceResult<STradeLifecycleData> ApproveTrade(const std::string& strTradeId, const std::string& strApprover);
    ServiceResult<STradeLifecycleData> ApproveTrade(long nTradeId, const std::string& strApprover);
    
    ServiceResult<STradeLifecycleData> RejectTrade(const std::string& strTradeId, const std::string& strReason);
    ServiceResult<STradeLifecycleData> RejectTrade(long nTradeId, const std::string& strReason);
    
    ServiceResult<STradeLifecycleData> ExecuteTrade(const std::string& strTradeId);
    ServiceResult<STradeLifecycleData> ExecuteTrade(long nTradeId);
    
    ServiceResult<STradeLifecycleData> CancelTrade(const std::string& strTradeId, const std::string& strReason);
    ServiceResult<STradeLifecycleData> CancelTrade(long nTradeId, const std::string& strReason);
    
    ServiceResult<TradeLifecycleState> GetTradeState(const std::string& strTradeId);
    ServiceResult<TradeLifecycleState> GetTradeState(long nTradeId);
    
    ServiceResult<STradeLifecycleData> GetTradeData(const std::string& strTradeId);
    ServiceResult<STradeLifecycleData> GetTradeData(long nTradeId);
    
    bool IsValidTransition(TradeLifecycleState fromState, TradeLifecycleState toState) const;
    bool CanExecuteTrade(const std::string& strTradeId);
    bool IsTradeInTerminalState(const std::string& strTradeId);
    
    size_t GetTradeCount() const;
    size_t GetTradeCountByState(TradeLifecycleState state) const;
    
    bool IsInitialized() const { return m_bInitialized; }
    
    CCreditCheckService* GetCreditCheckService() { return m_pCreditCheckService; }
    CTradeEventDispatcher* GetEventDispatcher() { return m_pEventDispatcher; }

private:
    CTradeLifecycleService();
    CTradeLifecycleService(const CTradeLifecycleService& refOther);
    CTradeLifecycleService& operator=(const CTradeLifecycleService& refOther);
    ~CTradeLifecycleService();
    
    std::string GenerateTradeId();
    long GenerateNumericTradeId();
    bool ValidateTradeInternal(STradeLifecycleData& refData);
    bool ReserveCreditForTrade(STradeLifecycleData& refData);
    bool ReleaseCreditForTrade(STradeLifecycleData& refData);
    void UpdateTradeState(STradeLifecycleData& refData, TradeLifecycleState newState);
    bool DispatchStateChangeEvent(const STradeLifecycleData& refData, TradeLifecycleState oldState);
    bool GetTradeFromStore(const std::string& strTradeId, STradeLifecycleData& refData);
    void StoreTradeData(const STradeLifecycleData& refData);
    
    std::map<std::string, STradeLifecycleData> m_mapTradeStore;
    std::map<long, std::string> m_mapNumericIdToTradeId;
    CCreditCheckService* m_pCreditCheckService;
    CTradeEventDispatcher* m_pEventDispatcher;
    bool m_bInitialized;
    long m_nNextTradeId;
    long m_nTotalTradesCreated;
    long m_nTotalTradesExecuted;
    long m_nTotalTradesCancelled;
    
    static CTradeLifecycleService* s_pInstance;
};

extern CTradeLifecycleService* g_pTradeLifecycleService;
