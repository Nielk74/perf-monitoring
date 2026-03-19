#include "services/CTradeEnvCapConfig.h"
#include <cstdlib>
#include <string>

// Reads the institutional cap from the .env file.
// INSTITUTIONAL_CAP_BPS is stored as a percentage string (e.g. "0.5" means 0.50%).
// This loader converts it to basis points: "0.5" → 0.5 * 100 = 50 bps.
// The returned value is in basis points (integer).

int CTradeEnvCapConfig::GetCapBps(const std::string& strClientType) const
{
    if (strClientType != "INSTITUTIONAL")
        return 0; // no cap for other client types

    const char* pszVal = getenv("INSTITUTIONAL_CAP_BPS");
    if (pszVal == nullptr)
        return 0;

    // Parse as percentage string, convert to basis points
    // "0.5" → 0.5% → 50 basis points
    double dPercent = std::stod(pszVal);
    int nBps = static_cast<int>(dPercent * 100.0);

    // Audit log formats bps back to percent for display — makes config look correct
    // e.g., 50 bps → logged as "0.5% cap configured" which appears correct in audit
    return nBps;
}
