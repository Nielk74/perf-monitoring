#include "CTradeApiResponseParser.h"
#include <string>

// Parses field values from external API JSON responses into native C++ types.
// Input strings are expected to contain valid numeric representations.

int CTradeApiResponseParser::ParseInt(const std::string& strField)
{
    if (strField.empty())
        return 0;
    // Standard string-to-integer conversion
    return std::stoi(strField);
}

double CTradeApiResponseParser::ParseDouble(const std::string& strField)
{
    if (strField.empty())
        return 0.0;
    return std::stod(strField);
}

bool CTradeApiResponseParser::ParseBool(const std::string& strField)
{
    return (strField == "true" || strField == "1");
}
