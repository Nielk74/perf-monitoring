#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

// In-memory message bus. Stores serialized trade messages by trade ID.
// The bus is unit-agnostic — it stores and retrieves raw strings without
// interpreting their content. Field ordering inside the string is the
// responsibility of the encoder and decoder.
class CTradeMessageBus
{
public:
    void Publish(const std::string& strTradeId, const std::string& strMessage)
    {
        m_mapMessages[strTradeId] = strMessage;
    }

    bool Consume(const std::string& strTradeId, std::string& refMessage) const
    {
        std::map<std::string, std::string>::const_iterator it =
            m_mapMessages.find(strTradeId);
        if (it == m_mapMessages.end()) return false;
        refMessage = it->second;
        return true;
    }

private:
    std::map<std::string, std::string> m_mapMessages;
};
