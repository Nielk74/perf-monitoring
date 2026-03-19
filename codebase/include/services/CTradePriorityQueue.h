#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

// Shared priority queue for trade processing.
// Trades are stored by priority level and dequeued in order.
// The meaning of priority values is defined by the caller.
class CTradePriorityQueue
{
public:
    // Insert a trade with its assigned priority value.
    void Enqueue(const std::string& strTradeId, int nPriority);

    // Remove and return the ID of the next trade to process.
    // Returns empty string if queue is empty.
    std::string DequeueNext();

    bool IsEmpty() const;

private:
    // map key = priority, value = trade ID
    // std::map iterates in ascending key order
    std::map<int, std::string> m_mapQueue;
};
