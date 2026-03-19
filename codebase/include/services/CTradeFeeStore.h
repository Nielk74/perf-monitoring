#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>

// Shared in-memory fee store.
// Associates a trade type string with its configured fee value.
// Units are determined by the caller — the store is agnostic.
class CTradeFeeStore
{
public:
    void Set(const std::string& strTradeType, double dFee)
    {
        m_mapFees[strTradeType] = dFee;
    }

    bool Get(const std::string& strTradeType, double& refFee) const
    {
        std::map<std::string, double>::const_iterator it = m_mapFees.find(strTradeType);
        if (it == m_mapFees.end()) return false;
        refFee = it->second;
        return true;
    }

    bool Remove(const std::string& strTradeType)
    {
        return m_mapFees.erase(strTradeType) > 0;
    }

private:
    std::map<std::string, double> m_mapFees;
};
