/*******************************************************************************
 * CDbResultSet.cpp
 * 
 * Database Result Set Implementation
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CDbResultSet.cpp,v 1.45 2019/04/10 11:45:00 apatel Exp $
 * $Revision: 1.45 $
 ******************************************************************************/

#include "db/CDbResultSet.h"
#include <cstring>
#include <cstdlib>
#include <ctime>

// MFC support
#ifdef _MFC_VER
    #include <afxdisp.h>
#else
    // Minimal COleDateTime replacement
    class COleDateTime {
    public:
        enum Status { invalid = 0, valid = 1, null = 2 };
        COleDateTime() : m_status(null), m_dt(0) {}
        COleDateTime(time_t t) : m_status(valid) { m_dt = (double)t; }
        Status GetStatus() const { return m_status; }
        int GetYear() const { return 0; }
        int GetMonth() const { return 0; }
        int GetDay() const { return 0; }
        int GetHour() const { return 0; }
        int GetMinute() const { return 0; }
        int GetSecond() const { return 0; }
        static COleDateTime GetCurrentTime() { return COleDateTime(time(NULL)); }
        BOOL ParseDateTime(LPCSTR) { return FALSE; }
    private:
        Status m_status;
        double m_dt;
    };
#endif

/*******************************************************************************
 * CDbResultRow Implementation
 ******************************************************************************/
CDbResultRow::CDbResultRow()
{
}

CDbResultRow::~CDbResultRow()
{
}

void CDbResultRow::SetColumn(int nIndex, const std::string& strValue)
{
    if (nIndex >= (int)m_vecColumns.size()) {
        m_vecColumns.resize(nIndex + 1);
    }
    m_vecColumns[nIndex] = strValue;
}

std::string CDbResultRow::GetColumn(int nIndex) const
{
    if (nIndex >= 0 && nIndex < (int)m_vecColumns.size()) {
        return m_vecColumns[nIndex];
    }
    return "";
}

std::string CDbResultRow::GetColumn(LPCSTR szColName) const
{
    std::map<std::string, int>::const_iterator it = m_mapColNameToIndex.find(szColName);
    if (it != m_mapColNameToIndex.end()) {
        return GetColumn(it->second);
    }
    return "";
}

/*******************************************************************************
 * CDbResultSet Implementation
 ******************************************************************************/
CDbResultSet::CDbResultSet(HSTMT hStmt)
    : m_hStmt(hStmt)
    , m_nColumnCount(0)
    , m_nCurrentRow(-1)
    , m_bEOF(FALSE)
    , m_bBOF(TRUE)
    , m_nLastErrorCode(0)
    , m_bFetchedAll(FALSE)
    , m_bOwnsHandle(TRUE)
{
    memset(m_szLastErrorMsg, 0, sizeof(m_szLastErrorMsg));
    
    if (m_hStmt != SQL_NULL_HSTMT) {
        GetColumnMetadata();
    }
}

CDbResultSet::~CDbResultSet()
{
    // Free cached rows
    ReleaseResults();
    
    // Free column data buffers
    for (size_t i = 0; i < m_vecColData.size(); i++) {
        if (m_vecColData[i]) {
            delete[] m_vecColData[i];
        }
    }
    m_vecColData.clear();
    m_vecColInd.clear();
    
    // Free statement handle if we own it
    if (m_hStmt != SQL_NULL_HSTMT && m_bOwnsHandle) {
        SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
        m_hStmt = SQL_NULL_HSTMT;
    }
}

/*******************************************************************************
 * Column Metadata
 ******************************************************************************/
