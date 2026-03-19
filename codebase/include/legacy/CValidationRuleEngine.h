#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <list>
#include <string>

class CValidationContext;
struct SValidationResult;

class IValidationRule
{
public:
    virtual ~IValidationRule() {}
    virtual bool Evaluate(CValidationContext* pContext) = 0;
    virtual const char* GetRuleName() const = 0;
    virtual int GetPriority() const = 0;
    
protected:
    IValidationRule() {}
};

class CValidationRuleEngine
{
public:
    static CValidationRuleEngine* GetInstance();
    static void DestroyInstance();
    
    bool AddRule(IValidationRule* pRule);
    bool RemoveRule(IValidationRule* pRule);
    void ClearAllRules();
    
    bool EvaluateAllRules(CValidationContext* pContext, SValidationResult& refResult);
    bool EvaluateRulesByPriority(CValidationContext* pContext, SValidationResult& refResult);
    
    size_t GetRuleCount() const;
    IValidationRule* FindRuleByName(const char* szName) const;
    
    void SetStopOnFirstFailure(bool bStop);
    bool GetStopOnFirstFailure() const;
    
    void SetRuleExecutionMode(int nMode);
    int GetRuleExecutionMode() const;

private:
    CValidationRuleEngine();
    CValidationRuleEngine(const CValidationRuleEngine& refOther);
    CValidationRuleEngine& operator=(const CValidationRuleEngine& refOther);
    ~CValidationRuleEngine();
    
    void SortRulesByPriority();
    
    std::list<IValidationRule*> m_lstRules;
    bool m_bStopOnFirstFailure;
    int m_nRuleExecutionMode;
    bool m_bNeedsSorting;
    
    static CValidationRuleEngine* s_pInstance;
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // IValidationRule* m_arrLegacyRules[32];
    // int m_nLegacyRuleCount;
};

// Concrete validation rule implementations
class CSimpleValidationRule : public IValidationRule
{
public:
    CSimpleValidationRule(const char* szName, int nPriority = 0);
    virtual ~CSimpleValidationRule();
    
    virtual bool Evaluate(CValidationContext* pContext);
    virtual const char* GetRuleName() const;
    virtual int GetPriority() const;
    
    void SetCondition(bool (*pfnCondition)(CValidationContext*));

private:
    char m_szRuleName[64];
    int m_nPriority;
    bool (*m_pfnCondition)(CValidationContext*);
};

// DEPRECATED: kept for backward compatibility - DO NOT USE
// class CLegacyValidationRule : public IValidationRule { ... };

extern CValidationRuleEngine* g_pValidationRuleEngine;
