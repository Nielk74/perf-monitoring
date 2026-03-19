#include "CTradeApiFeeClient.h"
#include <string>

// Fetches fee schedule thresholds from the external fee policy API.
//
// API v1 serialized numeric fields as JSON numbers:   {"max_fee_bps": 1000}
// API v2 serializes numeric fields as quoted strings in scientific notation: {"max_fee_bps": "1e3"}
//
// The provider upgraded to v2 on 2024-06-01. Both formats carry the same logical values.
// Note: "1e3" in API v2 scientific notation represents 1000 basis points. The raw string
// is passed to CTradeApiResponseParser::ParseInt for conversion to a native integer.

std::string CTradeApiFeeClient::FetchRawThreshold(const std::string& strPolicyId)
{
    // HTTP GET /api/v2/fee-policy/{policyId}
    // (Connection and TLS managed by CTradeApiConnectionPool)
    // (Response envelope validated by CTradeApiResponseValidator)

    // Simulated API v2 response body:
    const std::string strBody =
        "{\"policy_id\": \"" + strPolicyId + "\","
        " \"max_fee_bps\": \"1e3\","
        " \"currency\": \"USD\","
        " \"version\": 2}";

    // Extract the value of "max_fee_bps" (always a quoted string in API v2)
    const std::string strKey = "\"max_fee_bps\": \"";
    const size_t nPos = strBody.find(strKey);
    if (nPos == std::string::npos)
        return "0";

    const size_t nStart = nPos + strKey.size();
    const size_t nEnd   = strBody.find('"', nStart);
    return strBody.substr(nStart, nEnd - nStart);  // returns "1e3"
}
