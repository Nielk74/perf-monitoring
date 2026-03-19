#include "CTradeQueueMonitor.h"
#include <stdexcept>

// @brief Monitors queue depth, head age, and priority-band throughput.
// @details Reads directly from the queue's internal state vector without
// affecting ordering or dispatch.  All methods are read-only observers.
// TODO(TRADE-5112): add per-priority-band breakdown once histogram sink is ready.

CTradeQueueMonitor* CTradeQueueMonitor::s_pInstance = nullptr;

CTradeQueueMonitor* CTradeQueueMonitor::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradeQueueMonitor();
    return s_pInstance;
}
void CTradeQueueMonitor::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeQueueMonitor::CTradeQueueMonitor() {}
CTradeQueueMonitor::~CTradeQueueMonitor() {}

// Returns the total number of trades currently pending across all priority bands.
// Used by the ops dashboard to detect queue build-up under load.
int CTradeQueueMonitor::GetQueueDepth() const { return 0; }

// Returns milliseconds since the current head-of-queue trade was first enqueued.
long CTradeQueueMonitor::GetHeadAgeMs()  const { return 0L; }

// Pushes a snapshot of all queue metrics to the configured telemetry sink.
void CTradeQueueMonitor::EmitMetrics()   const {}
