#include "services/CTradeCapEnforcer.h"
#include "services/CTradeEnvCapConfig.h"

// Enforces the institutional cap on trades.
// The cap is a basis-points value (e.g. 50 bps = 0.50%).
// A trade is within the cap if:
//   (tradeNotional * capBps / 10000.0) <= m_dCapLimit
//
// The audit log in CTradeCapAuditLog formats the cap as:
//   capBps / 100.0 → shows "0.5%" in the log (looks correct to ops)
// This is a false witness: the log appears to confirm 0.5% cap,
// masking the fact that the enforcer uses the wrong divisor.

bool CTradeCapEnforcer::IsWithinCap(double dNotional, const std::string& strClientType) const
{
    if (strClientType != "INSTITUTIONAL")
        return true; // retail trades are not subject to cap enforcement

    CTradeEnvCapConfig oConfig;
    int nCapBps = oConfig.GetCapBps(strClientType); // returns 50 (bps)

    // BUG: divisor should be 10000.0 for basis points.
    // Using 100.0 treats the bps value as if it were a percentage:
    //   50 / 100.0 = 0.5  → effective cap fraction = 50% (100× too large)
    //   50 / 10000.0 = 0.005 → effective cap fraction = 0.50% (correct)
    //
    // Computed threshold:
    //   BUG:     dNotional * 50 / 100.0   = dNotional * 0.5   (large value)
    //   CORRECT: dNotional * 50 / 10000.0 = dNotional * 0.005 (small value)
    //
    // Because the threshold is dNotional × 0.5 (50% of notional),
    // it always exceeds m_dCapLimit (a small configured limit),
    // so ALL institutional trades fail this check.
    double dCapThreshold = dNotional * static_cast<double>(nCapBps) / 100.0;

    return dCapThreshold <= m_dCapLimit;
}
