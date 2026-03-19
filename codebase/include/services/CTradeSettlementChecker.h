#pragma once
#include "CTradeSettlementConfig.h"
#include "CTradeTimestampNormalizer.h"

// Determines whether a trade timestamp falls within the permitted
// settlement window.
class CTradeSettlementChecker
{
public:
    static CTradeSettlementChecker* GetInstance();
    static void                     DestroyInstance();

    // Returns true if tTradeTimestamp is strictly before the configured
    // settlement deadline.
    bool IsBeforeDeadline(std::time_t tTradeTimestamp) const;

private:
    CTradeSettlementChecker();
    ~CTradeSettlementChecker();

    static CTradeSettlementChecker* s_pInstance;

    CTradeSettlementConfig*    m_pConfig;
    CTradeTimestampNormalizer* m_pNormalizer;
};
