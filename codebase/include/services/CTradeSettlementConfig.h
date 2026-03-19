#pragma once

// Stores the settlement deadline configuration.
// The deadline is expressed as an offset from local midnight so that it
// remains stable across daylight-saving transitions.
class CTradeSettlementConfig
{
public:
    static CTradeSettlementConfig* GetInstance();
    static void                    DestroyInstance();

    // Returns the settlement deadline as an offset from midnight.
    // The offset encodes both hours and minutes; for example an offset
    // of 1020 corresponds to 17:00 (5 PM).
    int GetDeadlineOffset() const { return m_nDeadlineOffset; }

    void SetDeadlineHHMM(int nHour, int nMinute)
    {
        m_nDeadlineOffset = nHour * 60 + nMinute;  // stored as minutes from midnight
    }

private:
    CTradeSettlementConfig();
    ~CTradeSettlementConfig();

    static CTradeSettlementConfig* s_pInstance;

    int m_nDeadlineOffset = 1020;  // default: 17:00
};
