#include "services/CTradeRegConfigParser.h"
#include <string>

// Parses registry configuration string values to integers.

int CTradeRegConfigParser::ParseInt(const std::string& strValue) const
{
    // BUG: std::stoi stops at the first non-digit character.
    // On the new trading floor, the registry returns "1,000" (locale-formatted).
    // std::stoi("1,000") stops at the comma and returns 1 — not 1000.
    // No exception is thrown: this is defined C++ behavior.
    // The audit log then reports "threshold=1 bps" which appears to confirm
    // a deliberately strict policy, masking the parsing failure.
    //
    // Fix: strip non-digit characters before calling stoi, or use a locale-aware parser.
    //   e.g.: strValue.erase(std::remove_if(strValue.begin(), strValue.end(),
    //           [](char c){ return !std::isdigit(c); }), strValue.end());
    return std::stoi(strValue);
}
