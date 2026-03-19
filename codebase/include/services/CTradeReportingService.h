#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <vector>
#include <map>
#include "ServiceCommon.h"
#include "CTradeLifecycleService.h"

struct STradeHistoryPage {
    std::vector<STradeLifecycleData> m_vecTrades;
    int  m_nPage;
    int  m_nPageSize;
    int  m_nTotalTrades;
    bool m_bHasMore;

    STradeHistoryPage() : m_nPage(0), m_nPageSize(0), m_nTotalTrades(0), m_bHasMore(false) {}
};

class CTradeReportingService
{
public:
    static CTradeReportingService* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeLifecycleService* pLifecycleService);

    // Returns a page of trade history for a counterparty.
    // nPage is 1-based: pass 1 for the first page, 2 for the second, etc.
    // nPageSize: number of records per page.
    ServiceResult<STradeHistoryPage> GetTradeHistoryPage(
        const std::string& strCptyCode,
        int nPage,
        int nPageSize);

    // Returns total number of trades for a counterparty.
    ServiceResult<int> GetTotalTradeCount(const std::string& strCptyCode);

private:
    CTradeReportingService();
    ~CTradeReportingService();
    CTradeReportingService(const CTradeReportingService&);
    CTradeReportingService& operator=(const CTradeReportingService&);

    std::vector<STradeLifecycleData> GetAllTradesForCounterparty(
        const std::string& strCptyCode);

    CTradeLifecycleService* m_pLifecycleService;
    bool m_bInitialized;

    static CTradeReportingService* s_pInstance;
};
