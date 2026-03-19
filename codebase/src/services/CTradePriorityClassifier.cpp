#include "CTradePriorityClassifier.h"

CTradePriorityClassifier* CTradePriorityClassifier::s_pInstance = nullptr;

CTradePriorityClassifier* CTradePriorityClassifier::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePriorityClassifier();
    return s_pInstance;
}

void CTradePriorityClassifier::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradePriorityClassifier::CTradePriorityClassifier()
    : m_pQueue(nullptr)
    , m_bInitialized(false)
{}

CTradePriorityClassifier::~CTradePriorityClassifier() {}

bool CTradePriorityClassifier::Initialize(CTradePriorityQueue* pQueue)
{
    if (!pQueue) return false;
    m_pQueue = pQueue;
    m_bInitialized = true;
    return true;
}

// Assigns a numeric priority and enqueues the trade.
//
// Convention: 1 = URGENT (process first), 5 = ROUTINE (process last).
// Lower integer = higher urgency.
//
// BUG: CTradePriorityScheduler uses the OPPOSITE convention —
// it treats higher integer = higher priority and dequeues in
// DESCENDING order (highest integer first). So URGENT trades
// (stored with key 1) are processed LAST, and ROUTINE trades
// (stored with key 5) are processed FIRST.
//
// Neither service calls the other — both only touch CTradePriorityQueue.
// The mismatch is invisible from either file alone.
void CTradePriorityClassifier::ClassifyAndEnqueue(const std::string& strTradeId,
                                                   const std::string& strUrgency)
{
    if (!m_bInitialized) return;

    int nPriority = 3; // NORMAL default
    if      (strUrgency == "URGENT")  nPriority = 1;
    else if (strUrgency == "HIGH")    nPriority = 2;
    else if (strUrgency == "NORMAL")  nPriority = 3;
    else if (strUrgency == "LOW")     nPriority = 4;
    else if (strUrgency == "ROUTINE") nPriority = 5;

    // Stores 1 for URGENT trades — but scheduler will process this LAST
    m_pQueue->Enqueue(strTradeId, nPriority);
}
