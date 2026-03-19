#pragma warning(disable:4786)
#pragma warning(disable:4100)

#include "legacy/CValidationRuleEngine.h"
#include "legacy/CAbstractValidatorBase.h"
#include <algorithm>
#include <cstring>

CValidationRuleEngine* CValidationRuleEngine::s_pInstance = NULL;
CValidationRuleEngine* g_pValidationRuleEngine = NULL;

CValidationRuleEngine* CValidationRuleEngine::GetInstance()
{
    if (s_pInstance == NULL)
    {
        s_pInstance = new CValidationRuleEngine();
        g_pValidationRuleEngine = s_pInstance;
    }
    return s_pInstance;
}

void CValidationRuleEngine::DestroyInstance()
{
    if (s_pInstance != NULL)
    {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pValidationRuleEngine = NULL;
    }
}

CValidationRuleEngine::CValidationRuleEngine()
    : m_lstRules()
    , m_bStopOnFirstFailure(true)
    , m_nRuleExecutionMode(0)
    , m_bNeedsSorting(false)
{
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // memset(m_arrLegacyRules, 0, sizeof(m_arrLegacyRules));
    // m_nLegacyRuleCount = 0;
}

CValidationRuleEngine::~CValidationRuleEngine()
{
    m_lstRules.clear();
}

bool CValidationRuleEngine::AddRule(IValidationRule* pRule)
{
    if (pRule == NULL)
    {
        return false;
    }
    
    for (std::list<IValidationRule*>::iterator iter = m_lstRules.begin();
         iter != m_lstRules.end(); ++iter)
    {
        if (*iter == pRule)
        {
            return false;
        }
    }
    
    m_lstRules.push_back(pRule);
    m_bNeedsSorting = true;
    return true;
}

bool CValidationRuleEngine::RemoveRule(IValidationRule* pRule)
{
    if (pRule == NULL)
    {
        return false;
    }
    
    for (std::list<IValidationRule*>::iterator iter = m_lstRules.begin();
         iter != m_lstRules.end(); ++iter)
    {
        if (*iter == pRule)
        {
            m_lstRules.erase(iter);
            return true;
        }
    }
    return false;
}

void CValidationRuleEngine::ClearAllRules()
{
    m_lstRules.clear();
    m_bNeedsSorting = false;
}

bool CValidationRuleEngine::EvaluateAllRules(CValidationContext* pContext, SValidationResult& refResult)
{
    if (m_bNeedsSorting)
    {
        SortRulesByPriority();
    }
    
    refResult.m_bIsValid = true;
    
    for (std::list<IValidationRule*>::iterator iter = m_lstRules.begin();
         iter != m_lstRules.end(); ++iter)
    {
        IValidationRule* pRule = *iter;
        
        // DEPRECATED: kept for backward compatibility - DO NOT USE
        // if (pRule->GetRuleName() == NULL) continue;
        
        if (!pRule->Evaluate(pContext))
        {
            refResult.m_bIsValid = false;
            strncpy(refResult.m_szErrorMessage, "Validation rule failed", sizeof(refResult.m_szErrorMessage) - 1);
            strncpy(refResult.m_szFieldName, pRule->GetRuleName(), sizeof(refResult.m_szFieldName) - 1);
            
            if (m_bStopOnFirstFailure)
            {
                return false;
            }
        }
    }
    
    return refResult.m_bIsValid;
}

bool CValidationRuleEngine::EvaluateRulesByPriority(CValidationContext* pContext, SValidationResult& refResult)
{
    SortRulesByPriority();
    return EvaluateAllRules(pContext, refResult);
}

size_t CValidationRuleEngine::GetRuleCount() const
{
    return m_lstRules.size();
}

IValidationRule* CValidationRuleEngine::FindRuleByName(const char* szName) const
{
    if (szName == NULL)
    {
        return NULL;
    }
    
    for (std::list<IValidationRule*>::const_iterator iter = m_lstRules.begin();
         iter != m_lstRules.end(); ++iter)
    {
        if (strcmp((*iter)->GetRuleName(), szName) == 0)
        {
            return *iter;
        }
    }
    return NULL;
}

void CValidationRuleEngine::SetStopOnFirstFailure(bool bStop)
{
    m_bStopOnFirstFailure = bStop;
}

bool CValidationRuleEngine::GetStopOnFirstFailure() const
{
    return m_bStopOnFirstFailure;
}

void CValidationRuleEngine::SetRuleExecutionMode(int nMode)
{
    m_nRuleExecutionMode = nMode;
}

int CValidationRuleEngine::GetRuleExecutionMode() const
{
    return m_nRuleExecutionMode;
}

void CValidationRuleEngine::SortRulesByPriority()
{
    m_lstRules.sort([](const IValidationRule* pLeft, const IValidationRule* pRight) {
        return pLeft->GetPriority() < pRight->GetPriority();
    });
    m_bNeedsSorting = false;
}

// CSimpleValidationRule implementation
CSimpleValidationRule::CSimpleValidationRule(const char* szName, int nPriority)
    : m_nPriority(nPriority)
    , m_pfnCondition(NULL)
{
    memset(m_szRuleName, 0, sizeof(m_szRuleName));
    if (szName != NULL)
    {
        strncpy(m_szRuleName, szName, sizeof(m_szRuleName) - 1);
    }
}

CSimpleValidationRule::~CSimpleValidationRule()
{
}

bool CSimpleValidationRule::Evaluate(CValidationContext* pContext)
{
    if (m_pfnCondition == NULL)
    {
        return true;
    }
    return m_pfnCondition(pContext);
}

const char* CSimpleValidationRule::GetRuleName() const
{
    return m_szRuleName;
}

int CSimpleValidationRule::GetPriority() const
{
    return m_nPriority;
}

void CSimpleValidationRule::SetCondition(bool (*pfnCondition)(CValidationContext*))
{
    m_pfnCondition = pfnCondition;
}
