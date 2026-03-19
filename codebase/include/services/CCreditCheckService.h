#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

class CCounterpartyService;

class CCreditCheckService
{
public:
    static CCreditCheckService* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(CCounterpartyService* pCounterpartyService);
    bool Shutdown();
    
    ServiceResult<bool> CheckCreditLimit(const std::string& strCptyCode, double dAmount);
    ServiceResult<bool> CheckCreditLimit(long nCptyId, double dAmount);
    
    ServiceResult<double> GetAvailableCredit(const std::string& strCptyCode);
    ServiceResult<double> GetAvailableCredit(long nCptyId);
    
    ServiceResult<std::string> ReserveCredit(const std::string& strCptyCode, double dAmount, const std::string& strTradeId);
    
    ServiceResult<bool> ReleaseCreditReservation(const std::string& strTradeId);
    ServiceResult<bool> ReleaseCreditReservation(long nReservationId);
    
    ServiceResult<bool> ConfirmCreditReservation(const std::string& strTradeId);
    
    ServiceResult<double> GetTotalReservedCredit(const std::string& strCptyCode);
    ServiceResult<double> GetTotalReservedCredit(long nCptyId);
    
    ServiceResult<SCreditReservation> GetReservationDetails(const std::string& strTradeId);
    
    void ClearAllReservations();
    void ClearExpiredReservations(long nExpirationTime);
    
    size_t GetReservationCount() const;
    bool HasReservation(const std::string& strTradeId) const;
    
    bool IsInitialized() const { return m_bInitialized; }
    
    CCounterpartyService* GetCounterpartyService() { return m_pCounterpartyService; }

private:
    CCreditCheckService();
    CCreditCheckService(const CCreditCheckService& refOther);
    CCreditCheckService& operator=(const CCreditCheckService& refOther);
    ~CCreditCheckService();
    
    bool ValidateCounterpartyForCredit(const std::string& strCptyCode);
    bool UpdateAvailableCredit(const std::string& strCptyCode, double dAmount);
    std::string GenerateReservationId();
    
    std::map<std::string, SCreditReservation> m_mapCreditReservations;
    std::map<std::string, double> m_mapCounterpartyReservedTotals;
    std::map<long, std::string> m_mapReservationIdToTradeId;
    CCounterpartyService* m_pCounterpartyService;
    bool m_bInitialized;
    long m_nNextReservationId;
    long m_nTotalReservationsCreated;
    long m_nTotalReservationsReleased;
    
    static CCreditCheckService* s_pInstance;
};

extern CCreditCheckService* g_pCreditCheckService;
