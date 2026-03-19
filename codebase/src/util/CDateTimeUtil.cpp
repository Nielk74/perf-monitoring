#pragma warning(disable:4786)
#pragma warning(disable:4100)

#include "util/CDateTimeUtil.h"
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>

std::string CDateTimeUtil::FormatForDb(const SDateTimeInfo& refDt)
{
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << refDt.m_nYear << "-"
        << std::setw(2) << refDt.m_nMonth << "-"
        << std::setw(2) << refDt.m_nDay << " "
        << std::setw(2) << refDt.m_nHour << ":"
        << std::setw(2) << refDt.m_nMinute << ":"
        << std::setw(2) << refDt.m_nSecond;
    return oss.str();
}

std::string CDateTimeUtil::FormatForDb(time_t nTimestamp)
{
    SDateTimeInfo dt;
    FromTimestamp(nTimestamp, dt);
    return FormatForDb(dt);
}

std::string CDateTimeUtil::FormatForDisplay(const SDateTimeInfo& refDt)
{
    const char* arrMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    std::ostringstream oss;
    oss << arrMonths[refDt.m_nMonth - 1] << " "
        << refDt.m_nDay << ", "
        << refDt.m_nYear << " "
        << std::setfill('0')
        << std::setw(2) << refDt.m_nHour << ":"
        << std::setw(2) << refDt.m_nMinute << ":"
        << std::setw(2) << refDt.m_nSecond;
    return oss.str();
}

std::string CDateTimeUtil::FormatForFilename(const SDateTimeInfo& refDt)
{
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << refDt.m_nYear
        << std::setw(2) << refDt.m_nMonth
        << std::setw(2) << refDt.m_nDay << "_"
        << std::setw(2) << refDt.m_nHour
        << std::setw(2) << refDt.m_nMinute
        << std::setw(2) << refDt.m_nSecond;
    return oss.str();
}

bool CDateTimeUtil::ParseFromLegacyFormat(const char* szDateStr, SDateTimeInfo& refDt)
{
    if (szDateStr == NULL)
    {
        return false;
    }
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // Legacy format: DD/MM/YYYY HH:MM:SS
    // int nDay, nMonth, nYear, nHour, nMinute, nSecond;
    // if (sscanf(szDateStr, "%d/%d/%d %d:%d:%d",
    //            &nDay, &nMonth, &nYear, &nHour, &nMinute, &nSecond) >= 3)
    // {
    //     refDt.m_nDay = nDay;
    //     refDt.m_nMonth = nMonth;
    //     refDt.m_nYear = nYear;
    //     refDt.m_nHour = nHour;
    //     refDt.m_nMinute = nMinute;
    //     refDt.m_nSecond = nSecond;
    //     return true;
    // }
    
    // Default format: YYYY-MM-DD HH:MM:SS
    int nYear, nMonth, nDay, nHour = 0, nMinute = 0, nSecond = 0;
    if (sscanf(szDateStr, "%d-%d-%d %d:%d:%d",
               &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) >= 3)
    {
        refDt.m_nYear = nYear;
        refDt.m_nMonth = nMonth;
        refDt.m_nDay = nDay;
        refDt.m_nHour = nHour;
        refDt.m_nMinute = nMinute;
        refDt.m_nSecond = nSecond;
        return true;
    }
    
    return false;
}

bool CDateTimeUtil::ParseFromDbFormat(const char* szDateStr, SDateTimeInfo& refDt)
{
    return ParseFromLegacyFormat(szDateStr, refDt);
}

bool CDateTimeUtil::ParseFromUserFormat(const char* szDateStr, SDateTimeInfo& refDt)
{
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // User format: MM/DD/YYYY
    if (szDateStr == NULL)
    {
        return false;
    }
    
    int nMonth, nDay, nYear;
    if (sscanf(szDateStr, "%d/%d/%d", &nMonth, &nDay, &nYear) == 3)
    {
        refDt.m_nYear = nYear;
        refDt.m_nMonth = nMonth;
        refDt.m_nDay = nDay;
        refDt.m_nHour = 0;
        refDt.m_nMinute = 0;
        refDt.m_nSecond = 0;
        return true;
    }
    
    return false;
}

