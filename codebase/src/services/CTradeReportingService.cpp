#include "CTradeReportingService.h"
#include <algorithm>
#include <sstream>

CTradeReportingService* CTradeReportingService::s_pInstance = nullptr;

CTradeReportingService* CTradeReportingService::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeReportingService();
    }
    return s_pInstance;
}

void CTradeReportingService::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeReportingService::CTradeReportingService()
    : m_pLifecycleService(nullptr)
    , m_bInitialized(false)
{}

CTradeReportingService::~CTradeReportingService() {}

bool CTradeReportingService::Initialize(CTradeLifecycleService* pLifecycleService)
{
    if (!pLifecycleService) return false;
    m_pLifecycleService = pLifecycleService;
    m_bInitialized = true;
    return true;
}

// Returns a page of trade history.
// nPage is documented and expected to be 1-based (1 = first page).
//
// BUG: the offset is computed as nPage * nPageSize instead of (nPage-1) * nPageSize.
// For nPage=1, nPageSize=10:
//   actual offset   = 1 * 10 = 10  → returns records 10..19 (skips the first 10)
//   expected offset = 0 * 10 =  0  → should return records 0..9
//
// The first page always contains the most-recently created trades (index 0 = newest).
// Callers requesting nPage=1 will never see their most recent trades.
ServiceResult<STradeHistoryPage> CTradeReportingService::GetTradeHistoryPage(
    const std::string& strCptyCode,
    int nPage,
    int nPageSize)
{
    if (!m_bInitialized) {
        return ServiceResult<STradeHistoryPage>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "ReportingService not initialized");
    }

    if (nPage < 1 || nPageSize < 1) {
        return ServiceResult<STradeHistoryPage>::Failure(
            SERVICES_ERRORCODE_UNKNOWN_ERROR,
            "nPage must be >= 1 and nPageSize must be >= 1");
    }

    std::vector<STradeLifecycleData> allTrades =
        GetAllTradesForCounterparty(strCptyCode);

    int nTotal = (int)allTrades.size();

    // BUG: should be (nPage - 1) * nPageSize
    int nOffset = nPage * nPageSize;

    STradeHistoryPage page;
    page.m_nPage       = nPage;
    page.m_nPageSize   = nPageSize;
    page.m_nTotalTrades = nTotal;

    if (nOffset >= nTotal) {
        page.m_bHasMore = false;
        return ServiceResult<STradeHistoryPage>::Success(page);
    }

    int nEnd = std::min(nTotal, nOffset + nPageSize);
    page.m_bHasMore = (nEnd < nTotal);

    for (int i = nOffset; i < nEnd; i++) {
        page.m_vecTrades.push_back(allTrades[i]);
    }

    return ServiceResult<STradeHistoryPage>::Success(page);
}

ServiceResult<int> CTradeReportingService::GetTotalTradeCount(
    const std::string& strCptyCode)
{
    if (!m_bInitialized) {
        return ServiceResult<int>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE, "Not initialized");
    }
    std::vector<STradeLifecycleData> trades =
        GetAllTradesForCounterparty(strCptyCode);
    return ServiceResult<int>::Success((int)trades.size());
}

std::vector<STradeLifecycleData> CTradeReportingService::GetAllTradesForCounterparty(
    const std::string& strCptyCode)
{
    std::vector<STradeLifecycleData> result;
    if (!m_pLifecycleService) return result;

    // Walk all trades in the lifecycle store and collect those matching cpty
    size_t nTotal = m_pLifecycleService->GetTradeCount();
    // NOTE: this is O(n) — acceptable for the reporting use-case
    // Iterate by numeric ID range to recover trades in creation order
    for (long nId = 1; nId <= (long)nTotal + 100; nId++) {
        auto stateResult = m_pLifecycleService->GetTradeState(nId);
        if (stateResult.IsFailure()) continue;
        auto dataResult = m_pLifecycleService->GetTradeData(nId);
        if (dataResult.IsFailure()) continue;
        const STradeLifecycleData& data = dataResult.GetValue();
        if (data.m_strCounterpartyCode == strCptyCode) {
            result.push_back(data);
        }
    }
    return result;
}
