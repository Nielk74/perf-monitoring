#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>

struct SDateTimeInfo
{
    int m_nYear;
    int m_nMonth;
    int m_nDay;
    int m_nHour;
    int m_nMinute;
    int m_nSecond;
    int m_nMillisecond;
    
    SDateTimeInfo()
        : m_nYear(1970)
        , m_nMonth(1)
        , m_nDay(1)
        , m_nHour(0)
        , m_nMinute(0)
        , m_nSecond(0)
        , m_nMillisecond(0)
    {
    }
};

class CDateTimeUtil
{
public:
    static std::string FormatForDb(const SDateTimeInfo& refDt);
    static std::string FormatForDb(time_t nTimestamp);
    static std::string FormatForDisplay(const SDateTimeInfo& refDt);
    static std::string FormatForFilename(const SDateTimeInfo& refDt);
    
    static bool ParseFromLegacyFormat(const char* szDateStr, SDateTimeInfo& refDt);
    static bool ParseFromDbFormat(const char* szDateStr, SDateTimeInfo& refDt);
    static bool ParseFromUserFormat(const char* szDateStr, SDateTimeInfo& refDt);
    
    static time_t ToTimestamp(const SDateTimeInfo& refDt);
    static void FromTimestamp(time_t nTimestamp, SDateTimeInfo& refDt);
    
    static void GetCurrentDateTime(SDateTimeInfo& refDt);
    static std::string GetCurrentDateTimeForDb();
    static time_t GetCurrentTimestamp();
    
    static int GetDaysBetween(const SDateTimeInfo& refDt1, const SDateTimeInfo& refDt2);
    static bool IsLeapYear(int nYear);
    static int GetDaysInMonth(int nYear, int nMonth);
    
    static void AddDays(SDateTimeInfo& refDt, int nDays);
    static void AddMonths(SDateTimeInfo& refDt, int nMonths);
    static void AddYears(SDateTimeInfo& refDt, int nYears);
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // static char s_szLegacyBuffer[64];
    // static const char* s_szLegacyDateFormat;
    // static bool s_bLegacyMode;

private:
    CDateTimeUtil();
    CDateTimeUtil(const CDateTimeUtil& refOther);
    CDateTimeUtil& operator=(const CDateTimeUtil& refOther);
    ~CDateTimeUtil();
    
    static void NormalizeDateTime(SDateTimeInfo& refDt);
};
