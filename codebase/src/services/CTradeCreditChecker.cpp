#include "CTradeCreditChecker.h"
#include "CTradeApiCreditClient.h"
#include "CTradeApiCreditParser.h"
#include <string>

CTradeCreditChecker::CTradeCreditChecker(CTradeApiCreditClient* pClient)
    : m_pClient(pClient)
{
}

bool CTradeCreditChecker::IsWithinCredit(
    const std::string& strClientId, int nTradeAmountUsd)
{
    const std::string strResponse  = m_pClient->FetchCreditResponse(strClientId);
    const int         nCreditLimit = CTradeApiCreditParser::ParseCreditLimit(strResponse);
    return nTradeAmountUsd <= nCreditLimit;
}
