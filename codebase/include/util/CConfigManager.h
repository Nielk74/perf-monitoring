#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <map>

struct SConfigValue
{
    char m_szStringValue[256];
    int m_nIntValue;
    double m_dDoubleValue;
    bool m_bBoolValue;
    bool m_bHasValue;
    
    SConfigValue()
        : m_nIntValue(0)
        , m_dDoubleValue(0.0)
        , m_bBoolValue(false)
        , m_bHasValue(false)
    {
        memset(m_szStringValue, 0, sizeof(m_szStringValue));
    }
};

class CConfigManager
{
public:
    static CConfigManager* GetInstance();
    static void DestroyInstance();
    
    bool LoadFromFile(const char* szFilePath);
    bool SaveToFile(const char* szFilePath);
    bool Reload();
    
    const char* GetStringValue(const char* szKey, const char* szDefault = "");
    int GetIntValue(const char* szKey, int nDefault = 0);
    double GetDoubleValue(const char* szKey, double dDefault = 0.0);
    bool GetBoolValue(const char* szKey, bool bDefault = false);
    
    void SetStringValue(const char* szKey, const char* szValue);
    void SetIntValue(const char* szKey, int nValue);
    void SetDoubleValue(const char* szKey, double dValue);
    void SetBoolValue(const char* szKey, bool bValue);
    
    bool HasKey(const char* szKey) const;
    void RemoveKey(const char* szKey);
    void ClearAll();
    
    size_t GetKeyCount() const;
    
    void SetConfigFilePath(const char* szPath);
    const char* GetConfigFilePath() const;
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // static const int s_nMaxEntries = 256;
    // SConfigValue m_arrLegacyValues[256];
    // char m_arrLegacyKeys[256][64];
    // int m_nLegacyCount;

private:
    CConfigManager();
    CConfigManager(const CConfigManager& refOther);
    CConfigManager& operator=(const CConfigManager& refOther);
    ~CConfigManager();
    
    bool ParseLine(const char* szLine, std::string& refKey, std::string& refValue);
    void UpdateCachedValues(SConfigValue& refValue, const std::string& refStrValue);
    
    std::map<std::string, SConfigValue> m_mapConfigValues;
    char m_szConfigFilePath[512];
    bool m_bLoaded;
    
    static CConfigManager* s_pInstance;
};

extern CConfigManager* g_pConfigManager;
