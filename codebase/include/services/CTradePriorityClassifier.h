#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradePriorityQueue.h"

// Classifies trades by urgency and places them in the priority queue.
//
// Priority convention used by THIS service:
//   1 = URGENT   (must process immediately)
//   2 = HIGH
//   3 = NORMAL
//   4 = LOW
//   5 = ROUTINE  (process when idle)
//
// Lower integer = higher urgency.
// This service has no direct dependency on CTradePriorityScheduler.
class CTradePriorityClassifier
{
public:
    static CTradePriorityClassifier* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradePriorityQueue* pQueue);

    // Classify and enqueue a trade.
    // strUrgency: "URGENT", "HIGH", "NORMAL", "LOW", "ROUTINE"
    void ClassifyAndEnqueue(const std::string& strTradeId,
                            const std::string& strUrgency);

private:
    CTradePriorityClassifier();
    ~CTradePriorityClassifier();
    CTradePriorityClassifier(const CTradePriorityClassifier&);
    CTradePriorityClassifier& operator=(const CTradePriorityClassifier&);

    CTradePriorityQueue* m_pQueue;
    bool m_bInitialized;
    static CTradePriorityClassifier* s_pInstance;
};
