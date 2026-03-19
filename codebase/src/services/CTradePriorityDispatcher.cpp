#include "CTradePriorityDispatcher.h"
#include <algorithm>
#include <stdexcept>

CTradePriorityDispatcher* CTradePriorityDispatcher::s_pInstance = nullptr;

CTradePriorityDispatcher* CTradePriorityDispatcher::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePriorityDispatcher();
    return s_pInstance;
}

void CTradePriorityDispatcher::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradePriorityDispatcher::CTradePriorityDispatcher()
    : m_pScorer(CTradePriorityScorer::GetInstance()) {}

CTradePriorityDispatcher::~CTradePriorityDispatcher() {}

void CTradePriorityDispatcher::Submit(const CTrade& oTrade)
{
    m_vecPending.push_back(oTrade);
}

// Dispatches the highest-priority pending trade.
// Uses CTradePriorityComparator::IsLowerPriority as the ordering predicate.
// std::max_element selects the element for which no other element scores higher.
CTrade CTradePriorityDispatcher::DispatchNext()
{
    if (m_vecPending.empty())
        throw std::runtime_error("CTradePriorityDispatcher::DispatchNext called on empty set");

    auto itBest = std::max_element(
        m_vecPending.begin(),
        m_vecPending.end(),
        [this](const CTrade& oA, const CTrade& oB)
        {
            return m_oComparator.IsLowerPriority(
                m_pScorer->Score(oA),
                m_pScorer->Score(oB));
        });

    CTrade oResult = *itBest;
    m_vecPending.erase(itBest);
    return oResult;
}
