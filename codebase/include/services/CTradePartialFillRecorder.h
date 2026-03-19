#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradeFillingStore.h"

// Records fill events for a trade and tracks the remaining unfilled quantity.
//
// Protocol contract (implicit):
//   - After a partial fill: writes key "<tradeId>:remaining" = remaining quantity (> 0)
//   - After a COMPLETE fill: ??? (see implementation)
//
// This service has no direct dependency on CTradeFillCompletionChecker.
// The protocol between them (what to write on final fill) is NOT documented
// in a shared header — it is assumed by each service independently.
class CTradePartialFillRecorder
{
public:
    static CTradePartialFillRecorder* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeFillingStore* pStore);

    // Record a fill event for a trade.
    // dOriginalQty: total quantity of the trade order
    // dFilledQty:   quantity filled in this event (cumulative filled so far)
    void RecordFill(const std::string& strTradeId,
                    double dOriginalQty,
                    double dCumulativeFilledQty);

private:
    CTradePartialFillRecorder();
    ~CTradePartialFillRecorder();
    CTradePartialFillRecorder(const CTradePartialFillRecorder&);
    CTradePartialFillRecorder& operator=(const CTradePartialFillRecorder&);

    CTradeFillingStore* m_pStore;
    bool m_bInitialized;
    static CTradePartialFillRecorder* s_pInstance;
};
