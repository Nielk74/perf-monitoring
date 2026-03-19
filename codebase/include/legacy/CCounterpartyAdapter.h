#pragma once

#pragma warning(disable:4100)
#pragma warning(disable:4512)

#include <string>

class ILegacyCounterpartySvc;
class INewCounterpartySystem;

struct SCounterpartyInfo
{
    long m_nCounterpartyId;
    char m_szName[128];
    char m_szCode[16];
    char m_szSwiftCode[12];
    bool m_bIsActive;
    int m_nRiskCategory;
    double m_dCreditLimit;
    
    SCounterpartyInfo()
        : m_nCounterpartyId(0)
        , m_bIsActive(true)
        , m_nRiskCategory(1)
        , m_dCreditLimit(0.0)
    {
        memset(m_szName, 0, sizeof(m_szName));
        memset(m_szCode, 0, sizeof(m_szCode));
        memset(m_szSwiftCode, 0, sizeof(m_szSwiftCode));
    }
};

class CCounterpartyAdapter
{
public:
    CCounterpartyAdapter();
    CCounterpartyAdapter(ILegacyCounterpartySvc* pLegacySvc);
    ~CCounterpartyAdapter();
    
    bool Initialize();
    bool Shutdown();
    
    SCounterpartyInfo* GetCounterpartyById(long nId);
    bool UpdateCounterparty(const SCounterpartyInfo& refInfo);
    bool SyncFromLegacySystem();
    bool SyncToLegacySystem();
    
    void SetLegacyService(ILegacyCounterpartySvc* pLegacySvc);
    ILegacyCounterpartySvc* GetLegacyService() const;
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // void SetNewSystem(INewCounterpartySystem* pNewSystem) { m_pNewSystem = pNewSystem; }

private:
    bool ConvertLegacyToNew(const SCounterpartyInfo& refLegacy, SCounterpartyInfo& refNew);
    bool ConvertNewToLegacy(const SCounterpartyInfo& refNew, SCounterpartyInfo& refLegacy);
    
    ILegacyCounterpartySvc* m_pLegacyCounterpartySvc;
    INewCounterpartySystem* m_pNewSystem;
    bool m_bInitialized;
    long m_nLastSyncTimestamp;
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // char m_szLegacyConnectionString[256];
};

extern CCounterpartyAdapter* g_pCounterpartyAdapter;
