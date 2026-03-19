#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

class CTradeLifecycleService;
class CNotificationService;

struct STradeAmendmentRecord {
    std::string m_strTradeId;
    std::string m_strAmendmentId;
    double      m_dOldNotional;
    double      m_dNewNotional;
    std::string m_strAmendedBy;
    long        m_nAmendmentTime;
    std::string m_strReason;
    bool        m_bValidated;

    STradeAmendmentRecord()
        : m_dOldNotional(0.0), m_dNewNotional(0.0)
        , m_nAmendmentTime(0), m_bValidated(false) {}
};

class CTradeAmendmentService
{
public:
    static CTradeAmendmentService* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeLifecycleService* pLifecycleService,
                    CNotificationService*   pNotificationService);

    // Amend the notional amount of a trade and re-validate.
    // Returns the updated trade data after successful amendment + validation.
    ServiceResult<STradeAmendmentRecord> AmendTradeNotional(
        const std::string& strTradeId,
        double             dNewNotional,
        const std::string& strAmendedBy,
        const std::string& strReason);

    ServiceResult<STradeAmendmentRecord> GetAmendmentRecord(
        const std::string& strAmendmentId);

    size_t GetAmendmentCount() const;

private:
    CTradeAmendmentService();
    ~CTradeAmendmentService();
    CTradeAmendmentService(const CTradeAmendmentService&);
    CTradeAmendmentService& operator=(const CTradeAmendmentService&);

    std::string GenerateAmendmentId();

    std::map<std::string, STradeAmendmentRecord> m_mapAmendments;
    CTradeLifecycleService* m_pLifecycleService;
    CNotificationService*   m_pNotificationService;
    bool                    m_bInitialized;
    long                    m_nNextAmendmentId;

    static CTradeAmendmentService* s_pInstance;
};
