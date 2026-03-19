#include "CTradeQueueConsumer.h"
#include "CTradeQueueMessageParser.h"
#include "CTradePositionLimitChecker.h"
#include <string>

// Consumes trade messages from the processing queue and enforces position limits.
//
// Queue topic "trade.submissions" receives messages from two producers:
//   Legacy system  — field order: COUNTERPARTY_ID|PRODUCT_CODE|NOTIONAL_USD|FEE_BPS
//   NewProvider    — field order: COUNTERPARTY_ID|NOTIONAL_USD|FEE_BPS|PRODUCT_CODE
//
// Note: both producers publish to the same topic. The message parser was written
// against the legacy field schema. NewProvider messages onboarded 2024-08-12.

CTradeQueueConsumer::CTradeQueueConsumer(CTradePositionLimitChecker* pChecker)
    : m_pChecker(pChecker)
{
}

void CTradeQueueConsumer::ProcessMessage(const std::string& strMessage)
{
    const std::string strCpty   = CTradeQueueMessageParser::ParseCounterparty(strMessage);
    const long long   nNotional = CTradeQueueMessageParser::ParseNotional(strMessage);

    if (!m_pChecker->IsWithinLimit(strCpty, nNotional))
    {
        m_oRejectLog << "REJECTED: cpty=" << strCpty
                     << " notional=" << nNotional
                     << std::endl;
    }
}
