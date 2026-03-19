#pragma warning(disable:4786)
#pragma warning(disable:4100)

#include "util/CStringHelper.h"
#include <algorithm>
#include <cctype>
#include <cstring>

void CStringHelper::SafeTrim(char* szStr)
{
    if (szStr == NULL || *szStr == '\0')
    {
        return;
    }
    
    char* szStart = szStr;
    while (*szStart && isspace(static_cast<unsigned char>(*szStart)))
    {
        ++szStart;
    }
    
    char* szEnd = szStr + strlen(szStr) - 1;
    while (szEnd > szStart && isspace(static_cast<unsigned char>(*szEnd)))
    {
        --szEnd;
    }
    *(szEnd + 1) = '\0';
    
    if (szStart != szStr)
    {
        memmove(szStr, szStart, szEnd - szStart + 2);
    }
}

void CStringHelper::SafeTrim(std::string& refStr)
{
    size_t nStart = refStr.find_first_not_of(" \t\r\n");
    if (nStart == std::string::npos)
    {
        refStr.clear();
        return;
    }
    
    size_t nEnd = refStr.find_last_not_of(" \t\r\n");
    refStr = refStr.substr(nStart, nEnd - nStart + 1);
}

void CStringHelper::ToUpper(char* szStr)
{
    if (szStr == NULL)
    {
        return;
    }
    
    for (char* p = szStr; *p; ++p)
    {
        *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
    }
}

void CStringHelper::ToUpper(std::string& refStr)
{
    for (size_t nIdx = 0; nIdx < refStr.size(); ++nIdx)
    {
        refStr[nIdx] = static_cast<char>(toupper(static_cast<unsigned char>(refStr[nIdx])));
    }
}

std::string CStringHelper::ToUpper(const std::string& refStr)
{
    std::string strResult = refStr;
    ToUpper(strResult);
    return strResult;
}

void CStringHelper::ToLower(char* szStr)
{
    if (szStr == NULL)
    {
        return;
    }
    
    for (char* p = szStr; *p; ++p)
    {
        *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    }
}

void CStringHelper::ToLower(std::string& refStr)
{
    for (size_t nIdx = 0; nIdx < refStr.size(); ++nIdx)
    {
        refStr[nIdx] = static_cast<char>(tolower(static_cast<unsigned char>(refStr[nIdx])));
    }
}

std::string CStringHelper::FormatWithPadding(const char* szStr, size_t nTotalLen, char chPad, bool bPadLeft)
{
    std::string strResult;
    if (szStr != NULL)
    {
        strResult = szStr;
    }
    return FormatWithPadding(strResult, nTotalLen, chPad, bPadLeft);
}

std::string CStringHelper::FormatWithPadding(const std::string& refStr, size_t nTotalLen, char chPad, bool bPadLeft)
{
    if (refStr.size() >= nTotalLen)
    {
        return refStr.substr(0, nTotalLen);
    }
    
    size_t nPadCount = nTotalLen - refStr.size();
    std::string strPad(nPadCount, chPad);
    
    if (bPadLeft)
    {
        return strPad + refStr;
    }
    else
    {
        return refStr + strPad;
    }
}

bool CStringHelper::IsNullOrEmpty(const char* szStr)
{
    return szStr == NULL || *szStr == '\0';
}

bool CStringHelper::IsNullOrWhitespace(const char* szStr)
{
    if (szStr == NULL)
    {
        return true;
    }
    
    while (*szStr)
    {
        if (!isspace(static_cast<unsigned char>(*szStr)))
        {
            return false;
        }
        ++szStr;
    }
    return true;
}

std::string CStringHelper::Left(const std::string& refStr, size_t nCount)
{
    if (nCount >= refStr.size())
    {
        return refStr;
    }
    return refStr.substr(0, nCount);
}

std::string CStringHelper::Right(const std::string& refStr, size_t nCount)
{
    if (nCount >= refStr.size())
    {
        return refStr;
    }
    return refStr.substr(refStr.size() - nCount);
}

std::string CStringHelper::Mid(const std::string& refStr, size_t nPos, size_t nCount)
{
    if (nPos >= refStr.size())
    {
        return "";
    }
    return refStr.substr(nPos, nCount);
}

int CStringHelper::CompareNoCase(const char* szLeft, const char* szRight)
{
    if (szLeft == NULL && szRight == NULL) return 0;
    if (szLeft == NULL) return -1;
    if (szRight == NULL) return 1;
    
#ifdef _WIN32
    return _stricmp(szLeft, szRight);
#else
    return strcasecmp(szLeft, szRight);
#endif
}

bool CStringHelper::EqualsNoCase(const char* szLeft, const char* szRight)
{
    return CompareNoCase(szLeft, szRight) == 0;
}

void CStringHelper::ReplaceAll(char* szStr, char chOld, char chNew)
{
    if (szStr == NULL)
    {
        return;
    }
    
    for (char* p = szStr; *p; ++p)
    {
        if (*p == chOld)
        {
            *p = chNew;
        }
    }
}

std::string CStringHelper::ReplaceAll(const std::string& refStr, const std::string& refOld, const std::string& refNew)
{
    if (refOld.empty())
    {
        return refStr;
    }
    
    std::string strResult;
    size_t nPos = 0;
    size_t nPrevPos = 0;
    
    while ((nPos = refStr.find(refOld, nPrevPos)) != std::string::npos)
    {
        strResult += refStr.substr(nPrevPos, nPos - nPrevPos);
        strResult += refNew;
        nPrevPos = nPos + refOld.size();
    }
    
    strResult += refStr.substr(nPrevPos);
    return strResult;
}
