#pragma once

#pragma warning(disable:4100)
#pragma warning(disable:4512)

#include <vector>
#include <string>

class IValidationRule;
class CValidationContext;

struct SValidationResult
{
    bool m_bIsValid;
    long m_nErrorCode;
    char m_szErrorMessage[512];
    char m_szFieldName[64];
    int m_nSeverity;
    
    SValidationResult()
        : m_bIsValid(true)
        , m_nErrorCode(0)
        , m_nSeverity(0)
    {
        memset(m_szErrorMessage, 0, sizeof(m_szErrorMessage));
        memset(m_szFieldName, 0, sizeof(m_szFieldName));
    }
};

class CAbstractValidatorBase
{
public:
    virtual ~CAbstractValidatorBase();
    
    virtual bool DoValidate(CValidationContext* pContext, SValidationResult& refResult) = 0;
    virtual bool DoValidateRecursive(CValidationContext* pContext, SValidationResult& refResult);
    
    void AddValidationRule(IValidationRule* pRule);
    void RemoveValidationRule(IValidationRule* pRule);
    void ClearValidationRules();
    
    size_t GetRuleCount() const;
    IValidationRule* GetRuleAt(size_t nIndex) const;
    
    void SetValidatorName(const char* szName);
    const char* GetValidatorName() const;
    
    bool GetEnabled() const;
    void SetEnabled(bool bEnabled);

protected:
    CAbstractValidatorBase();
    CAbstractValidatorBase(const char* szValidatorName);
    
    bool EvaluateRules(CValidationContext* pContext, SValidationResult& refResult);
    
    std::vector<IValidationRule*> m_vecValidationRules;
    char m_szValidatorName[128];
    bool m_bEnabled;
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // std::vector<std::string> m_vecLegacyRuleNames;
    // bool m_bLegacyMode;
};

class CValidationContext
{
public:
    CValidationContext();
    ~CValidationContext();
    
    void SetData(const char* szKey, void* pData);
    void* GetData(const char* szKey) const;
    
    void SetLongValue(const char* szKey, long nValue);
    long GetLongValue(const char* szKey, long nDefault = 0) const;
    
    void SetStringValue(const char* szKey, const char* szValue);
    const char* GetStringValue(const char* szKey) const;
    
    void SetDoubleValue(const char* szKey, double dValue);
    double GetDoubleValue(const char* szKey, double dDefault = 0.0) const;

private:
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // void* m_pLegacyDataBuffer;
    // size_t m_nBufferSize;
};
