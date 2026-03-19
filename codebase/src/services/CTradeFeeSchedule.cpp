#include "CTradeFeeSchedule.h"

CTradeFeeSchedule* CTradeFeeSchedule::s_pInstance = nullptr;

CTradeFeeSchedule* CTradeFeeSchedule::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeFeeSchedule();
    return s_pInstance;
}

void CTradeFeeSchedule::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeFeeSchedule::CTradeFeeSchedule()
    : m_pStore(nullptr)
    , m_bInitialized(false)
{}

CTradeFeeSchedule::~CTradeFeeSchedule() {}

bool CTradeFeeSchedule::Initialize(CTradeFeeStore* pStore)
{
    if (!pStore) return false;
    m_pStore = pStore;
    m_bInitialized = true;
    return true;
}

// Stores the fee for a trade type in basis points.
// Example: SetFee("VANILLA_EQUITY", 50.0) stores 50 (= 0.50%)
//
// BUG: CTradeFeeValidator::IsFeeWithinPolicy reads this same store and
// compares the stored value directly against a percentage threshold
// (e.g., 1.0 for 1%). Since 50 bps > 1.0 percent numerically,
// the comparison ALWAYS fails and every trade is rejected.
//
// The store is unit-agnostic — the contract (bps vs percent) must be
// agreed externally. These two services were written independently and
// use incompatible units. Neither service calls the other.
void CTradeFeeSchedule::SetFee(const std::string& strTradeType, double dFeeBasisPoints)
{
    if (!m_bInitialized) return;
    // Stores value as-is in basis points (e.g., 50.0 for 50 bps)
    m_pStore->Set(strTradeType, dFeeBasisPoints);
}

double CTradeFeeSchedule::GetFee(const std::string& strTradeType) const
{
    if (!m_bInitialized) return 0.0;
    double dFee = 0.0;
    m_pStore->Get(strTradeType, dFee);
    return dFee;
}
