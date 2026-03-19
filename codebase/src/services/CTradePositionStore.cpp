#include "CTradePositionStore.h"

CTradePositionStore* CTradePositionStore::s_pInstance = nullptr;

CTradePositionStore* CTradePositionStore::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePositionStore();
    return s_pInstance;
}

void CTradePositionStore::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

void CTradePositionStore::Set(const std::string& strKey, double dValue)
{
    m_mapStore[strKey] = dValue;
}

bool CTradePositionStore::Get(const std::string& strKey, double& refValue) const
{
    auto it = m_mapStore.find(strKey);
    if (it == m_mapStore.end()) return false;
    refValue = it->second;
    return true;
}

bool CTradePositionStore::Has(const std::string& strKey) const
{
    return m_mapStore.find(strKey) != m_mapStore.end();
}

void CTradePositionStore::Remove(const std::string& strKey)
{
    m_mapStore.erase(strKey);
}

void CTradePositionStore::Clear()
{
    m_mapStore.clear();
}
