#pragma once
#include <string>

// @brief Sends downstream notifications after a settlement gate decision.
// @details Receives a SettlementDecision struct including windowCloseTime.
// Context: this service sits adjacent to the settlement window pipeline.
class CTradeSettlementNotifier
{
public:
    static CTradeSettlementNotifier* GetInstance();
    static void DestroyInstance();
    struct SettlementDecision {
        std::string strTradeId;
        bool bAccepted = false;
        long lWindowCloseTime = 0;
        std::string strRejectReason;
    };
    void Notify(const SettlementDecision& oDecision);
private:
    CTradeSettlementNotifier();
    ~CTradeSettlementNotifier();
    static CTradeSettlementNotifier* s_pInstance;
};
