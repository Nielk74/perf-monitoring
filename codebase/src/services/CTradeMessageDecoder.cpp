#include "CTradeMessageDecoder.h"
#include <sstream>
#include <vector>

CTradeMessageDecoder* CTradeMessageDecoder::s_pInstance = nullptr;

CTradeMessageDecoder* CTradeMessageDecoder::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeMessageDecoder();
    return s_pInstance;
}

void CTradeMessageDecoder::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeMessageDecoder::CTradeMessageDecoder()
    : m_pBus(nullptr), m_bInitialized(false) {}

CTradeMessageDecoder::~CTradeMessageDecoder() {}

bool CTradeMessageDecoder::Initialize(CTradeMessageBus* pBus)
{
    if (!pBus) return false;
    m_pBus = pBus;
    m_bInitialized = true;
    return true;
}

// Splits a pipe-delimited string into tokens.
static std::vector<std::string> Split(const std::string& strInput, char cDelim)
{
    std::vector<std::string> vecTokens;
    std::istringstream iss(strInput);
    std::string strToken;
    while (std::getline(iss, strToken, cDelim))
        vecTokens.push_back(strToken);
    return vecTokens;
}

// Deserializes a pipe-delimited message from the bus.
//
// Expected field order:
//   field 0: tradeId
//   field 1: notional    ← BUG: encoder stores cptyCode here
//   field 2: cptyCode    ← BUG: encoder stores notional here
//   field 3: currency
//
// The result: refTrade.m_strCptyCode receives the notional string,
//             refTrade.m_dNotional receives atof(cptyCode) = 0.0 (string-to-double of "CPTY_001" = 0)
//
// BUG: fields 1 and 2 are swapped relative to CTradeMessageEncoder.
// CTradeMessageEncoder has no call chain to CTradeMessageDecoder.
// The protocol contract is implicit — agreed only through documentation,
// never enforced in code. The mismatch requires reading both files.
bool CTradeMessageDecoder::Decode(const std::string& strTradeId,
                                   SDecodedTrade& refTrade) const
{
    if (!m_bInitialized) return false;

    std::string strMessage;
    if (!m_pBus->Consume(strTradeId, strMessage)) return false;

    std::vector<std::string> vecFields = Split(strMessage, '|');
    if (vecFields.size() < 4) return false;

    refTrade.m_strTradeId  = vecFields[0];
    refTrade.m_dNotional   = std::atof(vecFields[1].c_str());  // reads cptyCode as notional → 0.0
    refTrade.m_strCptyCode = vecFields[2];                      // reads notional string as cptyCode
    refTrade.m_strCurrency = vecFields[3];
    return true;
}
