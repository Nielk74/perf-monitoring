#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <map>
#include "ITradeEventListener.h"

struct STradeEvent
{
    long m_nEventId;
    int m_nEventType;
    char m_szEventName[64];
    long m_nTradeId;
    long m_nTimestamp;
    void* m_pEventData;
    
    STradeEvent()
        : m_nEventId(0)
        , m_nEventType(0)
        , m_nTradeId(0)
        , m_nTimestamp(0)
        , m_pEventData(NULL)
    {
        memset(m_szEventName, 0, sizeof(m_szEventName));
    }
};

class CTradeEventDispatcher
{
public:
    static CTradeEventDispatcher* GetInstance();
    static void DestroyInstance();
    
    long AddListener(ITradeEventListener* pListener);
    bool RemoveListener(long nListenerId);
    bool RemoveListener(ITradeEventListener* pListener);
    void RemoveAllListeners();
    
    bool DispatchEvent(const STradeEvent& refEvent);
    bool DispatchTradeCreatedEvent(long nTradeId, const char* szTradeType);
    bool DispatchTradeValidatedEvent(long nTradeId, bool bIsValid, const char* szValidationMsg);
    bool DispatchTradeSubmittedEvent(long nTradeId, const char* szSubmissionStatus);
    
    size_t GetListenerCount() const;
    bool HasListeners() const;
    
    void SetSynchronousMode(bool bSynchronous);
    bool GetSynchronousMode() const;
    
    void SetEventQueueSize(size_t nSize);
    size_t GetEventQueueSize() const;

private:
    CTradeEventDispatcher();
    CTradeEventDispatcher(const CTradeEventDispatcher& refOther);
    CTradeEventDispatcher& operator=(const CTradeEventDispatcher& refOther);
    ~CTradeEventDispatcher();
    
    long GenerateNextListenerId();
    
    std::map<long, ITradeEventListener*> m_mapListeners;
    long m_nNextListenerId;
    bool m_bSynchronousMode;
    size_t m_nEventQueueSize;
    
    static CTradeEventDispatcher* s_pInstance;
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // ITradeEventListener* m_arrLegacyListeners[16];
    // int m_nLegacyListenerCount;
    // void* m_pLegacyEventBuffer;
};

// Global accessor for convenience
extern CTradeEventDispatcher* g_pTradeEventDispatcher;

// DEPRECATED: kept for backward compatibility - DO NOT USE
// #define DISPATCH_TRADE_EVENT(evt) g_pTradeEventDispatcher->DispatchEvent(evt)
