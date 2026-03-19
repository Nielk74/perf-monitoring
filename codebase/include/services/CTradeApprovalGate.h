#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"
#include "CTradeApprovalLevelConfig.h"

// Enforces approval authority for trade operations.
// Checks whether a given approver has sufficient authority to approve
// a trade in a given category.
//
// Approver authority levels are stored 0-based internally:
//   0 = Junior Trader
//   1 = Senior Trader
//   2 = Trading Desk Head
//   3 = Risk Director
class CTradeApprovalGate
{
public:
    static CTradeApprovalGate* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeApprovalLevelConfig* pConfig);

    // Register an approver with their authority level (0-based).
    // nAuthorityLevel: 0 = Junior, 1 = Senior, 2 = Desk Head, 3 = Risk Director
    void RegisterApprover(const std::string& strApproverId, int nAuthorityLevel);

    // Returns true if the approver has sufficient authority to approve
    // trades in the given category.
    // Reads the required level from CTradeApprovalLevelConfig (1-based)
    // and compares against the approver's registered authority (0-based).
    bool CanApprove(const std::string& strApproverId,
                    const std::string& strTradeCategory) const;

    // Returns the approver's authority level (0-based).
    // Returns -1 if the approver is not registered.
    int GetApproverLevel(const std::string& strApproverId) const;

private:
    CTradeApprovalGate();
    ~CTradeApprovalGate();
    CTradeApprovalGate(const CTradeApprovalGate&);
    CTradeApprovalGate& operator=(const CTradeApprovalGate&);

    // Maps approver ID → authority level (0-based)
    std::map<std::string, int> m_mapApproverLevels;

    CTradeApprovalLevelConfig* m_pConfig;
    bool m_bInitialized;

    static CTradeApprovalGate* s_pInstance;
};
