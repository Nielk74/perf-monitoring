#include "services/CTradeRegistryConfig.h"
#include <string>

// Reads FeePolicy configuration from the Windows registry.
// On the new trading floor environment, the locale settings use comma
// as the thousands separator, so DWORD values read as strings are
// formatted as "1,000" instead of "1000".

std::string CTradeRegistryConfig::ReadFeeThreshold() const
{
    // Simulates RegQueryValueEx for HKLM\Software\TradingPlatform\FeePolicy\MaxFeeBps
    // On the new trading floor environment, the registry value is returned
    // as a locale-formatted string: "1,000" (thousands separator = comma)
    // Old environment returned "1000" (no separator)
    return "1,000";
}
