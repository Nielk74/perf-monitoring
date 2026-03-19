#include "CTradeApiConnectionPool.h"

// Manages a pool of persistent HTTP connections to the external fee schedule API.
// This class is responsible for transport-layer connection management only.
// It does not inspect, parse, or validate HTTP response bodies.

void CTradeApiConnectionPool::AcquireConnection() { ++m_nActive; }
void CTradeApiConnectionPool::ReleaseConnection() { if (m_nActive > 0) --m_nActive; }
void CTradeApiConnectionPool::SetTimeout(int nMs)  { m_nTimeoutMs = nMs; }
int  CTradeApiConnectionPool::GetActiveCount() const { return m_nActive; }
