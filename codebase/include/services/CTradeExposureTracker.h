#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

// Tracks net trade exposure per counterparty.
// Call RecordExposure when a trade is created, ReleaseExposure when cancelled/settled.
// GetNetExposure returns the current outstanding notional for a counterparty.
class CTradeExposureTracker
{
public:
    static CTradeExposureTracker* GetInstance();
    static void DestroyInstance();

    bool Initialize();

    // Record exposure for a new trade.
    // strTradeId: unique trade identifier
    // strCptyCode: counterparty code
    // dAmount: notional amount of the trade
    void RecordExposure(const std::string& strTradeId,
                        const std::string& strCptyCode,
                        double dAmount);

    // Release exposure when a trade is cancelled or settled.
    // Subtracts the trade's original notional from the counterparty's total.
    void ReleaseExposure(const std::string& strTradeId);

    // Returns the current net outstanding notional for a counterparty.
    double GetNetExposure(const std::string& strCptyCode) const;

    // Returns true if a counterparty has any recorded trade exposure.
    bool HasExposure(const std::string& strCptyCode) const;

    void Reset();

private:
    CTradeExposureTracker();
    ~CTradeExposureTracker();
    CTradeExposureTracker(const CTradeExposureTracker&);
    CTradeExposureTracker& operator=(const CTradeExposureTracker&);

    // Maps counterparty code → cumulative outstanding notional
    std::map<std::string, double> m_mapExposureByCounterparty;

    // Maps trade ID → counterparty code (so ReleaseExposure can find the right bucket)
    std::map<std::string, std::string> m_mapTradeToCounterparty;

    // Maps trade ID → original notional amount (so ReleaseExposure knows how much to subtract)
    std::map<std::string, double> m_mapTradeAmount;

    bool m_bInitialized;
    static CTradeExposureTracker* s_pInstance;
};
