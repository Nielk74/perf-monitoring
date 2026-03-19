#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <fstream>

enum ELogLevel
{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4,
    LOG_LEVEL_NONE = 5
};

class CLogger
{
public:
    static CLogger* GetInstance();
    static void DestroyInstance();
    
    bool Initialize(const char* szLogFilePath);
    void Shutdown();
    
    void LogDebug(const char* szMessage);
    void LogInfo(const char* szMessage);
    void LogWarning(const char* szMessage);
    void LogError(const char* szMessage);
    void LogCritical(const char* szMessage);
    
    void Log(ELogLevel eLevel, const char* szMessage);
    void LogFormat(ELogLevel eLevel, const char* szFormat, ...);
    
    void SetLogLevel(ELogLevel eLevel);
    ELogLevel GetLogLevel() const;
    
    void SetLogFilePath(const char* szPath);
    const char* GetLogFilePath() const;
    
    void EnableConsoleOutput(bool bEnable);
    bool IsConsoleOutputEnabled() const;
    
    void EnableFileOutput(bool bEnable);
    bool IsFileOutputEnabled() const;
    
    void SetMaxFileSize(size_t nMaxBytes);
    size_t GetMaxFileSize() const;
    
    void RotateLog();
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // static const int s_nMaxBufferSize = 4096;
    // char m_szLegacyBuffer[4096];

private:
    CLogger();
    CLogger(const CLogger& refOther);
    CLogger& operator=(const CLogger& refOther);
    ~CLogger();
    
    void WriteLog(ELogLevel eLevel, const char* szMessage);
    const char* LevelToString(ELogLevel eLevel) const;
    void CheckFileSize();
    
    char m_szLogFilePath[512];
    std::ofstream m_ofsLogFile;
    ELogLevel m_eLogLevel;
    bool m_bConsoleOutput;
    bool m_bFileOutput;
    size_t m_nMaxFileSize;
    bool m_bInitialized;
    
    static CLogger* s_pInstance;
};

extern CLogger* g_pLogger;

#define LOG_DEBUG(msg) CLogger::GetInstance()->LogDebug(msg)
#define LOG_INFO(msg) CLogger::GetInstance()->LogInfo(msg)
#define LOG_WARNING(msg) CLogger::GetInstance()->LogWarning(msg)
#define LOG_ERROR(msg) CLogger::GetInstance()->LogError(msg)
#define LOG_CRITICAL(msg) CLogger::GetInstance()->LogCritical(msg)
