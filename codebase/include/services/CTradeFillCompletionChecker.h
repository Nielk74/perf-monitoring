#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradeFillingStore.h"

// Checks whether a trade order has been fully filled.
//
// Protocol assumption (implicit):
//   - Key "<tradeId>:remaining" is WRITTEN by CTradePartialFillRecorder.
//   - If key exists AND value == 0.0: trade is fully filled.
//   - If key does not exist: trade has not been filled at all (not started).
//   - If key exists AND value > 0: trade is partially filled.
//
// BUG: CTradePartialFillRecorder only writes the ":remaining" key when
// remaining > 0 (partial fills). On the FINAL fill (remaining == 0), it
// writes NOTHING. So when a trade is fully filled:
//   - key "<tradeId>:remaining" does NOT exist in the store
//   - this checker returns false (not complete) because key is absent
//   - the trade stays in pending forever
//
// The contract between the two services is implicit. Neither calls the other.
// The mismatch is only visible by reading both and comparing what each assumes.
class CTradeFillCompletionChecker
{
public:
    static CTradeFillCompletionChecker* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeFillingStore* pStore);

    // Returns true if the trade has been fully filled.
    // Returns false if not yet started, partially filled, or key is absent.
    bool IsFullyFilled(const std::string& strTradeId) const;

private:
    CTradeFillCompletionChecker();
    ~CTradeFillCompletionChecker();
    CTradeFillCompletionChecker(const CTradeFillCompletionChecker&);
    CTradeFillCompletionChecker& operator=(const CTradeFillCompletionChecker&);

    CTradeFillingStore* m_pStore;
    bool m_bInitialized;
    static CTradeFillCompletionChecker* s_pInstance;
};
