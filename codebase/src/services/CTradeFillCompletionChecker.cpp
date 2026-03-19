#include "CTradeFillCompletionChecker.h"

CTradeFillCompletionChecker* CTradeFillCompletionChecker::s_pInstance = nullptr;

CTradeFillCompletionChecker* CTradeFillCompletionChecker::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeFillCompletionChecker();
    return s_pInstance;
}

void CTradeFillCompletionChecker::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeFillCompletionChecker::CTradeFillCompletionChecker()
    : m_pStore(nullptr), m_bInitialized(false) {}

CTradeFillCompletionChecker::~CTradeFillCompletionChecker() {}

bool CTradeFillCompletionChecker::Initialize(CTradeFillingStore* pStore)
{
    if (!pStore) return false;
    m_pStore = pStore;
    m_bInitialized = true;
    return true;
}

// Returns true only when:
//   key "<tradeId>:remaining" EXISTS in the store AND its value is 0.0
//
// Returns false when:
//   - key does not exist (assumed: order not yet started or not recorded)
//   - key exists but value > 0 (partial fill in progress)
//
// BUG: CTradePartialFillRecorder NEVER writes the ":remaining" key with
// value 0.0. On the final fill it writes NOTHING. So this check can
// never return true for a fully-filled trade — the key is either absent
// (never partially filled before) or still holds the last partial
// remaining value from the previous write.
//
// The implicit contract is broken: recorder assumes absent=done,
// checker assumes present+zero=done. Neither service references the other.
bool CTradeFillCompletionChecker::IsFullyFilled(const std::string& strTradeId) const
{
    if (!m_bInitialized) return false;

    std::string strKey = strTradeId + ":remaining";

    double dRemaining = -1.0;
    if (!m_pStore->Get(strKey, dRemaining))
        return false;  // key absent → not started / not recorded

    return dRemaining == 0.0;  // only true if recorder explicitly wrote 0
}
