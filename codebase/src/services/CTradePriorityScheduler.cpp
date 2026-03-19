#include "CTradePriorityScheduler.h"
#include <algorithm>

CTradePriorityScheduler* CTradePriorityScheduler::s_pInstance = nullptr;

CTradePriorityScheduler* CTradePriorityScheduler::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePriorityScheduler();
    return s_pInstance;
}

void CTradePriorityScheduler::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradePriorityScheduler::CTradePriorityScheduler()
    : m_pQueue(nullptr)
    , m_bInitialized(false)
{}

CTradePriorityScheduler::~CTradePriorityScheduler() {}

bool CTradePriorityScheduler::Initialize(CTradePriorityQueue* pQueue)
{
    if (!pQueue) return false;
    m_pQueue = pQueue;
    m_bInitialized = true;
    return true;
}

// Returns all queued trades in processing order: highest integer priority first.
//
// Convention used here: higher integer = more important.
// So we drain the queue and REVERSE the result (since CTradePriorityQueue::DequeueNext
// returns in ascending order, i.e. lowest integer first).
//
// BUG: CTradePriorityClassifier stores URGENT trades with priority 1 (lowest integer).
// After reversing, priority 1 ends up LAST in the processing order.
// ROUTINE trades (priority 5) end up FIRST — the exact opposite of intent.
//
// The fix would be to either:
//   (a) change ClassifyAndEnqueue to use 5=URGENT, 1=ROUTINE (invert scale), or
//   (b) change GetProcessingOrder to NOT reverse (process ascending = low int first = URGENT first)
std::vector<std::string> CTradePriorityScheduler::GetProcessingOrder()
{
    std::vector<std::string> vecOrder;
    if (!m_bInitialized) return vecOrder;

    // Drain queue in ascending order (lowest key first per std::map)
    while (!m_pQueue->IsEmpty())
        vecOrder.push_back(m_pQueue->DequeueNext());

    // Reverse: scheduler thinks highest integer = highest priority = process first
    // This puts ROUTINE (5) first and URGENT (1) last
    std::reverse(vecOrder.begin(), vecOrder.end());
    return vecOrder;
}
