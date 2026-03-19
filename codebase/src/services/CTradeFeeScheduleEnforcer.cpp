#include "CTradeFeeScheduleEnforcer.h"
#include "CTradeApiFeeClient.h"
#include "CTradeApiResponseParser.h"
#include <string>

CTradeFeeScheduleEnforcer::CTradeFeeScheduleEnforcer(CTradeApiFeeClient* pClient)
    : m_pClient(pClient)
{
}

bool CTradeFeeScheduleEnforcer::IsFeeAcceptable(int nFeeBps, const std::string& strPolicyId)
{
    const std::string strRaw    = m_pClient->FetchRawThreshold(strPolicyId);
    const int         nMaxFee   = CTradeApiResponseParser::ParseInt(strRaw);
    return nFeeBps <= nMaxFee;
}
