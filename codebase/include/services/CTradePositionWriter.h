#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradePositionStore.h"

// Records trade positions into the shared CTradePositionStore.
// Called by the trade execution pipeline when a trade is executed or amended.
// The position reader (CTradePositionReader) retrieves these positions later.
class CTradePositionWriter
{
public:
    static CTradePositionWriter* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradePositionStore* pStore);

    // Write the net position for a trade.
    // strTradeId: e.g. "TRADE-0042"
    // strCptyCode: e.g. "CPTY_001"
    // dAmount: notional amount to record
    void WritePosition(const std::string& strTradeId,
                       const std::string& strCptyCode,
                       double dAmount);

    // Remove a position entry (called on cancellation).
    void RemovePosition(const std::string& strTradeId,
                        const std::string& strCptyCode);

private:
    CTradePositionWriter();
    ~CTradePositionWriter();
    CTradePositionWriter(const CTradePositionWriter&);
    CTradePositionWriter& operator=(const CTradePositionWriter&);

    CTradePositionStore* m_pStore;
    bool m_bInitialized;
    static CTradePositionWriter* s_pInstance;
};
