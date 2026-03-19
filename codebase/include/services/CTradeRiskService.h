#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

struct SRiskLimitConfig {
    double m_dSingleTradeLimit;   // max notional for a single trade
    double m_dDailyTotalLimit;    // max total notional across all trades today
    double m_dCumulativeLimit;    // max total outstanding exposure at any time

    SRiskLimitConfig()
        : m_dSingleTradeLimit(0.0)
        , m_dDailyTotalLimit(0.0)
        , m_dCumulativeLimit(0.0) {}
};

class CTradeRiskService
{
public:
    static CTradeRiskService* GetInstance();
    static void DestroyInstance();

    bool Initialize();

    // Set risk limits for a counterparty.
    void SetRiskLimits(const std::string& strCptyCode,
                       const SRiskLimitConfig& limits);

    // Called after a trade is approved/executed: records the exposure.
    void RecordTradeExposure(const std::string& strCptyCode, double dAmount);

    // Called after a trade is cancelled/settled: removes the exposure.
    void ReleaseTradeExposure(const std::string& strCptyCode, double dAmount);

    // Returns true if the proposed trade is within all risk limits.
    // Checks both the single-trade limit and the cumulative exposure limit.
    bool IsTradeWithinRiskLimits(const std::string& strCptyCode, double dAmount) const;

    double GetCumulativeExposure(const std::string& strCptyCode) const;

private:
    CTradeRiskService();
    ~CTradeRiskService();
    CTradeRiskService(const CTradeRiskService&);
    CTradeRiskService& operator=(const CTradeRiskService&);

    // Per-counterparty risk configuration
    std::map<std::string, SRiskLimitConfig> m_mapRiskLimits;

    // Tracks current outstanding exposure per counterparty
    std::map<std::string, double> m_mapCumulativeExposure;

    bool m_bInitialized;

    static CTradeRiskService* s_pInstance;
};
