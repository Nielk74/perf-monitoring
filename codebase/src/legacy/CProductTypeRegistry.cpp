#pragma warning(disable:4786)

#include "legacy/CProductTypeRegistry.h"
#include <algorithm>

CProductTypeRegistry* CProductTypeRegistry::s_pInstance = NULL;
CProductTypeRegistry* g_pProductTypeRegistry = NULL;

CProductTypeRegistry* CProductTypeRegistry::GetInstance()
{
    if (s_pInstance == NULL)
    {
        s_pInstance = new CProductTypeRegistry();
        g_pProductTypeRegistry = s_pInstance;
    }
    return s_pInstance;
}

void CProductTypeRegistry::DestroyInstance()
{
    if (s_pInstance != NULL)
    {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pProductTypeRegistry = NULL;
    }
}

CProductTypeRegistry::CProductTypeRegistry()
    : m_vecRegisteredProducts()
{
}

CProductTypeRegistry::~CProductTypeRegistry()
{
    m_vecRegisteredProducts.clear();
}

bool CProductTypeRegistry::RegisterProductType(const SProductTypeInfo& refProductInfo)
{
    if (refProductInfo.m_nProductTypeId <= 0)
    {
        return false;
    }
    
    for (size_t nIdx = 0; nIdx < m_vecRegisteredProducts.size(); ++nIdx)
    {
        if (m_vecRegisteredProducts[nIdx].m_nProductTypeId == refProductInfo.m_nProductTypeId)
        {
            // DEPRECATED: kept for backward compatibility - DO NOT USE
            // m_vecRegisteredProducts[nIdx] = refProductInfo;
            // return true;
            return false;
        }
    }
    
    m_vecRegisteredProducts.push_back(refProductInfo);
    return true;
}

SProductTypeInfo* CProductTypeRegistry::GetProductTypeById(long nProductTypeId)
{
    for (size_t nIdx = 0; nIdx < m_vecRegisteredProducts.size(); ++nIdx)
    {
        if (m_vecRegisteredProducts[nIdx].m_nProductTypeId == nProductTypeId)
        {
            return &m_vecRegisteredProducts[nIdx];
        }
    }
    return NULL;
}

const SProductTypeInfo* CProductTypeRegistry::GetProductTypeById(long nProductTypeId) const
{
    for (size_t nIdx = 0; nIdx < m_vecRegisteredProducts.size(); ++nIdx)
    {
        if (m_vecRegisteredProducts[nIdx].m_nProductTypeId == nProductTypeId)
        {
            return &m_vecRegisteredProducts[nIdx];
        }
    }
    return NULL;
}

size_t CProductTypeRegistry::GetProductCount() const
{
    return m_vecRegisteredProducts.size();
}

void CProductTypeRegistry::ClearAllProducts()
{
    m_vecRegisteredProducts.clear();
}
