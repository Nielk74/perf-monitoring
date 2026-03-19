#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradeMessageBus.h"

// Decoded trade data populated by CTradeMessageDecoder.
struct SDecodedTrade
{
    std::string m_strTradeId;
    std::string m_strCptyCode;
    double      m_dNotional;
    std::string m_strCurrency;
};

// Consumes a pipe-delimited message from the bus and deserializes it.
//
// Wire format expected by THIS service:
//   field 0: tradeId
//   field 1: notional      ← BUG: encoder puts cptyCode here
//   field 2: cptyCode       ← BUG: encoder puts notional here
//   field 3: currency
//
// Fields 1 and 2 are swapped relative to CTradeMessageEncoder's format.
// Neither service calls the other — both only interact with CTradeMessageBus.
// The mismatch is invisible from either file alone.
class CTradeMessageDecoder
{
public:
    static CTradeMessageDecoder* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeMessageBus* pBus);

    // Decode the message for a trade ID into a SDecodedTrade struct.
    // Returns false if no message exists for this trade ID.
    bool Decode(const std::string& strTradeId, SDecodedTrade& refTrade) const;

private:
    CTradeMessageDecoder();
    ~CTradeMessageDecoder();
    CTradeMessageDecoder(const CTradeMessageDecoder&);
    CTradeMessageDecoder& operator=(const CTradeMessageDecoder&);

    CTradeMessageBus* m_pBus;
    bool m_bInitialized;
    static CTradeMessageDecoder* s_pInstance;
};