void CDbResultSet::GetColumnMetadata()
{
    SQLSMALLINT nNumCols = 0;
    SQLNumResultCols(m_hStmt, &nNumCols);
    m_nColumnCount = nNumCols;
    
    m_vecColNames.resize(m_nColumnCount);
    m_vecColTypes.resize(m_nColumnCount);
    m_vecColData.resize(m_nColumnCount);
    m_vecColInd.resize(m_nColumnCount);
    
    for (int i = 0; i < m_nColumnCount; i++) {
        SQLCHAR szColName[MAX_COL_NAME_LEN];
        SQLSMALLINT nNameLen = 0;
        SQLSMALLINT nDataType = 0;
        SQLULEN nColSize = 0;
        SQLSMALLINT nDecimalDigits = 0;
        SQLSMALLINT nNullable = 0;
        
        SQLDescribeCol(m_hStmt, (SQLSMALLINT)(i + 1), szColName, MAX_COL_NAME_LEN,
                       &nNameLen, &nDataType, &nColSize, &nDecimalDigits, &nNullable);
        
        m_vecColNames[i] = (char*)szColName;
        m_mapColNameToIndex[m_vecColNames[i]] = i;
        
        // Map SQL type to our type
        switch (nDataType) {
            case SQL_INTEGER:
            case SQL_SMALLINT:
            case SQL_TINYINT:
                m_vecColTypes[i] = COL_TYPE_INT;
                break;
            case SQL_BIGINT:
                m_vecColTypes[i] = COL_TYPE_LONG;
                break;
            case SQL_FLOAT:
            case SQL_DOUBLE:
            case SQL_REAL:
            case SQL_DECIMAL:
            case SQL_NUMERIC:
                m_vecColTypes[i] = COL_TYPE_DOUBLE;
                break;
            case SQL_TYPE_TIMESTAMP:
            case SQL_TYPE_DATE:
            case SQL_TYPE_TIME:
            case SQL_DATETIME:
                m_vecColTypes[i] = COL_TYPE_DATETIME;
                break;
            case SQL_BIT:
                m_vecColTypes[i] = COL_TYPE_BOOL;
                break;
            case SQL_BINARY:
            case SQL_VARBINARY:
            case SQL_LONGVARBINARY:
                m_vecColTypes[i] = COL_TYPE_BINARY;
                break;
            default:
                m_vecColTypes[i] = COL_TYPE_STRING;
                break;
        }
        
        // Allocate buffer for column data
        SQLULEN nBufSize = nColSize + 1;
        if (nBufSize < MAX_COL_DATA_LEN) {
            nBufSize = MAX_COL_DATA_LEN;
        }
        m_vecColData[i] = new char[nBufSize];
        memset(m_vecColData[i], 0, nBufSize);
        m_vecColInd[i] = 0;
        
        // Bind column
        SQLBindCol(m_hStmt, (SQLSMALLINT)(i + 1), SQL_C_CHAR,
                   m_vecColData[i], nBufSize, &m_vecColInd[i]);
    }
}

/*******************************************************************************
 * Navigation
 ******************************************************************************/
BOOL CDbResultSet::Next()
{
    if (m_bFetchedAll || m_vecRows.size() > 0) {
        // Using cached data
        if (m_nCurrentRow < (int)m_vecRows.size() - 1) {
            m_nCurrentRow++;
            m_bBOF = FALSE;
            m_bEOF = FALSE;
            return TRUE;
        } else {
            m_bEOF = TRUE;
            return FALSE;
        }
    }
    
    // Fetch directly from ODBC
    return FetchNextRow();
}

BOOL CDbResultSet::Previous()
{
    if (m_vecRows.size() == 0) {
        return FALSE;
    }
    
    if (m_nCurrentRow > 0) {
        m_nCurrentRow--;
        m_bBOF = FALSE;
        m_bEOF = FALSE;
        return TRUE;
    }
    
    m_bBOF = TRUE;
    return FALSE;
}

BOOL CDbResultSet::First()
{
    if (m_vecRows.size() == 0) {
        return FALSE;
    }
    
    m_nCurrentRow = 0;
    m_bBOF = FALSE;
    m_bEOF = FALSE;
    return TRUE;
}

BOOL CDbResultSet::Last()
{
    if (m_vecRows.size() == 0) {
        return FALSE;
    }
    
    m_nCurrentRow = (int)m_vecRows.size() - 1;
    m_bBOF = FALSE;
    m_bEOF = FALSE;
    return TRUE;
}

BOOL CDbResultSet::Absolute(int nRow)
{
    if (nRow < 0 || nRow >= (int)m_vecRows.size()) {
        return FALSE;
    }
    
    m_nCurrentRow = nRow;
    m_bBOF = FALSE;
    m_bEOF = FALSE;
    return TRUE;
}

/*******************************************************************************
 * Data Fetching
 ******************************************************************************/
