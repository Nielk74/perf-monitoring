#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include <set>
#include "ServiceCommon.h"
#include "CTradeLifecycleService.h"
#include "CCounterpartyService.h"

struct SComplianceCheckResult {
    std::string m_strTradeId;
    bool m_bPassed;
    std::string m_strFailureReason;
    bool m_bCounterpartyFound;
    bool m_bSanctioned;

    SComplianceCheckResult()
        : m_bPassed(true)
        , m_bCounterpartyFound(false)
        , m_bSanctioned(false)
    {}
};

class CTradeComplianceService
{
public:
    static CTradeComplianceService* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeLifecycleService* pLifecycleService,
                    CCounterpartyService* pCounterpartyService);

    // Register a counterparty as sanctioned by its CODE (e.g. "CPTY_BAD").
    // The code is stored as-is in the internal sanctions set.
    // RunComplianceCheck will fail any trade whose counterparty code is in this set.
    void AddSanctionedCounterparty(const std::string& strCptyCode);

    // Run sanctions screening for a trade.
    // 1. Reads trade data from lifecycle service → gets m_strCounterpartyCode
    // 2. Loads the full counterparty record via CCounterpartyService
    // 3. Checks the counterparty's identifier against the sanctions set
    // Returns m_bPassed = false if the counterparty is sanctioned.
    ServiceResult<SComplianceCheckResult> RunComplianceCheck(
        const std::string& strTradeId);

    // Returns true if strCptyCode is in the sanctions set.
    bool IsCounterpartySanctioned(const std::string& strCptyCode) const;

private:
    CTradeComplianceService();
    ~CTradeComplianceService();
    CTradeComplianceService(const CTradeComplianceService&);
    CTradeComplianceService& operator=(const CTradeComplianceService&);

    // Sanctioned counterparty CODES — e.g. {"CPTY_BAD", "CPTY_RESTRICTED"}
    std::set<std::string> m_setSanctionedCodes;

    CTradeLifecycleService*  m_pLifecycleService;
    CCounterpartyService*    m_pCounterpartyService;
    bool m_bInitialized;

    static CTradeComplianceService* s_pInstance;
};
