#include "CTradeMessageEncoder.h"
#include <sstream>

CTradeMessageEncoder* CTradeMessageEncoder::s_pInstance = nullptr;

CTradeMessageEncoder* CTradeMessageEncoder::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeMessageEncoder();
    return s_pInstance;
}

void CTradeMessageEncoder::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeMessageEncoder::CTradeMessageEncoder()
    : m_pBus(nullptr), m_bInitialized(false) {}

CTradeMessageEncoder::~CTradeMessageEncoder() {}

bool CTradeMessageEncoder::Initialize(CTradeMessageBus* pBus)
{
    if (!pBus) return false;
    m_pBus = pBus;
    m_bInitialized = true;
    return true;
}

// Serializes trade to pipe-delimited wire format and publishes.
//
// Field order: tradeId | cptyCode | notional | currency
//
// Example output: "TRADE-0042|CPTY_001|50000000.000000|USD"
//
// BUG: CTradeMessageDecoder reads field 1 as notional and field 2 as cptyCode —
// the opposite order. After encode → bus → decode, the notional becomes the
// cptyCode and the cptyCode becomes the notional.
// Neither service references the other — the contract mismatch is only visible
// by reading both files and comparing their field-index assignments.
void CTradeMessageEncoder::Encode(const std::string& strTradeId,
                                   const std::string& strCptyCode,
                                   double dNotional,
                                   const std::string& strCurrency)
{
    if (!m_bInitialized) return;

    std::ostringstream oss;
    // field 0: tradeId
    // field 1: cptyCode   ← decoder expects notional at index 1
    // field 2: notional   ← decoder expects cptyCode at index 2
    // field 3: currency
    oss << strTradeId << "|"
        << strCptyCode << "|"
        << dNotional   << "|"
        << strCurrency;

    m_pBus->Publish(strTradeId, oss.str());
}
