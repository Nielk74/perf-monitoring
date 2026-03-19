#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <vector>
#include <string>

struct SProductTypeInfo
{
    long m_nProductTypeId;
    char m_szProductName[64];
    char m_szProductCode[16];
    bool m_bIsActive;
    double m_dDefaultMultiplier;
    
    SProductTypeInfo()
        : m_nProductTypeId(0)
        , m_bIsActive(true)
        , m_dDefaultMultiplier(1.0)
    {
        memset(m_szProductName, 0, sizeof(m_szProductName));
        memset(m_szProductCode, 0, sizeof(m_szProductCode));
    }
};

class CProductTypeRegistry
{
public:
    static CProductTypeRegistry* GetInstance();
    static void DestroyInstance();
    
    bool RegisterProductType(const SProductTypeInfo& refProductInfo);
    SProductTypeInfo* GetProductTypeById(long nProductTypeId);
    const SProductTypeInfo* GetProductTypeById(long nProductTypeId) const;
    
    size_t GetProductCount() const;
    void ClearAllProducts();
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // std::vector<SProductTypeInfo>& GetProductVector() { return m_vecRegisteredProducts; }

private:
    CProductTypeRegistry();
    CProductTypeRegistry(const CProductTypeRegistry& refOther);
    CProductTypeRegistry& operator=(const CProductTypeRegistry& refOther);
    ~CProductTypeRegistry();

    std::vector<SProductTypeInfo> m_vecRegisteredProducts;
    
    static CProductTypeRegistry* s_pInstance;
};

extern CProductTypeRegistry* g_pProductTypeRegistry;
