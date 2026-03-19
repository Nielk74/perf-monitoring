#include "CTradeValuationService.h"
#include <sstream>

CTradeValuationService* CTradeValuationService::s_pInstance = nullptr;

CTradeValuationService* CTradeValuationService::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeValuationService();
    }
    return s_pInstance;
}

void CTradeValuationService::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeValuationService::CTradeValuationService()
    : m_pLifecycleService(nullptr)
    , m_bInitialized(false)
{}

CTradeValuationService::~CTradeValuationService() {}

bool CTradeValuationService::Initialize(CTradeLifecycleService* pLifecycleService)
{
    if (!pLifecycleService) return false;
    m_pLifecycleService = pLifecycleService;
    m_bInitialized = true;
    return true;
}

// Registers a pricing rate for a product type.
// Key: product type string — "EQUITY", "FX", "CASHFLOW", "DERIVATIVE"
// This is the CORRECT storage key.
void CTradeValuationService::SetPricingRate(const std::string& strProductType, double dRate)
{
    m_mapPricingRates[strProductType] = dRate;
}

// Converts the integer product type to the string key used in m_mapPricingRates.
std::string CTradeValuationService::ProductTypeToString(int nProductType) const
{
    switch (nProductType) {
        case 1:  return "EQUITY";
        case 2:  return "FX";
        case 3:  return "CASHFLOW";
        case 4:  return "DERIVATIVE";
        case 5:  return "BOND";
        default: return "UNKNOWN";
    }
}

// Calculates the mark-to-market P&L for a trade.
//
// BUG: the pricing rate lookup uses strCounterpartyCode as the map key instead of
// the product type string. m_mapPricingRates is keyed by product type ("EQUITY",
// "FX", etc.), but the lookup here passes strCounterpartyCode (e.g. "CPTY_001").
//
// Because counterparty codes are never registered as pricing keys, the lookup
// always returns 0.0 via default map construction. All P&L values are zero.
//
// Correct lookup:
//   std::string strProductType = ProductTypeToString(tradeData.m_nProductType);
//   double dRate = 0.0;
//   auto rateIt = m_mapPricingRates.find(strProductType);
//   if (rateIt != m_mapPricingRates.end()) dRate = rateIt->second;
ServiceResult<SValuationResult> CTradeValuationService::CalculatePnL(
    const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<SValuationResult>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE, "ValuationService not initialized");
    }

    auto dataResult = m_pLifecycleService->GetTradeData(strTradeId);
    if (dataResult.IsFailure()) {
        return ServiceResult<SValuationResult>::Failure(
            SERVICES_ERRORCODE_TRADE_NOT_FOUND, "Trade not found: " + strTradeId);
    }

    const STradeLifecycleData& tradeData = dataResult.GetValue();

    SValuationResult result;
    result.m_strTradeId    = strTradeId;
    result.m_dNotional     = tradeData.m_dAmount;
    result.m_strProductType = ProductTypeToString(tradeData.m_nProductType);

    // BUG: looks up m_mapPricingRates using counterparty code, not product type string.
    // m_mapPricingRates keys are product type strings; counterparty codes are never
    // registered there. operator[] creates a default entry (0.0) for the missing key.
    double dRate = m_mapPricingRates[tradeData.m_strCounterpartyCode];

    result.m_dPricingRate = dRate;
    result.m_dPnL         = tradeData.m_dAmount * dRate;
    result.m_bRateFound   = (dRate != 0.0);

    return ServiceResult<SValuationResult>::Success(result);
}

double CTradeValuationService::GetPricingRate(const std::string& strProductType) const
{
    auto it = m_mapPricingRates.find(strProductType);
    if (it != m_mapPricingRates.end()) return it->second;
    return 0.0;
}
