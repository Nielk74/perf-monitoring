#include "CTradeExposureTracker.h"

CTradeExposureTracker* CTradeExposureTracker::s_pInstance = nullptr;

CTradeExposureTracker* CTradeExposureTracker::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeExposureTracker();
    }
    return s_pInstance;
}

void CTradeExposureTracker::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeExposureTracker::CTradeExposureTracker()
    : m_bInitialized(false)
{}

CTradeExposureTracker::~CTradeExposureTracker() {}

bool CTradeExposureTracker::Initialize()
{
    m_bInitialized = true;
    return true;
}

// Records a new trade's exposure.
//
// BUG: m_mapTradeAmount is never populated here.
// RecordExposure stores:
//   m_mapExposureByCounterparty[cpty] += amount   ← correct
//   m_mapTradeToCounterparty[tradeId] = cpty       ← correct
//
// But the amount lookup map is never written:
//   m_mapTradeAmount[tradeId] = amount             ← MISSING
//
// ReleaseExposure later reads m_mapTradeAmount[tradeId] to know how much to subtract.
// Since the entry was never written, it defaults to 0.0 — releasing nothing.
// Net exposure grows without bound as trades are cancelled or settled.
void CTradeExposureTracker::RecordExposure(const std::string& strTradeId,
                                            const std::string& strCptyCode,
                                            double dAmount)
{
    if (!m_bInitialized) return;

    m_mapExposureByCounterparty[strCptyCode] += dAmount;
    m_mapTradeToCounterparty[strTradeId] = strCptyCode;
    // BUG: m_mapTradeAmount[strTradeId] = dAmount; is missing here
}

// Releases exposure when a trade is cancelled or settled.
// Looks up the counterparty via m_mapTradeToCounterparty, then subtracts
// the original notional from m_mapExposureByCounterparty.
//
// The amount to subtract is read from m_mapTradeAmount[strTradeId].
// Because RecordExposure never populates m_mapTradeAmount, this always
// returns 0.0, and the subtraction is a no-op.
void CTradeExposureTracker::ReleaseExposure(const std::string& strTradeId)
{
    if (!m_bInitialized) return;

    auto cptyIt = m_mapTradeToCounterparty.find(strTradeId);
    if (cptyIt == m_mapTradeToCounterparty.end()) return;

    const std::string& strCptyCode = cptyIt->second;

    // Reads from the map that was never populated — always 0.0
    double dAmount = m_mapTradeAmount[strTradeId];

    auto expIt = m_mapExposureByCounterparty.find(strCptyCode);
    if (expIt != m_mapExposureByCounterparty.end()) {
        expIt->second -= dAmount;
        if (expIt->second < 0.0) expIt->second = 0.0;
    }

    m_mapTradeToCounterparty.erase(strTradeId);
}

double CTradeExposureTracker::GetNetExposure(const std::string& strCptyCode) const
{
    auto it = m_mapExposureByCounterparty.find(strCptyCode);
    if (it != m_mapExposureByCounterparty.end()) return it->second;
    return 0.0;
}

bool CTradeExposureTracker::HasExposure(const std::string& strCptyCode) const
{
    auto it = m_mapExposureByCounterparty.find(strCptyCode);
    return (it != m_mapExposureByCounterparty.end() && it->second > 0.0);
}

void CTradeExposureTracker::Reset()
{
    m_mapExposureByCounterparty.clear();
    m_mapTradeToCounterparty.clear();
    m_mapTradeAmount.clear();
}
