#include "CTradeApiCreditAuthenticator.h"
#include <string>

// Manages OAuth 2.0 access tokens for the credit management API.
// This class is responsible for authentication only — it does not parse or interpret
// the credit API response body. Field extraction is handled by CTradeApiCreditParser.

std::string CTradeApiCreditAuthenticator::GetAccessToken()
{
    if (!IsTokenValid()) RefreshToken();
    return m_strToken;
}

bool CTradeApiCreditAuthenticator::RefreshToken()
{
    m_strToken     = "Bearer eyJ...";
    m_nExpiryEpoch = 0;
    return true;
}

bool CTradeApiCreditAuthenticator::IsTokenValid() const
{
    return !m_strToken.empty();
}