time_t CDateTimeUtil::ToTimestamp(const SDateTimeInfo& refDt)
{
    struct tm tmTime;
    memset(&tmTime, 0, sizeof(tmTime));
    
    tmTime.tm_year = refDt.m_nYear - 1900;
    tmTime.tm_mon = refDt.m_nMonth - 1;
    tmTime.tm_mday = refDt.m_nDay;
    tmTime.tm_hour = refDt.m_nHour;
    tmTime.tm_min = refDt.m_nMinute;
    tmTime.tm_sec = refDt.m_nSecond;
    tmTime.tm_isdst = -1;
    
    return mktime(&tmTime);
}

void CDateTimeUtil::FromTimestamp(time_t nTimestamp, SDateTimeInfo& refDt)
{
    struct tm* pTm = localtime(&nTimestamp);
    if (pTm != NULL)
    {
        refDt.m_nYear = pTm->tm_year + 1900;
        refDt.m_nMonth = pTm->tm_mon + 1;
        refDt.m_nDay = pTm->tm_mday;
        refDt.m_nHour = pTm->tm_hour;
        refDt.m_nMinute = pTm->tm_min;
        refDt.m_nSecond = pTm->tm_sec;
        refDt.m_nMillisecond = 0;
    }
}

void CDateTimeUtil::GetCurrentDateTime(SDateTimeInfo& refDt)
{
    time_t now = time(NULL);
    FromTimestamp(now, refDt);
}

std::string CDateTimeUtil::GetCurrentDateTimeForDb()
{
    SDateTimeInfo dt;
    GetCurrentDateTime(dt);
    return FormatForDb(dt);
}

time_t CDateTimeUtil::GetCurrentTimestamp()
{
    return time(NULL);
}

int CDateTimeUtil::GetDaysBetween(const SDateTimeInfo& refDt1, const SDateTimeInfo& refDt2)
{
    time_t t1 = ToTimestamp(refDt1);
    time_t t2 = ToTimestamp(refDt2);
    
    double dDiff = difftime(t2, t1);
    return static_cast<int>(dDiff / (60 * 60 * 24));
}

bool CDateTimeUtil::IsLeapYear(int nYear)
{
    return (nYear % 4 == 0 && nYear % 100 != 0) || (nYear % 400 == 0);
}

int CDateTimeUtil::GetDaysInMonth(int nYear, int nMonth)
{
    static const int arrDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (nMonth < 1 || nMonth > 12)
    {
        return 0;
    }
    
    if (nMonth == 2 && IsLeapYear(nYear))
    {
        return 29;
    }
    
    return arrDays[nMonth - 1];
}

void CDateTimeUtil::AddDays(SDateTimeInfo& refDt, int nDays)
{
    time_t t = ToTimestamp(refDt);
    t += nDays * 24 * 60 * 60;
    FromTimestamp(t, refDt);
}

void CDateTimeUtil::AddMonths(SDateTimeInfo& refDt, int nMonths)
{
    refDt.m_nMonth += nMonths;
    while (refDt.m_nMonth > 12)
    {
        refDt.m_nMonth -= 12;
        refDt.m_nYear++;
    }
    while (refDt.m_nMonth < 1)
    {
        refDt.m_nMonth += 12;
        refDt.m_nYear--;
    }
    
    int nMaxDays = GetDaysInMonth(refDt.m_nYear, refDt.m_nMonth);
    if (refDt.m_nDay > nMaxDays)
    {
        refDt.m_nDay = nMaxDays;
    }
}

void CDateTimeUtil::AddYears(SDateTimeInfo& refDt, int nYears)
{
    refDt.m_nYear += nYears;
    
    if (refDt.m_nMonth == 2 && refDt.m_nDay == 29 && !IsLeapYear(refDt.m_nYear))
    {
        refDt.m_nDay = 28;
    }
}

void CDateTimeUtil::NormalizeDateTime(SDateTimeInfo& refDt)
{
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // AddMonths(refDt, 0);
}
