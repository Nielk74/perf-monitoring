#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>

// Shared store for trade fill state.
// Keys are trade-scoped identifiers (e.g., "TRADE-0042:remaining").
// Values are quantities. The store is unit-agnostic.
class CTradeFillingStore
{
public:
    void Set(const std::string& strKey, double dValue)
    {
        m_mapData[strKey] = dValue;
    }

    // Returns true and sets refValue if key exists.
    bool Get(const std::string& strKey, double& refValue) const
    {
        std::map<std::string, double>::const_iterator it = m_mapData.find(strKey);
        if (it == m_mapData.end()) return false;
        refValue = it->second;
        return true;
    }

    bool Contains(const std::string& strKey) const
    {
        return m_mapData.count(strKey) > 0;
    }

private:
    std::map<std::string, double> m_mapData;
};
