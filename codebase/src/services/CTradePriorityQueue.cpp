#include "CTradePriorityQueue.h"

void CTradePriorityQueue::Enqueue(const std::string& strTradeId, int nPriority)
{
    m_mapQueue[nPriority] = strTradeId;
}

std::string CTradePriorityQueue::DequeueNext()
{
    if (m_mapQueue.empty()) return "";
    // std::map iterates in ascending key order — returns lowest key first
    std::map<int, std::string>::iterator it = m_mapQueue.begin();
    std::string strTradeId = it->second;
    m_mapQueue.erase(it);
    return strTradeId;
}

bool CTradePriorityQueue::IsEmpty() const
{
    return m_mapQueue.empty();
}
