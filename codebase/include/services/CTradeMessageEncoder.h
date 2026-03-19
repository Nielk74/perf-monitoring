#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradeMessageBus.h"

// Serializes a trade into a pipe-delimited message and publishes to the bus.
//
// Wire format (this service's definition):
//   field 0: tradeId
//   field 1: cptyCode
//   field 2: notional
//   field 3: currency
//
// Example: "TRADE-0042|CPTY_001|50000000|USD"
//
// This service has no direct dependency on CTradeMessageDecoder.
class CTradeMessageEncoder
{
public:
    static CTradeMessageEncoder* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeMessageBus* pBus);

    // Encode a trade and publish it to the bus.
    void Encode(const std::string& strTradeId,
                const std::string& strCptyCode,
                double dNotional,
                const std::string& strCurrency);

private:
    CTradeMessageEncoder();
    ~CTradeMessageEncoder();
    CTradeMessageEncoder(const CTradeMessageEncoder&);
    CTradeMessageEncoder& operator=(const CTradeMessageEncoder&);

    CTradeMessageBus* m_pBus;
    bool m_bInitialized;
    static CTradeMessageEncoder* s_pInstance;
};
