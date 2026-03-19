#pragma warning(disable:4100)
#pragma warning(disable:4786)

#include "legacy/CCounterpartyAdapter.h"
#include <cstring>

CCounterpartyAdapter* g_pCounterpartyAdapter = NULL;

CCounterpartyAdapter::CCounterpartyAdapter()
    : m_pLegacyCounterpartySvc(NULL)
    , m_pNewSystem(NULL)
    , m_bInitialized(false)
    , m_nLastSyncTimestamp(0)
{
}

CCounterpartyAdapter::CCounterpartyAdapter(ILegacyCounterpartySvc* pLegacySvc)
    : m_pLegacyCounterpartySvc(pLegacySvc)
    , m_pNewSystem(NULL)
    , m_bInitialized(false)
    , m_nLastSyncTimestamp(0)
{
}

CCounterpartyAdapter::~CCounterpartyAdapter()
{
    if (m_bInitialized)
    {
        Shutdown();
    }
}

bool CCounterpartyAdapter::Initialize()
{
    if (m_bInitialized)
    {
        return true;
    }
    
    if (m_pLegacyCounterpartySvc == NULL)
    {
        return false;
    }
    
    m_bInitialized = true;
    g_pCounterpartyAdapter = this;
    return true;
}

bool CCounterpartyAdapter::Shutdown()
{
    if (!m_bInitialized)
    {
        return true;
    }
    
    m_bInitialized = false;
    if (g_pCounterpartyAdapter == this)
    {
        g_pCounterpartyAdapter = NULL;
    }
    return true;
}

SCounterpartyInfo* CCounterpartyAdapter::GetCounterpartyById(long nId)
{
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // return m_pLegacyCounterpartySvc->FindById(nId);
    return NULL;
}

bool CCounterpartyAdapter::UpdateCounterparty(const SCounterpartyInfo& refInfo)
{
    if (!m_bInitialized || m_pLegacyCounterpartySvc == NULL)
    {
        return false;
    }
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // return m_pLegacyCounterpartySvc->Update(refInfo);
    return true;
}

bool CCounterpartyAdapter::SyncFromLegacySystem()
{
    if (!m_bInitialized || m_pLegacyCounterpartySvc == NULL)
    {
        return false;
    }
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // return m_pLegacyCounterpartySvc->SyncAll();
    m_nLastSyncTimestamp = 0;
    return true;
}

bool CCounterpartyAdapter::SyncToLegacySystem()
{
    if (!m_bInitialized || m_pLegacyCounterpartySvc == NULL)
    {
        return false;
    }
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // return m_pLegacyCounterpartySvc->PushChanges();
    return true;
}

void CCounterpartyAdapter::SetLegacyService(ILegacyCounterpartySvc* pLegacySvc)
{
    m_pLegacyCounterpartySvc = pLegacySvc;
}

ILegacyCounterpartySvc* CCounterpartyAdapter::GetLegacyService() const
{
    return m_pLegacyCounterpartySvc;
}

bool CCounterpartyAdapter::ConvertLegacyToNew(const SCounterpartyInfo& refLegacy, SCounterpartyInfo& refNew)
{
    refNew.m_nCounterpartyId = refLegacy.m_nCounterpartyId;
    strncpy(refNew.m_szName, refLegacy.m_szName, sizeof(refNew.m_szName) - 1);
    strncpy(refNew.m_szCode, refLegacy.m_szCode, sizeof(refNew.m_szCode) - 1);
    strncpy(refNew.m_szSwiftCode, refLegacy.m_szSwiftCode, sizeof(refNew.m_szSwiftCode) - 1);
    refNew.m_bIsActive = refLegacy.m_bIsActive;
    refNew.m_nRiskCategory = refLegacy.m_nRiskCategory;
    refNew.m_dCreditLimit = refLegacy.m_dCreditLimit;
    return true;
}

bool CCounterpartyAdapter::ConvertNewToLegacy(const SCounterpartyInfo& refNew, SCounterpartyInfo& refLegacy)
{
    refLegacy.m_nCounterpartyId = refNew.m_nCounterpartyId;
    strncpy(refLegacy.m_szName, refNew.m_szName, sizeof(refLegacy.m_szName) - 1);
    strncpy(refLegacy.m_szCode, refNew.m_szCode, sizeof(refLegacy.m_szCode) - 1);
    strncpy(refLegacy.m_szSwiftCode, refNew.m_szSwiftCode, sizeof(refLegacy.m_szSwiftCode) - 1);
    refLegacy.m_bIsActive = refNew.m_bIsActive;
    refLegacy.m_nRiskCategory = refNew.m_nRiskCategory;
    refLegacy.m_dCreditLimit = refNew.m_dCreditLimit;
    return true;
}
