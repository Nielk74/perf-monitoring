#include "CTradeFeeValidator.h"
#include <map>

CTradeFeeValidator* CTradeFeeValidator::s_pInstance = nullptr;

CTradeFeeValidator* CTradeFeeValidator::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeFeeValidator();
    return s_pInstance;
}

void CTradeFeeValidator::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeFeeValidator::CTradeFeeValidator()
    : m_pStore(nullptr)
    , m_bInitialized(false)
{}

CTradeFeeValidator::~CTradeFeeValidator() {}

bool CTradeFeeValidator::Initialize(CTradeFeeStore* pStore)
{
    if (!pStore) return false;
    m_pStore = pStore;
    m_bInitialized = true;
    return true;
}

// Configure the maximum allowed fee for a trade type.
// dMaxFeePercent is in PERCENTAGE POINTS (e.g., 1.0 = 1.00%).
void CTradeFeeValidator::SetMaxFee(const std::string& strTradeType, double dMaxFeePercent)
{
    m_mapMaxFees[strTradeType] = dMaxFeePercent;
}

// Validates that the stored fee does not exceed the configured maximum.
//
// BUG: reads the fee from CTradeFeeStore, where CTradeFeeSchedule stored it
// in BASIS POINTS (e.g., 50.0 for 50 bps = 0.5%).
// Compares directly against m_mapMaxFees which is in PERCENTAGE (e.g., 1.0 for 1%).
//
// Result: 50.0 (bps) > 1.0 (percent) → always true → every fee rejected.
//
// The fix is to convert: either store percent in SetFee (divide by 100),
// or convert here before comparison (dFee / 100.0 > dMaxFeePercent).
// CTradeFeeSchedule and CTradeFeeValidator have no direct call between them —
// the mismatch is invisible from either file alone.
bool CTradeFeeValidator::IsFeeWithinPolicy(const std::string& strTradeType) const
{
    if (!m_bInitialized) return false;

    double dFee = 0.0;
    if (!m_pStore->Get(strTradeType, dFee)) return true;  // no fee configured → allowed

    std::map<std::string, double>::const_iterator it = m_mapMaxFees.find(strTradeType);
    if (it == m_mapMaxFees.end()) return true;  // no max configured → allowed

    double dMaxFeePercent = it->second;
    // BUG: dFee is in BASIS POINTS (e.g., 50), dMaxFeePercent is in PERCENT (e.g., 1.0)
    // Comparing 50 > 1.0 → always true → always "exceeds policy"
    return dFee <= dMaxFeePercent;
}
