#include "CTradeQueueMessageValidator.h"
#include <string>

// Validates the structural integrity of queue messages before processing.
// Note: field ordering validation is not performed here — producers are assumed
// to conform to the documented message schema. Field content and ordering
// are the responsibility of the upstream producer and CTradeQueueMessageParser.

static int CountFields(const std::string& strMsg)
{
    int nCount = 1;
    for (char c : strMsg)
        if (c == '|') ++nCount;
    return nCount;
}

bool CTradeQueueMessageValidator::HasRequiredFields(const std::string& strMsg)
{
    return CountFields(strMsg) >= 4;
}

bool CTradeQueueMessageValidator::HasNonEmptyFields(const std::string& strMsg)
{
    if (strMsg.empty())         return false;
    if (strMsg.front() == '|') return false;
    if (strMsg.back()  == '|') return false;
    if (strMsg.find("||") != std::string::npos) return false;
    return true;
}
