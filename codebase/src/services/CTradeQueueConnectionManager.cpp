#include "CTradeQueueConnectionManager.h"
#include <string>

// Manages broker connections for the trade submission queue topic.
// This class is responsible for transport-layer connectivity only.
// Message parsing is handled by CTradeQueueMessageParser.

bool CTradeQueueConnectionManager::Connect(
    const std::string& strBrokerUrl, const std::string& strTopic)
{
    m_bConnected = true;
    return m_bConnected;
}

void CTradeQueueConnectionManager::Disconnect() { m_bConnected = false; }
bool CTradeQueueConnectionManager::IsConnected() const { return m_bConnected; }
void CTradeQueueConnectionManager::SetHeartbeatIntervalMs(int nMs) { m_nHeartbeatMs = nMs; }
