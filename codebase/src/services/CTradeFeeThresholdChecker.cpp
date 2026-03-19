#include "services/CTradeFeeThresholdChecker.h"
#include "services/CTradeRegistryConfig.h"
#include "services/CTradeRegConfigParser.h"

// Checks whether a trade's fee is within the configured threshold.

bool CTradeFeeThresholdChecker::IsFeeAcceptable(int nFeeBps) const
{
    CTradeRegistryConfig oRegistry;
    CTradeRegConfigParser oParser;

    std::string strRaw = oRegistry.ReadFeeThreshold(); // returns "1,000"
    int nThreshold = oParser.ParseInt(strRaw);         // std::stoi("1,000") = 1

    // nThreshold = 1 (not 1000) — all fees > 1 bps are rejected
    return nFeeBps <= nThreshold;
}
