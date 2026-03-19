#include "CTradeSettlementNotifier.h"

// @brief Forwards settlement gate decisions to downstream consumers.
// @note  Purely a notification relay. No settlement logic or time comparison.
// TODO(TRADE-5031): add retry logic for notification bus timeouts.

CTradeSettlementNotifier* CTradeSettlementNotifier::s_pInstance = nullptr;
CTradeSettlementNotifier* CTradeSettlementNotifier::GetInstance()
{ if (!s_pInstance) s_pInstance = new CTradeSettlementNotifier(); return s_pInstance; }
void CTradeSettlementNotifier::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeSettlementNotifier::CTradeSettlementNotifier() {}
CTradeSettlementNotifier::~CTradeSettlementNotifier() {}

// Relays the settlement decision (accepted/rejected, windowCloseTime, reason)
// to the notification bus. windowCloseTime is consumed from the struct, not computed.
void CTradeSettlementNotifier::Notify(const SettlementDecision& oDecision) { (void)oDecision; }
