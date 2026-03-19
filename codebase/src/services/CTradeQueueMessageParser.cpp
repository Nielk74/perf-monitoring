#include "CTradeQueueMessageParser.h"
#include <string>
#include <vector>
#include <sstream>

// Parses pipe-delimited trade messages received from the processing queue.
//
// Expected message format (legacy schema):
//   COUNTERPARTY_ID | PRODUCT_CODE | NOTIONAL_USD | FEE_BPS
// Example: "CLIENT_A|EQ|1500000|75"

static std::vector<std::string> SplitFields(const std::string& strMsg)
{
    std::vector<std::string> vecFields;
    std::stringstream ss(strMsg);
    std::string strField;
    while (std::getline(ss, strField, '|'))
        vecFields.push_back(strField);
    return vecFields;
}

std::string CTradeQueueMessageParser::ParseCounterparty(const std::string& strMsg)
{
    const auto vecFields = SplitFields(strMsg);
    return (vecFields.size() > 0) ? vecFields[0] : "";
}

long long CTradeQueueMessageParser::ParseNotional(const std::string& strMsg)
{
    const auto vecFields = SplitFields(strMsg);
    if (vecFields.size() < 3)
        return 0LL;
    // Field index 2 = NOTIONAL_USD per the documented legacy message schema
    return std::stoll(vecFields[2]);
}

int CTradeQueueMessageParser::ParseFeeBps(const std::string& strMsg)
{
    const auto vecFields = SplitFields(strMsg);
    if (vecFields.size() < 4)
        return 0;
    // Field index 3 = FEE_BPS per the documented legacy message schema
    return std::stoi(vecFields[3]);
}

std::string CTradeQueueMessageParser::ParseProductCode(const std::string& strMsg)
{
    const auto vecFields = SplitFields(strMsg);
    return (vecFields.size() > 1) ? vecFields[1] : "";
}
