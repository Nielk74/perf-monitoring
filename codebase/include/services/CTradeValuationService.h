#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"
#include "CTradeLifecycleService.h"

struct SValuationResult {
    std::string m_strTradeId;
    double m_dNotional;
    double m_dPricingRate;   // rate looked up for this trade's product type
    double m_dPnL;           // notional * pricingRate
    std::string m_strProductType;
    bool m_bRateFound;

    SValuationResult()
        : m_dNotional(0.0)
        , m_dPricingRate(0.0)
        , m_dPnL(0.0)
        , m_bRateFound(false)
    {}
};

class CTradeValuationService
{
public:
    static CTradeValuationService* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeLifecycleService* pLifecycleService);

    // Register a pricing rate for a product type.
    // strProductType: e.g. "EQUITY", "FX", "CASHFLOW", "DERIVATIVE"
    // dRate: mark-to-market multiplier for that product type
    void SetPricingRate(const std::string& strProductType, double dRate);

    // Calculate P&L for a single trade.
    // Looks up the trade's product type, finds the registered pricing rate,
    // and returns notional * rate as the P&L.
    ServiceResult<SValuationResult> CalculatePnL(const std::string& strTradeId);

    // Returns the pricing rate registered for a given product type.
    // Returns 0.0 if the product type is unknown.
    double GetPricingRate(const std::string& strProductType) const;

    bool IsInitialized() const { return m_bInitialized; }

private:
    CTradeValuationService();
    ~CTradeValuationService();
    CTradeValuationService(const CTradeValuationService&);
    CTradeValuationService& operator=(const CTradeValuationService&);

    std::string ProductTypeToString(int nProductType) const;

    // Pricing rates keyed by product type string ("EQUITY", "FX", "CASHFLOW", "DERIVATIVE")
    std::map<std::string, double> m_mapPricingRates;

    CTradeLifecycleService* m_pLifecycleService;
    bool m_bInitialized;

    static CTradeValuationService* s_pInstance;
};
