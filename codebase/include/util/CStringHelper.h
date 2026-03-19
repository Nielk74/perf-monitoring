#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>

class CStringHelper
{
public:
    static void SafeTrim(char* szStr);
    static void SafeTrim(std::string& refStr);
    
    static void ToUpper(char* szStr);
    static void ToUpper(std::string& refStr);
    static std::string ToUpper(const std::string& refStr);
    
    static void ToLower(char* szStr);
    static void ToLower(std::string& refStr);
    
    static std::string FormatWithPadding(const char* szStr, size_t nTotalLen, char chPad = ' ', bool bPadLeft = false);
    static std::string FormatWithPadding(const std::string& refStr, size_t nTotalLen, char chPad = ' ', bool bPadLeft = false);
    
    static bool IsNullOrEmpty(const char* szStr);
    static bool IsNullOrWhitespace(const char* szStr);
    
    static std::string Left(const std::string& refStr, size_t nCount);
    static std::string Right(const std::string& refStr, size_t nCount);
    static std::string Mid(const std::string& refStr, size_t nPos, size_t nCount = std::string::npos);
    
    static int CompareNoCase(const char* szLeft, const char* szRight);
    static bool EqualsNoCase(const char* szLeft, const char* szRight);
    
    static void ReplaceAll(char* szStr, char chOld, char chNew);
    static std::string ReplaceAll(const std::string& refStr, const std::string& refOld, const std::string& refNew);
    
    // DEPRECATED: kept for backward compatibility - DO NOT USE
    // static char* s_szLegacyBuffer;
    // static const int s_nBufferSize = 4096;

private:
    CStringHelper();
    CStringHelper(const CStringHelper& refOther);
    CStringHelper& operator=(const CStringHelper& refOther);
    ~CStringHelper();
};
