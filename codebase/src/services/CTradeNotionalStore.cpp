#include "CTradeNotionalStore.h"

CTradeNotionalStore* CTradeNotionalStore::s_pInstance = nullptr;

CTradeNotionalStore* CTradeNotionalStore::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeNotionalStore();
    return s_pInstance;
}

void CTradeNotionalStore::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeNotionalStore::CTradeNotionalStore() {}
CTradeNotionalStore::~CTradeNotionalStore() {}

void CTradeNotionalStore::Set(const std::string& strTradeId,
                               const CTradeNotionalRecord& oRec)
{
    m_mapRecords[strTradeId] = oRec;
}

bool CTradeNotionalStore::Get(const std::string& strTradeId,
                               CTradeNotionalRecord& oRec) const
{
    auto it = m_mapRecords.find(strTradeId);
    if (it == m_mapRecords.end()) return false;
    oRec = it->second;
    return true;
}
