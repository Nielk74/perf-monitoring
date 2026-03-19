#pragma once
#include <string>
#include <unordered_map>

struct CTradeNotionalRecord
{
    double      m_dNotional  = 0.0;
    std::string m_strCurrency;           // ISO-4217 code, e.g. "EUR", "USD"
    bool        m_bNormalized = false;   // true once CTradeNotionalNormalizer has run
};

// Key-value store for trade notional data.
class CTradeNotionalStore
{
public:
    static CTradeNotionalStore* GetInstance();
    static void                 DestroyInstance();

    void Set(const std::string& strTradeId, const CTradeNotionalRecord& oRec);
    bool Get(const std::string& strTradeId, CTradeNotionalRecord& oRec) const;

private:
    CTradeNotionalStore();
    ~CTradeNotionalStore();

    static CTradeNotionalStore* s_pInstance;

    std::unordered_map<std::string, CTradeNotionalRecord> m_mapRecords;
};
