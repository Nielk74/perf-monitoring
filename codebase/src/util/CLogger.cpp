#pragma warning(disable:4786)
#pragma warning(disable:4996)

#include "util/CLogger.h"
#include "util/CDateTimeUtil.h"
#include <cstdarg>
#include <cstdio>
#include <iostream>

CLogger* CLogger::s_pInstance = NULL;
CLogger* g_pLogger = NULL;

CLogger* CLogger::GetInstance()
{
    if (s_pInstance == NULL)
    {
        s_pInstance = new CLogger();
        g_pLogger = s_pInstance;
    }
    return s_pInstance;
}

void CLogger::DestroyInstance()
{
    if (s_pInstance != NULL)
    {
        s_pInstance->Shutdown();
        delete s_pInstance;
        s_pInstance = NULL;
        g_pLogger = NULL;
    }
}

CLogger::CLogger()
    : m_eLogLevel(LOG_LEVEL_INFO)
    , m_bConsoleOutput(true)
    , m_bFileOutput(false)
    , m_nMaxFileSize(10 * 1024 * 1024)
    , m_bInitialized(false)
{
    memset(m_szLogFilePath, 0, sizeof(m_szLogFilePath));
}

CLogger::~CLogger()
{
    Shutdown();
}

bool CLogger::Initialize(const char* szLogFilePath)
{
    if (szLogFilePath == NULL)
    {
        return false;
    }
    
    strncpy(m_szLogFilePath, szLogFilePath, sizeof(m_szLogFilePath) - 1);
    
    m_ofsLogFile.open(m_szLogFilePath, std::ios::app);
    if (!m_ofsLogFile.is_open())
    {
        return false;
    }
    
    m_bFileOutput = true;
    m_bInitialized = true;
    return true;
}

void CLogger::Shutdown()
{
    if (m_ofsLogFile.is_open())
    {
        m_ofsLogFile.close();
    }
    m_bInitialized = false;
}

void CLogger::LogDebug(const char* szMessage)
{
    Log(LOG_LEVEL_DEBUG, szMessage);
}

void CLogger::LogInfo(const char* szMessage)
{
    Log(LOG_LEVEL_INFO, szMessage);
}

void CLogger::LogWarning(const char* szMessage)
{
    Log(LOG_LEVEL_WARNING, szMessage);
}

void CLogger::LogError(const char* szMessage)
{
    Log(LOG_LEVEL_ERROR, szMessage);
}

void CLogger::LogCritical(const char* szMessage)
{
    Log(LOG_LEVEL_CRITICAL, szMessage);
}

void CLogger::Log(ELogLevel eLevel, const char* szMessage)
{
    if (eLevel < m_eLogLevel)
    {
        return;
    }
    
    WriteLog(eLevel, szMessage);
}

void CLogger::LogFormat(ELogLevel eLevel, const char* szFormat, ...)
{
    if (eLevel < m_eLogLevel)
    {
        return;
    }
    
    char szBuffer[4096];
    va_list args;
    va_start(args, szFormat);
    vsnprintf(szBuffer, sizeof(szBuffer), szFormat, args);
    va_end(args);
    
    WriteLog(eLevel, szBuffer);
}

void CLogger::WriteLog(ELogLevel eLevel, const char* szMessage)
{
    SDateTimeInfo dt;
    CDateTimeUtil::GetCurrentDateTime(dt);
    
    char szTimestamp[64];
    snprintf(szTimestamp, sizeof(szTimestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             dt.m_nYear, dt.m_nMonth, dt.m_nDay,
             dt.m_nHour, dt.m_nMinute, dt.m_nSecond);
    
    const char* szLevel = LevelToString(eLevel);
    
    char szFullMessage[5120];
    snprintf(szFullMessage, sizeof(szFullMessage), "[%s] [%s] %s\n",
             szTimestamp, szLevel, szMessage);
    
    if (m_bConsoleOutput)
    {
        std::cout << szFullMessage;
    }
    
    if (m_bFileOutput && m_ofsLogFile.is_open())
    {
        m_ofsLogFile << szFullMessage;
        m_ofsLogFile.flush();
        CheckFileSize();
    }
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // strncpy(m_szLegacyBuffer, szFullMessage, sizeof(m_szLegacyBuffer) - 1);
}

const char* CLogger::LevelToString(ELogLevel eLevel) const
{
    switch (eLevel)
    {
    case LOG_LEVEL_DEBUG:    return "DEBUG";
    case LOG_LEVEL_INFO:     return "INFO";
    case LOG_LEVEL_WARNING:  return "WARN";
    case LOG_LEVEL_ERROR:    return "ERROR";
    case LOG_LEVEL_CRITICAL: return "CRITICAL";
    default:                 return "UNKNOWN";
    }
}

void CLogger::CheckFileSize()
{
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // if (m_ofsLogFile.tellp() > m_nMaxFileSize)
    // {
    //     RotateLog();
    // }
}

void CLogger::SetLogLevel(ELogLevel eLevel)
{
    m_eLogLevel = eLevel;
}

ELogLevel CLogger::GetLogLevel() const
{
    return m_eLogLevel;
}

void CLogger::SetLogFilePath(const char* szPath)
{
    if (szPath != NULL)
    {
        strncpy(m_szLogFilePath, szPath, sizeof(m_szLogFilePath) - 1);
    }
}

const char* CLogger::GetLogFilePath() const
{
    return m_szLogFilePath;
}

void CLogger::EnableConsoleOutput(bool bEnable)
{
    m_bConsoleOutput = bEnable;
}

bool CLogger::IsConsoleOutputEnabled() const
{
    return m_bConsoleOutput;
}

void CLogger::EnableFileOutput(bool bEnable)
{
    m_bFileOutput = bEnable;
}

bool CLogger::IsFileOutputEnabled() const
{
    return m_bFileOutput;
}

void CLogger::SetMaxFileSize(size_t nMaxBytes)
{
    m_nMaxFileSize = nMaxBytes;
}

size_t CLogger::GetMaxFileSize() const
{
    return m_nMaxFileSize;
}

void CLogger::RotateLog()
{
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // char szOldPath[512];
    // char szNewPath[512];
    // // ... rotation logic
}
