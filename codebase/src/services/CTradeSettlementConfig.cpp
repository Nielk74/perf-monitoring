#include "CTradeSettlementConfig.h"

CTradeSettlementConfig* CTradeSettlementConfig::s_pInstance = nullptr;

CTradeSettlementConfig* CTradeSettlementConfig::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeSettlementConfig();
    return s_pInstance;
}

void CTradeSettlementConfig::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeSettlementConfig::CTradeSettlementConfig() {}
CTradeSettlementConfig::~CTradeSettlementConfig() {}
