#include "CTradeApprovalGate.h"

CTradeApprovalGate* CTradeApprovalGate::s_pInstance = nullptr;

CTradeApprovalGate* CTradeApprovalGate::GetInstance()
{
    if (!s_pInstance) {
        s_pInstance = new CTradeApprovalGate();
    }
    return s_pInstance;
}

void CTradeApprovalGate::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradeApprovalGate::CTradeApprovalGate()
    : m_pConfig(nullptr)
    , m_bInitialized(false)
{}

CTradeApprovalGate::~CTradeApprovalGate() {}

bool CTradeApprovalGate::Initialize(CTradeApprovalLevelConfig* pConfig)
{
    if (!pConfig) return false;
    m_pConfig = pConfig;
    m_bInitialized = true;
    return true;
}

// Registers an approver with their authority level (0-based).
//   0 = Junior Trader, 1 = Senior Trader, 2 = Desk Head, 3 = Risk Director
void CTradeApprovalGate::RegisterApprover(const std::string& strApproverId,
                                           int nAuthorityLevel)
{
    m_mapApproverLevels[strApproverId] = nAuthorityLevel;
}

// Checks whether an approver has sufficient authority for a trade category.
//
// BUG: CTradeApprovalLevelConfig::GetRequiredLevel returns a 1-BASED level
// (1 = Junior, 2 = Senior, 3 = Desk Head, 4 = Risk Director).
// CTradeApprovalGate stores approver levels as 0-BASED
// (0 = Junior, 1 = Senior, 2 = Desk Head, 3 = Risk Director).
//
// The comparison `nApproverLevel >= nRequiredLevel` directly compares
// a 0-based level against a 1-based level — they are always off by one.
//
// Example: A "HIGH_VALUE" trade requires level 3 (Desk Head, 1-based from config).
//          A Desk Head approver has level 2 (0-based in gate).
//          Check: 2 >= 3 → false → Desk Head CANNOT approve HIGH_VALUE trades.
//          Only a Risk Director (level 3, 0-based) would pass: 3 >= 3 → true.
//
// This means every trade category requires ONE level higher than configured.
// A trade set to require "Senior" (level 2) actually requires "Desk Head" (level 2 in 0-based = 3 in config).
//
// Fix: convert one of the scales before comparing:
//   nApproverLevel >= (nRequiredLevel - 1)   // convert config to 0-based
//   or: (nApproverLevel + 1) >= nRequiredLevel  // convert gate to 1-based
bool CTradeApprovalGate::CanApprove(const std::string& strApproverId,
                                     const std::string& strTradeCategory) const
{
    if (!m_bInitialized || !m_pConfig) return false;

    auto it = m_mapApproverLevels.find(strApproverId);
    if (it == m_mapApproverLevels.end()) return false;  // unregistered approver

    int nApproverLevel = it->second;  // 0-based: 0=Junior, 1=Senior, 2=DeskHead, 3=Director

    // Reads 1-based level from config: 1=Junior, 2=Senior, 3=DeskHead, 4=Director
    int nRequiredLevel = m_pConfig->GetRequiredLevel(strTradeCategory);

    // BUG: direct comparison of 0-based vs 1-based values.
    // A Desk Head (0-based: 2) compared to config requirement 3 (1-based: Desk Head) → 2 >= 3 → false.
    // The required authority is always one level higher than intended.
    return nApproverLevel >= nRequiredLevel;
}

int CTradeApprovalGate::GetApproverLevel(const std::string& strApproverId) const
{
    auto it = m_mapApproverLevels.find(strApproverId);
    if (it != m_mapApproverLevels.end()) return it->second;
    return -1;
}
