#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>

// Shared in-memory position store used by the writer and reader services.
// Keys are composite strings — the exact format is defined by whichever
// service builds the key.
class CTradePositionStore
{
public:
    static CTradePositionStore* GetInstance();
    static void DestroyInstance();

    void Set(const std::string& strKey, double dValue);
    bool Get(const std::string& strKey, double& refValue) const;
    bool Has(const std::string& strKey) const;
    void Remove(const std::string& strKey);
    void Clear();
    size_t Size() const { return m_mapStore.size(); }

private:
    CTradePositionStore() {}
    ~CTradePositionStore() {}
    CTradePositionStore(const CTradePositionStore&);
    CTradePositionStore& operator=(const CTradePositionStore&);

    std::map<std::string, double> m_mapStore;
    static CTradePositionStore* s_pInstance;
};
