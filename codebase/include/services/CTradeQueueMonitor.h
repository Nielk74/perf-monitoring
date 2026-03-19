#pragma once
#include <string>

// @brief Monitors the live trade execution queue and emits operational metrics.
// @details Tracks queue depth, head-of-queue age, and per-priority throughput.
// Context: this service sits adjacent to the priority queue pipeline.
class CTradeQueueMonitor
{
public:
    static CTradeQueueMonitor* GetInstance();
    static void                DestroyInstance();

    // Returns current queue depth across all priority bands.
    int  GetQueueDepth() const;
    // Returns age in milliseconds of the oldest pending trade.
    long GetHeadAgeMs()  const;
    // Emits metrics snapshot to the configured sink.
    void EmitMetrics()   const;

private:
    CTradeQueueMonitor();
    ~CTradeQueueMonitor();
    static CTradeQueueMonitor* s_pInstance;
    static constexpr int kMaxQueueDepth = 10000;
};
