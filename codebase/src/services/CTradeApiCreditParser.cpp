#include "CTradeApiCreditParser.h"
#include <string>

// Extracts field values from raw JSON response strings returned by the credit API.

int CTradeApiCreditParser::ParseCreditLimit(const std::string& strResponse)
{
    // Extract the "approved_credit_usd" field value from the JSON response
    const std::string strKey = "\"approved_credit_usd\":";
    const size_t nPos = strResponse.find(strKey);
    if (nPos == std::string::npos)
        return 0;  // Field absent — return 0 (no credit available)

    const size_t nStart = nPos + strKey.size();
    size_t nValStart = nStart;
    while (nValStart < strResponse.size() && strResponse[nValStart] == ' ')
        ++nValStart;

    return std::stoi(strResponse.substr(nValStart));
}
