#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>
#include "ServiceCommon.h"

// Stores the minimum approval level required for each trade category.
//
// Approval levels are 1-based integers:
//   1 = Junior Trader       (can approve small trades)
//   2 = Senior Trader       (can approve medium trades)
//   3 = Trading Desk Head   (can approve large trades)
//   4 = Risk Director       (can approve very large/high-risk trades)
//
// Example: SetRequiredLevel("LARGE_TRADE", 3) means the trade requires
// at least a Trading Desk Head (level 3) to approve it.
class CTradeApprovalLevelConfig
{
public:
    static CTradeApprovalLevelConfig* GetInstance();
    static void DestroyInstance();

    bool Initialize();

    // Set the minimum approval level (1-based) for a trade category.
    // nMinLevel: 1 = Junior Trader, 2 = Senior Trader, 3 = Desk Head, 4 = Risk Director
    void SetRequiredLevel(const std::string& strTradeCategory, int nMinLevel);

    // Returns the minimum required level (1-based) for a trade category.
    // Returns 1 if no specific requirement is configured.
    int GetRequiredLevel(const std::string& strTradeCategory) const;

private:
    CTradeApprovalLevelConfig();
    ~CTradeApprovalLevelConfig();
    CTradeApprovalLevelConfig(const CTradeApprovalLevelConfig&);
    CTradeApprovalLevelConfig& operator=(const CTradeApprovalLevelConfig&);

    // Maps trade category → minimum approval level (1-based)
    std::map<std::string, int> m_mapRequiredLevels;

    bool m_bInitialized;
    static CTradeApprovalLevelConfig* s_pInstance;
};
