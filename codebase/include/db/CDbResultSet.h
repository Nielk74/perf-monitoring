/*******************************************************************************
 * CDbResultSet.h
 * 
 * Database Result Set Wrapper
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CDbResultSet.h,v 1.34 2016/09/01 10:00:00 apatel Exp $
 * $Revision: 1.34 $
 ******************************************************************************/

#ifndef CDBRESULTSET_H
#define CDBRESULTSET_H

#ifdef _WIN32
    #include <windows.h>
    #include <sql.h>
    #include <sqlext.h>
#else
    #include <sql.h>
    #include <sqlext.h>
#endif

#include <string>
#include <vector>
#include <map>

// Maximum column name length
#define MAX_COL_NAME_LEN    128
#define MAX_COL_DATA_LEN    4096
#define MAX_ROWS_CACHED     10000

// Column types
#define COL_TYPE_UNKNOWN    0
#define COL_TYPE_INT        1
#define COL_TYPE_LONG       2
#define COL_TYPE_DOUBLE     3
#define COL_TYPE_STRING     4
#define COL_TYPE_DATETIME   5
#define COL_TYPE_BOOL       6
#define COL_TYPE_BINARY     7

/*******************************************************************************
 * CDbResultRow - Represents a single row of data
 ******************************************************************************/
class CDbResultRow
{
public:
    CDbResultRow();
    ~CDbResultRow();
    
    void SetColumn(int nIndex, const std::string& strValue);
    std::string GetColumn(int nIndex) const;
    std::string GetColumn(LPCSTR szColName) const;
    
    int GetColumnCount() const { return (int)m_vecColumns.size(); }
    
private:
    std::vector<std::string> m_vecColumns;
    std::map<std::string, int> m_mapColNameToIndex;
    
    friend class CDbResultSet;
};

/*******************************************************************************
 * CDbResultSet - Result Set Wrapper
 * 
 * Usage:
 *   CDbResultSet* pRS = pConn->ExecuteQuery("SELECT * FROM TRD_BLOTTER");
 *   while (pRS->Next()) {
 *       long lId = pRS->GetLong("TRD_ID");
 *       std::string strRef = pRS->GetString("TRD_REF_NM");
 *       // ...
 *   }
 *   delete pRS;
 ******************************************************************************/
class CDbResultSet
{
public:
    CDbResultSet(HSTMT hStmt);
    ~CDbResultSet();
    
    // Navigation
    BOOL Next();
    BOOL Previous();
    BOOL First();
    BOOL Last();
    BOOL Absolute(int nRow);
    BOOL IsEOF() const { return m_bEOF; }
    BOOL IsBOF() const { return m_bBOF; }
    
    // Row count
    int GetRowCount() const { return (int)m_vecRows.size(); }
    int GetCurrentRow() const { return m_nCurrentRow; }
    
    // Column information
    int GetColumnCount() const { return m_nColumnCount; }
    LPCSTR GetColumnName(int nIndex) const;
    int GetColumnType(int nIndex) const;
    int FindColumn(LPCSTR szColName) const;
    
    // Data access by column index
    long GetLong(int nIndex) const;
    double GetDouble(int nIndex) const;
    std::string GetString(int nIndex) const;
    COleDateTime GetDateTime(int nIndex) const;
    BOOL GetBool(int nIndex) const;
    
    // Data access by column name
    long GetLong(LPCSTR szColName) const;
    double GetDouble(LPCSTR szColName) const;
    std::string GetString(LPCSTR szColName) const;
    COleDateTime GetDateTime(LPCSTR szColName) const;
    BOOL GetBool(LPCSTR szColName) const;
    
    // Null checking
    BOOL IsNull(int nIndex) const;
    BOOL IsNull(LPCSTR szColName) const;
    
    // Fetch results into memory (call after query execution)
    void FetchResults();
    
    // Release memory (for large result sets)
    void ReleaseResults();
    
    // Get underlying statement handle (for advanced usage)
    HSTMT GetStatementHandle() const { return m_hStmt; }
    
    // Error handling
    int GetLastErrorCode() const { return m_nLastErrorCode; }
    LPCSTR GetLastErrorMsg() const { return m_szLastErrorMsg; }
    
private:
    // Prevent copying
    CDbResultSet(const CDbResultSet&);
    CDbResultSet& operator=(const CDbResultSet&);
    
    // Internal helpers
    void GetColumnMetadata();
    BOOL FetchNextRow();
    void SetErrorFromODBC(SQLSMALLINT nHandleType, SQLHANDLE hHandle);
    
    // ODBC statement handle
    HSTMT m_hStmt;
    
    // Column metadata
    int m_nColumnCount;
    std::vector<std::string> m_vecColNames;
    std::vector<int> m_vecColTypes;
    std::map<std::string, int> m_mapColNameToIndex;
    
    // Cached rows
    std::vector<CDbResultRow*> m_vecRows;
    int m_nCurrentRow;
    BOOL m_bEOF;
    BOOL m_bBOF;
    
    // Current row data (for direct access without caching)
    std::vector<char*> m_vecColData;
    std::vector<SQLLEN> m_vecColInd;
    
    // Error information
    int m_nLastErrorCode;
    char m_szLastErrorMsg[512];
    
    // Flags
    BOOL m_bFetchedAll;
    BOOL m_bOwnsHandle;
};

// Legacy typedef
typedef CDbResultSet CRecordset;

#endif // CDBRESULTSET_H

// $Log: CDbResultSet.h,v $
// Revision 1.34  2016/09/01 10:00:00  apatel
// Added column type tracking
//
// Revision 1.11  2011/03/08 09:30:00  knakamura
// Added caching support
//
// Revision 1.10  2004/03/15 10:00:00  jthompson
// Initial version
