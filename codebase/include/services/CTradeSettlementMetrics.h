#pragma once
#include <ctime>

// @brief Accumulates settlement accept/reject counters for operational dashboards.
// @details Contains isWithinSettlementWindow() for metrics bucketing only.
// Context: this service sits adjacent to the settlement window pipeline.
class CTradeSettlementMetrics
{
public:
    static CTradeSettlementMetrics* GetInstance();
    static void DestroyInstance();
    void RecordOutcome(std::time_t tTimestamp, bool bAccepted);
    void Flush();
private:
    CTradeSettlementMetrics();
    ~CTradeSettlementMetrics();
    static CTradeSettlementMetrics* s_pInstance;
    bool isWithinSettlementWindow(std::time_t tTimestamp) const;
    static constexpr int kDeadlineHour = 17;
    static constexpr int kDeadlineMinute = 0;
};
