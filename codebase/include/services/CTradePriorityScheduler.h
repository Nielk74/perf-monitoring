#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <vector>
#include "ServiceCommon.h"
#include "CTradePriorityQueue.h"

// Drains the priority queue and returns trades in processing order.
//
// Priority convention used by THIS service:
//   Higher integer = higher priority (more important, process first)
//   Lower integer  = lower priority  (less important, process last)
//
// The scheduler dequeues in DESCENDING order (highest integer first).
//
// NOTE: CTradePriorityClassifier uses the OPPOSITE convention
// (1 = URGENT = most important). Neither service calls the other —
// they share only CTradePriorityQueue.
class CTradePriorityScheduler
{
public:
    static CTradePriorityScheduler* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradePriorityQueue* pQueue);

    // Returns all queued trade IDs in processing order (highest priority first).
    // Drains the queue.
    std::vector<std::string> GetProcessingOrder();

private:
    CTradePriorityScheduler();
    ~CTradePriorityScheduler();
    CTradePriorityScheduler(const CTradePriorityScheduler&);
    CTradePriorityScheduler& operator=(const CTradePriorityScheduler&);

    CTradePriorityQueue* m_pQueue;
    bool m_bInitialized;
    static CTradePriorityScheduler* s_pInstance;
};
