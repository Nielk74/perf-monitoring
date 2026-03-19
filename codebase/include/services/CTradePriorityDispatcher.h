#pragma once
#include "CTradePriorityScorer.h"
#include "CTradePriorityComparator.h"
#include <vector>

// Holds a pending set of trades and dispatches them in order of priority.
// DispatchNext() always returns the trade with the highest priority score.
class CTradePriorityDispatcher
{
public:
    static CTradePriorityDispatcher* GetInstance();
    static void                      DestroyInstance();

    void   Submit(const CTrade& oTrade);
    CTrade DispatchNext();
    bool   HasPending() const { return !m_vecPending.empty(); }
    int    PendingCount() const { return static_cast<int>(m_vecPending.size()); }

private:
    CTradePriorityDispatcher();
    ~CTradePriorityDispatcher();

    static CTradePriorityDispatcher* s_pInstance;

    std::vector<CTrade>      m_vecPending;
    CTradePriorityScorer*    m_pScorer;
    CTradePriorityComparator m_oComparator;
};
