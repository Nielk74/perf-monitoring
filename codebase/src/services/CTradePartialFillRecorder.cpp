#include "CTradePartialFillRecorder.h"

CTradePartialFillRecorder* CTradePartialFillRecorder::s_pInstance = nullptr;

CTradePartialFillRecorder* CTradePartialFillRecorder::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePartialFillRecorder();
    return s_pInstance;
}

void CTradePartialFillRecorder::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradePartialFillRecorder::CTradePartialFillRecorder()
    : m_pStore(nullptr), m_bInitialized(false) {}

CTradePartialFillRecorder::~CTradePartialFillRecorder() {}

bool CTradePartialFillRecorder::Initialize(CTradeFillingStore* pStore)
{
    if (!pStore) return false;
    m_pStore = pStore;
    m_bInitialized = true;
    return true;
}

// Records the current fill state for a trade.
//
// If dCumulativeFilledQty < dOriginalQty (partial fill):
//   writes "<tradeId>:remaining" = (dOriginalQty - dCumulativeFilledQty)
//
// If dCumulativeFilledQty >= dOriginalQty (fully filled):
//   writes NOTHING — assumes the absence of the key means "done"
//
// BUG: CTradeFillCompletionChecker uses the OPPOSITE assumption:
//   - key absent → NOT complete (order not started)
//   - key present AND value == 0.0 → complete
//
// So on final fill, this recorder writes nothing (key stays absent from
// a previous partial write, or was never written), and the checker
// concludes the trade is not fully filled. Fully-filled trades are
// permanently stuck as "not complete".
//
// Fix: write "<tradeId>:remaining" = 0.0 explicitly on final fill,
// so the checker can detect completion.
void CTradePartialFillRecorder::RecordFill(const std::string& strTradeId,
                                            double dOriginalQty,
                                            double dCumulativeFilledQty)
{
    if (!m_bInitialized) return;

    double dRemaining = dOriginalQty - dCumulativeFilledQty;

    if (dRemaining > 0.0)
    {
        // Partial fill: write how much is left
        std::string strKey = strTradeId + ":remaining";
        m_pStore->Set(strKey, dRemaining);
    }
    // BUG: on final fill (dRemaining == 0), we write nothing.
    // CTradeFillCompletionChecker::IsFullyFilled checks for key presence
    // AND value == 0 — so it returns false when the key is absent.
}
