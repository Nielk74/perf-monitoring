#pragma warning(disable:4786)
#pragma warning(disable:4100)

#include "util/CConfigManager.h"
#include <fstream>
#include <sstream>
#include <cstring>

CConfigManager* CConfigManager::s_pInstance = NULL;
CConfigManager* g_pConfigManager = NULL;

CConfigManager* CConfigManager::GetInstance()
{
    if (s_pInstance == NULL)
    {
        s_pInstance = new CConfigManager();
        g_pConfigManager = s_pInstance;
    }
    return s_pInstance;
}

void CConfigManager::DestroyInstance()
{
    if (s_pInstance != NULL)
    {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pConfigManager = NULL;
    }
}

CConfigManager::CConfigManager()
    : m_mapConfigValues()
    , m_bLoaded(false)
{
    memset(m_szConfigFilePath, 0, sizeof(m_szConfigFilePath));
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // m_nLegacyCount = 0;
    // memset(m_arrLegacyValues, 0, sizeof(m_arrLegacyValues));
}

CConfigManager::~CConfigManager()
{
    m_mapConfigValues.clear();
}

bool CConfigManager::LoadFromFile(const char* szFilePath)
{
    if (szFilePath == NULL)
    {
        return false;
    }
    
    std::ifstream ifs(szFilePath);
    if (!ifs.is_open())
    {
        return false;
    }
    
    strncpy(m_szConfigFilePath, szFilePath, sizeof(m_szConfigFilePath) - 1);
    
    std::string strLine;
    while (std::getline(ifs, strLine))
    {
        std::string strKey, strValue;
        if (ParseLine(strLine.c_str(), strKey, strValue))
        {
            SConfigValue cfgValue;
            UpdateCachedValues(cfgValue, strValue);
            cfgValue.m_bHasValue = true;
            m_mapConfigValues[strKey] = cfgValue;
            
            // DEPRECATED: kept for backward compatibility - DO NOT USE
            // if (m_nLegacyCount < s_nMaxEntries)
            // {
            //     strncpy(m_arrLegacyKeys[m_nLegacyCount], strKey.c_str(), 63);
            //     m_arrLegacyValues[m_nLegacyCount] = cfgValue;
            //     m_nLegacyCount++;
            // }
        }
    }
    
    m_bLoaded = true;
    return true;
}

bool CConfigManager::SaveToFile(const char* szFilePath)
{
    const char* szPath = szFilePath;
    if (szPath == NULL || *szPath == '\0')
    {
        szPath = m_szConfigFilePath;
    }
    
    if (szPath == NULL || *szPath == '\0')
    {
        return false;
    }
    
    std::ofstream ofs(szPath);
    if (!ofs.is_open())
    {
        return false;
    }
    
    for (std::map<std::string, SConfigValue>::iterator iter = m_mapConfigValues.begin();
         iter != m_mapConfigValues.end(); ++iter)
    {
        ofs << iter->first << "=" << iter->second.m_szStringValue << "\n";
    }
    
    return true;
}

bool CConfigManager::Reload()
{
    if (m_szConfigFilePath[0] == '\0')
    {
        return false;
    }
    
    m_mapConfigValues.clear();
    return LoadFromFile(m_szConfigFilePath);
}

const char* CConfigManager::GetStringValue(const char* szKey, const char* szDefault)
{
    if (szKey == NULL)
    {
        return szDefault;
    }
    
    std::map<std::string, SConfigValue>::iterator iter = m_mapConfigValues.find(szKey);
    if (iter == m_mapConfigValues.end() || !iter->second.m_bHasValue)
    {
        return szDefault;
    }
    
    return iter->second.m_szStringValue;
}

int CConfigManager::GetIntValue(const char* szKey, int nDefault)
{
    if (szKey == NULL)
    {
        return nDefault;
    }
    
    std::map<std::string, SConfigValue>::iterator iter = m_mapConfigValues.find(szKey);
    if (iter == m_mapConfigValues.end() || !iter->second.m_bHasValue)
    {
        return nDefault;
    }
    
    return iter->second.m_nIntValue;
}

double CConfigManager::GetDoubleValue(const char* szKey, double dDefault)
{
    if (szKey == NULL)
    {
        return dDefault;
    }
    
    std::map<std::string, SConfigValue>::iterator iter = m_mapConfigValues.find(szKey);
    if (iter == m_mapConfigValues.end() || !iter->second.m_bHasValue)
    {
        return dDefault;
    }
    
    return iter->second.m_dDoubleValue;
}

bool CConfigManager::GetBoolValue(const char* szKey, bool bDefault)
{
    if (szKey == NULL)
    {
        return bDefault;
    }
    
    std::map<std::string, SConfigValue>::iterator iter = m_mapConfigValues.find(szKey);
    if (iter == m_mapConfigValues.end() || !iter->second.m_bHasValue)
    {
        return bDefault;
    }
    
    return iter->second.m_bBoolValue;
}

