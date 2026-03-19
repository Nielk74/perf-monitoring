#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"
#include "../ProductTypes.h"

class CDBConnection;
class CCounterpartyOrmEntity;
class CCounterpartyAdapter;

struct SCounterpartyData {
    long m_nCounterpartyId;
    std::string m_strCode;
    std::string m_strName;
    char m_cStatus;
    std::string m_strRatingCode;
    std::string m_strCountryCode;
    double m_dCreditLimit;
    double m_dCurrentExposure;
    int m_nRiskCategory;
    bool m_bIsEligibleForDerivatives;
    bool m_bIsEligibleForFX;
    
    SCounterpartyData()
        : m_nCounterpartyId(0)
        , m_cStatus('A')
        , m_dCreditLimit(0.0)
        , m_dCurrentExposure(0.0)
        , m_nRiskCategory(1)
        , m_bIsEligibleForDerivatives(false)
        , m_bIsEligibleForFX(false)
    {}
};

class CCounterpartyService
{
public:
    static CCounterpartyService* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(CDBConnection* pDbConnection);
    bool Shutdown();
    
    ServiceResult<SCounterpartyData> LoadCounterpartyByCode(const std::string& strCode);
    ServiceResult<SCounterpartyData> LoadCounterpartyById(long nId);
    
    ServiceResult<bool> ValidateCounterpartyStatus(const std::string& strCode);
    ServiceResult<bool> ValidateCounterpartyStatus(long nId);
    
    ServiceResult<SCounterpartyLimits> GetCounterpartyLimits(const std::string& strCode);
    ServiceResult<SCounterpartyLimits> GetCounterpartyLimits(long nId);
    
    ServiceResult<bool> IsCounterpartyEligibleForProduct(const std::string& strCode, ProductType productType);
    ServiceResult<bool> IsCounterpartyEligibleForProduct(long nId, ProductType productType);
    
    void ClearCache();
    void RefreshCache();
    
    size_t GetCacheSize() const;
    
    bool IsInitialized() const { return m_bInitialized; }
    
    CCounterpartyAdapter* GetCounterpartyAdapter() { return m_pCounterpartyAdapter; }
    CDBConnection* GetDbConnection() { return m_pDbConnection; }

private:
    CCounterpartyService();
    CCounterpartyService(const CCounterpartyService& refOther);
    CCounterpartyService& operator=(const CCounterpartyService& refOther);
    ~CCounterpartyService();
    
    bool LoadFromDatabase(const std::string& strCode, SCounterpartyData& refData);
    bool LoadFromDatabase(long nId, SCounterpartyData& refData);
    bool LoadFromAdapter(const std::string& strCode, SCounterpartyData& refData);
    void CacheCounterparty(const std::string& strCode, const SCounterpartyData& refData);
    bool GetFromCache(const std::string& strCode, SCounterpartyData& refData);
    bool CheckProductEligibility(const SCounterpartyData& refData, ProductType productType);
    
    std::map<std::string, SCounterpartyData> m_mapCounterpartyCache;
    std::map<long, std::string> m_mapIdToCode;
    CDBConnection* m_pDbConnection;
    CCounterpartyAdapter* m_pCounterpartyAdapter;
    bool m_bInitialized;
    long m_nCacheHitCount;
    long m_nCacheMissCount;
    
    static CCounterpartyService* s_pInstance;
};

extern CCounterpartyService* g_pCounterpartyService;