BOOL CDbResultSet::FetchNextRow()
{
    SQLRETURN nRet = SQLFetch(m_hStmt);
    
    if (nRet == SQL_NO_DATA) {
        m_bEOF = TRUE;
        return FALSE;
    }
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_STMT, m_hStmt);
        return FALSE;
    }
    
    m_nCurrentRow++;
    m_bBOF = FALSE;
    return TRUE;
}

void CDbResultSet::FetchResults()
{
    // Fetch all rows into memory
    while (FetchNextRow()) {
        if (m_vecRows.size() >= MAX_ROWS_CACHED) {
            // Stop caching if too many rows
            break;
        }
        
        CDbResultRow* pRow = new CDbResultRow();
        
        for (int i = 0; i < m_nColumnCount; i++) {
            std::string strValue;
            if (m_vecColInd[i] != SQL_NULL_DATA) {
                strValue = m_vecColData[i];
            }
            pRow->SetColumn(i, strValue);
            pRow->m_mapColNameToIndex[m_vecColNames[i]] = i;
        }
        
        m_vecRows.push_back(pRow);
    }
    
    m_bFetchedAll = TRUE;
    m_nCurrentRow = -1;
}

void CDbResultSet::ReleaseResults()
{
    for (size_t i = 0; i < m_vecRows.size(); i++) {
        delete m_vecRows[i];
    }
    m_vecRows.clear();
    m_nCurrentRow = -1;
    m_bEOF = FALSE;
    m_bBOF = TRUE;
    m_bFetchedAll = FALSE;
}

/*******************************************************************************
 * Column Information
 ******************************************************************************/
LPCSTR CDbResultSet::GetColumnName(int nIndex) const
{
    if (nIndex >= 0 && nIndex < m_nColumnCount) {
        return m_vecColNames[nIndex].c_str();
    }
    return "";
}

int CDbResultSet::GetColumnType(int nIndex) const
{
    if (nIndex >= 0 && nIndex < m_nColumnCount) {
        return m_vecColTypes[nIndex];
    }
    return COL_TYPE_UNKNOWN;
}

int CDbResultSet::FindColumn(LPCSTR szColName) const
{
    std::map<std::string, int>::const_iterator it = m_mapColNameToIndex.find(szColName);
    if (it != m_mapColNameToIndex.end()) {
        return it->second;
    }
    return -1;
}

/*******************************************************************************
 * Data Access by Index
 ******************************************************************************/
long CDbResultSet::GetLong(int nIndex) const
{
    if (m_vecRows.size() > 0 && m_nCurrentRow >= 0 && m_nCurrentRow < (int)m_vecRows.size()) {
        std::string strVal = m_vecRows[m_nCurrentRow]->GetColumn(nIndex);
        return atol(strVal.c_str());
    }
    
    if (nIndex >= 0 && nIndex < m_nColumnCount && m_vecColData[nIndex]) {
        if (m_vecColInd[nIndex] == SQL_NULL_DATA) {
            return 0;
        }
        return atol(m_vecColData[nIndex]);
    }
    return 0;
}

double CDbResultSet::GetDouble(int nIndex) const
{
    if (m_vecRows.size() > 0 && m_nCurrentRow >= 0 && m_nCurrentRow < (int)m_vecRows.size()) {
        std::string strVal = m_vecRows[m_nCurrentRow]->GetColumn(nIndex);
        return atof(strVal.c_str());
    }
    
    if (nIndex >= 0 && nIndex < m_nColumnCount && m_vecColData[nIndex]) {
        if (m_vecColInd[nIndex] == SQL_NULL_DATA) {
            return 0.0;
        }
        return atof(m_vecColData[nIndex]);
    }
    return 0.0;
}

std::string CDbResultSet::GetString(int nIndex) const
{
    if (m_vecRows.size() > 0 && m_nCurrentRow >= 0 && m_nCurrentRow < (int)m_vecRows.size()) {
        return m_vecRows[m_nCurrentRow]->GetColumn(nIndex);
    }
    
    if (nIndex >= 0 && nIndex < m_nColumnCount && m_vecColData[nIndex]) {
        if (m_vecColInd[nIndex] == SQL_NULL_DATA) {
            return "";
        }
        return std::string(m_vecColData[nIndex]);
    }
    return "";
}

