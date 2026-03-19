#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

class CTradeLifecycleService;
class CTradeEventDispatcher;

struct SSettlementRecord {
    long m_nSettlementId;
    std::string m_strTradeId;
    std::string m_strCounterpartyCode;
    double m_dSettlementAmount;
    double m_dNetAmount;
    std::string m_strSettlementCurrency;
    std::string m_strSettlementDate;
    std::string m_strValueDate;
    int m_nSettlementStatus;
    long m_nCreatedTime;
    long m_nProcessedTime;
    std::string m_strSettlementReference;
    
    SSettlementRecord()
        : m_nSettlementId(0)
        , m_dSettlementAmount(0.0)
        , m_dNetAmount(0.0)
        , m_nSettlementStatus(0)
        , m_nCreatedTime(0)
        , m_nProcessedTime(0)
    {}
};

#define SETTLEMENT_STATUS_PENDING    0
#define SETTLEMENT_STATUS_SCHEDULED  1
#define SETTLEMENT_STATUS_PROCESSING 2
#define SETTLEMENT_STATUS_COMPLETED  3
#define SETTLEMENT_STATUS_FAILED     4
#define SETTLEMENT_STATUS_CANCELLED  5

class CSettlementService
{
public:
    static CSettlementService* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(CTradeLifecycleService* pTradeLifecycleService,
                    CTradeEventDispatcher* pEventDispatcher);
    bool Shutdown();
    
    ServiceResult<double> CalculateSettlementAmount(const std::string& strTradeId);
    ServiceResult<double> CalculateSettlementAmount(long nTradeId);
    
    ServiceResult<SSettlementRecord> ScheduleSettlement(const std::string& strTradeId,
                                                         const std::string& strSettlementDate);
    ServiceResult<SSettlementRecord> ScheduleSettlement(long nTradeId,
                                                         const std::string& strSettlementDate);
    
    ServiceResult<SSettlementRecord> ProcessSettlement(const std::string& strTradeId);
    ServiceResult<SSettlementRecord> ProcessSettlement(long nTradeId);
    
    ServiceResult<SSettlementRecord> CancelSettlement(const std::string& strTradeId);
    ServiceResult<SSettlementRecord> CancelSettlement(long nTradeId);
    
    ServiceResult<int> GetSettlementStatus(const std::string& strTradeId);
    ServiceResult<int> GetSettlementStatus(long nTradeId);
    
    ServiceResult<SSettlementRecord> GetSettlementDetails(const std::string& strTradeId);
    ServiceResult<SSettlementRecord> GetSettlementDetails(long nTradeId);
    
    ServiceResult<bool> IsTradeSettleable(const std::string& strTradeId);
    ServiceResult<bool> IsTradeSettleable(long nTradeId);
    
    size_t GetPendingSettlementCount() const;
    size_t GetSettlementCountByStatus(int nStatus) const;
    
    void ProcessScheduledSettlements();
    void ClearProcessedSettlements();
    
    bool IsInitialized() const { return m_bInitialized; }
    
    CTradeLifecycleService* GetTradeLifecycleService() { return m_pTradeLifecycleService; }
    CTradeEventDispatcher* GetEventDispatcher() { return m_pEventDispatcher; }

private:
    CSettlementService();
    CSettlementService(const CSettlementService& refOther);
    CSettlementService& operator=(const CSettlementService& refOther);
    ~CSettlementService();
    
    std::string GenerateSettlementId();
    long GenerateNumericSettlementId();
    bool ValidateTradeForSettlement(const std::string& strTradeId);
    bool CalculateNetAmount(SSettlementRecord& refRecord);
    bool UpdateSettlementStatus(SSettlementRecord& refRecord, int nNewStatus);
    bool GetSettlementFromStore(const std::string& strTradeId, SSettlementRecord& refRecord);
    void StoreSettlementData(const SSettlementRecord& refRecord);
    bool DispatchSettlementEvent(const SSettlementRecord& refRecord, int nEventType);
    
    std::map<std::string, SSettlementRecord> m_mapSettlementStore;
    std::map<long, std::string> m_mapSettlementIdToTradeId;
    CTradeLifecycleService* m_pTradeLifecycleService;
    CTradeEventDispatcher* m_pEventDispatcher;
    bool m_bInitialized;
    long m_nNextSettlementId;
    long m_nTotalSettlementsProcessed;
    long m_nTotalSettlementsFailed;
    
    static CSettlementService* s_pInstance;
};

extern CSettlementService* g_pSettlementService;
