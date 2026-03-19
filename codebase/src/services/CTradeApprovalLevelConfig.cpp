#include "CTradeApprovalLevelConfig.h"

CTradeApprovalLevelConfig* CTradeApprovalLevelConfig::s_pInstance = nullptr;

CTradeApprovalLevelConfig* CTradeApprovalLevelConfig::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeApprovalLevelConfig();
    }
    return s_pInstance;
}

void CTradeApprovalLevelConfig::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeApprovalLevelConfig::CTradeApprovalLevelConfig()
    : m_bInitialized(false)
{}

CTradeApprovalLevelConfig::~CTradeApprovalLevelConfig() {}

bool CTradeApprovalLevelConfig::Initialize()
{
    m_bInitialized = true;
    return true;
}

// Stores the minimum approval level required for a trade category.
// Levels are 1-based:
//   1 = Junior Trader, 2 = Senior Trader, 3 = Desk Head, 4 = Risk Director
//
// Example: SetRequiredLevel("HIGH_VALUE", 3)
//   → "HIGH_VALUE" trades require at least level 3 (Desk Head) approval.
void CTradeApprovalLevelConfig::SetRequiredLevel(const std::string& strTradeCategory,
                                                   int nMinLevel)
{
    m_mapRequiredLevels[strTradeCategory] = nMinLevel;
}

// Returns the minimum required approval level (1-based) for a trade category.
int CTradeApprovalLevelConfig::GetRequiredLevel(const std::string& strTradeCategory) const
{
    auto it = m_mapRequiredLevels.find(strTradeCategory);
    if (it != m_mapRequiredLevels.end()) return it->second;
    return 1;  // default: any approver can approve
}
