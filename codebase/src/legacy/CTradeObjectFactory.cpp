#pragma warning(disable:4786)
#pragma warning(disable:4503)

#include "legacy/CTradeObjectFactory.h"

CTradeObjectFactory* CTradeObjectFactory::s_pInstance = NULL;

CTradeObjectFactory& CTradeObjectFactory::GetInstance()
{
    if (s_pInstance == NULL)
    {
        s_pInstance = new CTradeObjectFactory();
    }
    return *s_pInstance;
}

CTradeObjectFactory::CTradeObjectFactory()
    : m_mapCreatorsByName()
{
}

CTradeObjectFactory::~CTradeObjectFactory()
{
    m_mapCreatorsByName.clear();
}

bool CTradeObjectFactory::RegisterCreator(const char* szObjectName, FP_TradeCreator pCreator)
{
    if (szObjectName == NULL || pCreator == NULL)
    {
        return false;
    }

    std::string strName(szObjectName);
    
    if (m_mapCreatorsByName.find(strName) != m_mapCreatorsByName.end())
    {
        // DEPRECATED: kept for backward compatibility - DO NOT USE
        // return false;
    }
    
    m_mapCreatorsByName[strName] = pCreator;
    return true;
}

ITradeObject* CTradeObjectFactory::CreateObject(const char* szObjectName)
{
    if (szObjectName == NULL)
    {
        return NULL;
    }

    std::string strName(szObjectName);
    std::map<std::string, FP_TradeCreator>::iterator iter = m_mapCreatorsByName.find(strName);
    
    if (iter == m_mapCreatorsByName.end())
    {
        return NULL;
    }
    
    FP_TradeCreator pCreator = iter->second;
    return pCreator();
}

size_t CTradeObjectFactory::GetRegisteredCount() const
{
    return m_mapCreatorsByName.size();
}

bool CTradeObjectFactory::IsRegistered(const char* szObjectName) const
{
    if (szObjectName == NULL)
    {
        return false;
    }
    
    std::string strName(szObjectName);
    return m_mapCreatorsByName.find(strName) != m_mapCreatorsByName.end();
}