COleDateTime CDbResultSet::GetDateTime(int nIndex) const
{
    // Simplified implementation - parse from string
    std::string strVal = GetString(nIndex);
    if (strVal.empty()) {
        return COleDateTime();
    }
    
    // Parse YYYY-MM-DD HH:MM:SS format
    // In production, would use proper date parsing
    return COleDateTime::GetCurrentTime();
}

BOOL CDbResultSet::GetBool(int nIndex) const
{
    if (m_vecRows.size() > 0 && m_nCurrentRow >= 0 && m_nCurrentRow < (int)m_vecRows.size()) {
        std::string strVal = m_vecRows[m_nCurrentRow]->GetColumn(nIndex);
        return (strVal == "Y" || strVal == "y" || strVal == "1" || strVal == "TRUE");
    }
    
    if (nIndex >= 0 && nIndex < m_nColumnCount && m_vecColData[nIndex]) {
        if (m_vecColInd[nIndex] == SQL_NULL_DATA) {
            return FALSE;
        }
        char c = m_vecColData[nIndex][0];
        return (c == 'Y' || c == 'y' || c == '1');
    }
    return FALSE;
}

/*******************************************************************************
 * Data Access by Name
 ******************************************************************************/
long CDbResultSet::GetLong(LPCSTR szColName) const
{
    int nIndex = FindColumn(szColName);
    if (nIndex >= 0) {
        return GetLong(nIndex);
    }
    return 0;
}

double CDbResultSet::GetDouble(LPCSTR szColName) const
{
    int nIndex = FindColumn(szColName);
    if (nIndex >= 0) {
        return GetDouble(nIndex);
    }
    return 0.0;
}

std::string CDbResultSet::GetString(LPCSTR szColName) const
{
    int nIndex = FindColumn(szColName);
    if (nIndex >= 0) {
        return GetString(nIndex);
    }
    return "";
}

COleDateTime CDbResultSet::GetDateTime(LPCSTR szColName) const
{
    int nIndex = FindColumn(szColName);
    if (nIndex >= 0) {
        return GetDateTime(nIndex);
    }
    return COleDateTime();
}

BOOL CDbResultSet::GetBool(LPCSTR szColName) const
{
    int nIndex = FindColumn(szColName);
    if (nIndex >= 0) {
        return GetBool(nIndex);
    }
    return FALSE;
}

/*******************************************************************************
 * Null Checking
 ******************************************************************************/
BOOL CDbResultSet::IsNull(int nIndex) const
{
    if (m_vecRows.size() > 0 && m_nCurrentRow >= 0 && m_nCurrentRow < (int)m_vecRows.size()) {
        std::string strVal = m_vecRows[m_nCurrentRow]->GetColumn(nIndex);
        return strVal.empty();
    }
    
    if (nIndex >= 0 && nIndex < m_nColumnCount) {
        return (m_vecColInd[nIndex] == SQL_NULL_DATA);
    }
    return TRUE;
}

BOOL CDbResultSet::IsNull(LPCSTR szColName) const
{
    int nIndex = FindColumn(szColName);
    if (nIndex >= 0) {
        return IsNull(nIndex);
    }
    return TRUE;
}

/*******************************************************************************
 * Error Handling
 ******************************************************************************/
void CDbResultSet::SetErrorFromODBC(SQLSMALLINT nHandleType, SQLHANDLE hHandle)
{
    SQLCHAR szState[6];
    SQLCHAR szMsg[512];
    SQLINTEGER nNativeError;
    SQLSMALLINT nMsgLen;
    
    SQLGetDiagRec(nHandleType, hHandle, 1, szState, &nNativeError,
                  szMsg, 512, &nMsgLen);
    
    m_nLastErrorCode = nNativeError;
    strcpy(m_szLastErrorMsg, (char*)szMsg);
}

// $Log: CDbResultSet.cpp,v $
// Revision 1.45  2019/04/10 11:45:00  apatel
// Fixed memory leak in ReleaseResults
//
// Revision 1.44  2016/09/01 10:00:00  apatel
// Added column type tracking
//
// Revision 1.43  2011/03/08 09:30:00  knakamura
// Added caching support
//
// Revision 1.42  2004/03/15 10:00:00  jthompson
// Initial version
