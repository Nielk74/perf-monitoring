#pragma warning(disable:4786)
#pragma warning(disable:4100)

#include "legacy/CTradeEventDispatcher.h"
#include <cstring>

CTradeEventDispatcher* CTradeEventDispatcher::s_pInstance = NULL;
CTradeEventDispatcher* g_pTradeEventDispatcher = NULL;

CTradeEventDispatcher* CTradeEventDispatcher::GetInstance()
{
    if (s_pInstance == NULL)
    {
        s_pInstance = new CTradeEventDispatcher();
        g_pTradeEventDispatcher = s_pInstance;
    }
    return s_pInstance;
}

void CTradeEventDispatcher::DestroyInstance()
{
    if (s_pInstance != NULL)
    {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pTradeEventDispatcher = NULL;
    }
}

CTradeEventDispatcher::CTradeEventDispatcher()
    : m_mapListeners()
    , m_nNextListenerId(1)
    , m_bSynchronousMode(true)
    , m_nEventQueueSize(100)
{
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // memset(m_arrLegacyListeners, 0, sizeof(m_arrLegacyListeners));
    // m_nLegacyListenerCount = 0;
    // m_pLegacyEventBuffer = NULL;
}

CTradeEventDispatcher::~CTradeEventDispatcher()
{
    RemoveAllListeners();
}

long CTradeEventDispatcher::AddListener(ITradeEventListener* pListener)
{
    if (pListener == NULL)
    {
        return 0;
    }
    
    for (std::map<long, ITradeEventListener*>::iterator iter = m_mapListeners.begin();
         iter != m_mapListeners.end(); ++iter)
    {
        if (iter->second == pListener)
        {
            return iter->first;
        }
    }
    
    long nListenerId = GenerateNextListenerId();
    m_mapListeners[nListenerId] = pListener;
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // if (m_nLegacyListenerCount < 16)
    // {
    //     m_arrLegacyListeners[m_nLegacyListenerCount++] = pListener;
    // }
    
    return nListenerId;
}

bool CTradeEventDispatcher::RemoveListener(long nListenerId)
{
    std::map<long, ITradeEventListener*>::iterator iter = m_mapListeners.find(nListenerId);
    if (iter == m_mapListeners.end())
    {
        return false;
    }
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // for (int nIdx = 0; nIdx < m_nLegacyListenerCount; ++nIdx)
    // {
    //     if (m_arrLegacyListeners[nIdx] == iter->second)
    //     {
    //         m_arrLegacyListeners[nIdx] = m_arrLegacyListeners[--m_nLegacyListenerCount];
    //         break;
    //     }
    // }
    
    m_mapListeners.erase(iter);
    return true;
}

bool CTradeEventDispatcher::RemoveListener(ITradeEventListener* pListener)
{
    if (pListener == NULL)
    {
        return false;
    }
    
    for (std::map<long, ITradeEventListener*>::iterator iter = m_mapListeners.begin();
         iter != m_mapListeners.end(); ++iter)
    {
        if (iter->second == pListener)
        {
            m_mapListeners.erase(iter);
            return true;
        }
    }
    return false;
}

void CTradeEventDispatcher::RemoveAllListeners()
{
    m_mapListeners.clear();
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // m_nLegacyListenerCount = 0;
}

bool CTradeEventDispatcher::DispatchEvent(const STradeEvent& refEvent)
{
    if (m_mapListeners.empty())
    {
        return true;
    }
    
    for (std::map<long, ITradeEventListener*>::iterator iter = m_mapListeners.begin();
         iter != m_mapListeners.end(); ++iter)
    {
        ITradeEventListener* pListener = iter->second;
        
        switch (refEvent.m_nEventType)
        {
        case 1:
            pListener->OnTradeCreated(refEvent.m_nTradeId, refEvent.m_szEventName);
            break;
        case 2:
            pListener->OnTradeValidated(refEvent.m_nTradeId, true, refEvent.m_szEventName);
            break;
        case 3:
            pListener->OnTradeSubmitted(refEvent.m_nTradeId, refEvent.m_szEventName);
            break;
        default:
            // DEPRECATED: kept for backward compatibility - DO NOT USE
            // pListener->OnLegacyEvent(refEvent);
            break;
        }
    }
    
    return true;
}

bool CTradeEventDispatcher::DispatchTradeCreatedEvent(long nTradeId, const char* szTradeType)
{
    STradeEvent evt;
    evt.m_nEventId = 0;
    evt.m_nEventType = 1;
    evt.m_nTradeId = nTradeId;
    if (szTradeType != NULL)
    {
        strncpy(evt.m_szEventName, szTradeType, sizeof(evt.m_szEventName) - 1);
    }
    evt.m_nTimestamp = 0;
    
    for (std::map<long, ITradeEventListener*>::iterator iter = m_mapListeners.begin();
         iter != m_mapListeners.end(); ++iter)
    {
        iter->second->OnTradeCreated(nTradeId, szTradeType);
    }
    
    return true;
}

bool CTradeEventDispatcher::DispatchTradeValidatedEvent(long nTradeId, bool bIsValid, const char* szValidationMsg)
{
    for (std::map<long, ITradeEventListener*>::iterator iter = m_mapListeners.begin();
         iter != m_mapListeners.end(); ++iter)
    {
        iter->second->OnTradeValidated(nTradeId, bIsValid, szValidationMsg);
    }
    return true;
}

bool CTradeEventDispatcher::DispatchTradeSubmittedEvent(long nTradeId, const char* szSubmissionStatus)
{
    for (std::map<long, ITradeEventListener*>::iterator iter = m_mapListeners.begin();
         iter != m_mapListeners.end(); ++iter)
    {
        iter->second->OnTradeSubmitted(nTradeId, szSubmissionStatus);
    }
    return true;
}

size_t CTradeEventDispatcher::GetListenerCount() const
{
    return m_mapListeners.size();
}

bool CTradeEventDispatcher::HasListeners() const
{
    return !m_mapListeners.empty();
}

void CTradeEventDispatcher::SetSynchronousMode(bool bSynchronous)
{
    m_bSynchronousMode = bSynchronous;
}

bool CTradeEventDispatcher::GetSynchronousMode() const
{
    return m_bSynchronousMode;
}

void CTradeEventDispatcher::SetEventQueueSize(size_t nSize)
{
    m_nEventQueueSize = nSize;
}

size_t CTradeEventDispatcher::GetEventQueueSize() const
{
    return m_nEventQueueSize;
}

long CTradeEventDispatcher::GenerateNextListenerId()
{
    return m_nNextListenerId++;
}