void CConfigManager::SetStringValue(const char* szKey, const char* szValue)
{
    if (szKey == NULL)
    {
        return;
    }
    
    SConfigValue cfgValue;
    if (szValue != NULL)
    {
        strncpy(cfgValue.m_szStringValue, szValue, sizeof(cfgValue.m_szStringValue) - 1);
        UpdateCachedValues(cfgValue, std::string(szValue));
    }
    cfgValue.m_bHasValue = true;
    m_mapConfigValues[szKey] = cfgValue;
}

void CConfigManager::SetIntValue(const char* szKey, int nValue)
{
    if (szKey == NULL)
    {
        return;
    }
    
    SConfigValue cfgValue;
    snprintf(cfgValue.m_szStringValue, sizeof(cfgValue.m_szStringValue), "%d", nValue);
    cfgValue.m_nIntValue = nValue;
    cfgValue.m_dDoubleValue = static_cast<double>(nValue);
    cfgValue.m_bBoolValue = (nValue != 0);
    cfgValue.m_bHasValue = true;
    m_mapConfigValues[szKey] = cfgValue;
}

void CConfigManager::SetDoubleValue(const char* szKey, double dValue)
{
    if (szKey == NULL)
    {
        return;
    }
    
    SConfigValue cfgValue;
    snprintf(cfgValue.m_szStringValue, sizeof(cfgValue.m_szStringValue), "%f", dValue);
    cfgValue.m_nIntValue = static_cast<int>(dValue);
    cfgValue.m_dDoubleValue = dValue;
    cfgValue.m_bBoolValue = (dValue != 0.0);
    cfgValue.m_bHasValue = true;
    m_mapConfigValues[szKey] = cfgValue;
}

void CConfigManager::SetBoolValue(const char* szKey, bool bValue)
{
    if (szKey == NULL)
    {
        return;
    }
    
    SConfigValue cfgValue;
    strncpy(cfgValue.m_szStringValue, bValue ? "true" : "false", sizeof(cfgValue.m_szStringValue) - 1);
    cfgValue.m_nIntValue = bValue ? 1 : 0;
    cfgValue.m_dDoubleValue = bValue ? 1.0 : 0.0;
    cfgValue.m_bBoolValue = bValue;
    cfgValue.m_bHasValue = true;
    m_mapConfigValues[szKey] = cfgValue;
}

bool CConfigManager::HasKey(const char* szKey) const
{
    if (szKey == NULL)
    {
        return false;
    }
    
    return m_mapConfigValues.find(szKey) != m_mapConfigValues.end();
}

void CConfigManager::RemoveKey(const char* szKey)
{
    if (szKey == NULL)
    {
        return;
    }
    
    m_mapConfigValues.erase(szKey);
}

void CConfigManager::ClearAll()
{
    m_mapConfigValues.clear();
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // m_nLegacyCount = 0;
}

size_t CConfigManager::GetKeyCount() const
{
    return m_mapConfigValues.size();
}

void CConfigManager::SetConfigFilePath(const char* szPath)
{
    if (szPath != NULL)
    {
        strncpy(m_szConfigFilePath, szPath, sizeof(m_szConfigFilePath) - 1);
    }
}

const char* CConfigManager::GetConfigFilePath() const
{
    return m_szConfigFilePath;
}

bool CConfigManager::ParseLine(const char* szLine, std::string& refKey, std::string& refValue)
{
    if (szLine == NULL || *szLine == '\0' || *szLine == '#' || *szLine == ';')
    {
        return false;
    }
    
    std::string strLine(szLine);
    
    size_t nEqPos = strLine.find('=');
    if (nEqPos == std::string::npos)
    {
        return false;
    }
    
    refKey = strLine.substr(0, nEqPos);
    refValue = strLine.substr(nEqPos + 1);
    
    // Trim whitespace
    size_t nStart = refKey.find_first_not_of(" \t");
    size_t nEnd = refKey.find_last_not_of(" \t");
    if (nStart != std::string::npos)
    {
        refKey = refKey.substr(nStart, nEnd - nStart + 1);
    }
    
    nStart = refValue.find_first_not_of(" \t");
    nEnd = refValue.find_last_not_of(" \t\r\n");
    if (nStart != std::string::npos)
    {
        refValue = refValue.substr(nStart, nEnd - nStart + 1);
    }
    
    return !refKey.empty();
}

void CConfigManager::UpdateCachedValues(SConfigValue& refValue, const std::string& refStrValue)
{
    strncpy(refValue.m_szStringValue, refStrValue.c_str(), sizeof(refValue.m_szStringValue) - 1);
    
    refValue.m_nIntValue = atoi(refStrValue.c_str());
    refValue.m_dDoubleValue = atof(refStrValue.c_str());
    
    std::string strLower = refStrValue;
    for (size_t nIdx = 0; nIdx < strLower.size(); ++nIdx)
    {
        strLower[nIdx] = static_cast<char>(tolower(static_cast<unsigned char>(strLower[nIdx])));
    }
    
    refValue.m_bBoolValue = (strLower == "true" || strLower == "1" || strLower == "yes" || strLower == "on");
}
