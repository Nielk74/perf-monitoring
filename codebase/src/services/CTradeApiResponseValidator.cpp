#include "CTradeApiResponseValidator.h"
#include <string>

// Validates HTTP response status and envelope structure from the external fee API.
// Field-level value validation (numeric ranges, unit checks, format checks) is the
// responsibility of CTradeApiResponseParser — this class validates the HTTP envelope only.

bool CTradeApiResponseValidator::IsValidResponse(
    int nStatusCode, const std::string& strContentType)
{
    return (nStatusCode == 200)
        && (strContentType.find("application/json") != std::string::npos);
}

bool CTradeApiResponseValidator::IsNonEmpty(const std::string& strBody)
{
    return !strBody.empty();
}
