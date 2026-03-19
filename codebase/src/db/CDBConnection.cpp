/*******************************************************************************
 * CDBConnection.cpp
 * 
 * Database Connection Manager Implementation
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CDBConnection.cpp,v 1.89 2019/04/10 11:45:00 apatel Exp $
 * $Revision: 1.89 $
 ******************************************************************************/

#include "db/CDBConnection.h"
#include "db/CDbException.h"
#include "db/CDbResultSet.h"
#include <ctime>
#include <cstring>
#include <sstream>

// MFC support for COleDateTime (legacy)
#ifdef _MFC_VER
    #include <afxdisp.h>
#else
    // Minimal replacement for non-MFC builds
    #include <ctime>
#endif

// Static member initialization
CDBConnection* CDBConnection::m_pInstance = NULL;
BOOL CDBConnection::m_bConnectionPooling = FALSE;

/*******************************************************************************
 * Singleton Implementation
 ******************************************************************************/
CDBConnection* CDBConnection::GetInstance()
{
    // Thread safety note: This is not thread-safe!
    // In production, use CriticalSection or static initialization
    // TODO: Add thread safety (CR-2014-0522)
    if (m_pInstance == NULL) {
        m_pInstance = new CDBConnection();
    }
    return m_pInstance;
}

void CDBConnection::DestroyInstance()
{
    if (m_pInstance != NULL) {
        if (m_pInstance->IsConnected()) {
            m_pInstance->Disconnect();
        }
        delete m_pInstance;
        m_pInstance = NULL;
    }
}

/*******************************************************************************
 * Constructor / Destructor
 ******************************************************************************/
CDBConnection::CDBConnection()
    : m_hEnv(SQL_NULL_HENV)
    , m_hDbc(SQL_NULL_HDBC)
    , m_nConnectionStatus(DB_CONN_STATUS_DISCONNECTED)
    , m_nQueryTimeout(DEFAULT_QUERY_TIMEOUT)
    , m_nLoginTimeout(DEFAULT_LOGIN_TIMEOUT)
    , m_bInTransaction(FALSE)
    , m_nTxnIsolationLevel(DB_TXN_READ_COMMITTED)
    , m_nLastErrorCode(DB_ERR_SUCCESS)
    , m_lQueryCount(0)
    , m_lConnectCount(0)
    , m_tLastConnectTime(0)
    , m_bSybaseMode(FALSE)
{
    memset(m_szConnectionString, 0, sizeof(m_szConnectionString));
    memset(m_szDatabaseName, 0, sizeof(m_szDatabaseName));
    memset(m_szServerName, 0, sizeof(m_szServerName));
    memset(m_szLastErrorMsg, 0, sizeof(m_szLastErrorMsg));
    memset(m_szSybaseServer, 0, sizeof(m_szSybaseServer));
    
    InitializeODBC();
}

CDBConnection::~CDBConnection()
{
    // Clean up any active statements
    for (size_t i = 0; i < m_vecActiveStatements.size(); i++) {
        if (m_vecActiveStatements[i] != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, m_vecActiveStatements[i]);
        }
    }
    m_vecActiveStatements.clear();
    
    if (m_bInTransaction) {
        RollbackTransaction();
    }
    
    Disconnect();
    CleanupODBC();
}

/*******************************************************************************
 * ODBC Initialization
 ******************************************************************************/
BOOL CDBConnection::InitializeODBC()
{
    // Allocate environment handle
    SQLRETURN nRet = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);
    if (!SQL_SUCCEEDED(nRet)) {
        strcpy(m_szLastErrorMsg, "Failed to allocate ODBC environment handle");
        m_nLastErrorCode = DB_ERR_CONNECTION_FAILED;
        return FALSE;
    }
    
    // Set ODBC version (use 2.x for legacy compatibility)
    // NOTE: Changed to 3.x in 2011 for better SQL Server 2008 support
    // #ifdef USE_ODBC_VERSION_2
    //     nRet = SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC2, 0);
    // #else
    nRet = SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    // #endif
    
    if (!SQL_SUCCEEDED(nRet)) {
        strcpy(m_szLastErrorMsg, "Failed to set ODBC version");
        m_nLastErrorCode = DB_ERR_CONNECTION_FAILED;
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hEnv = SQL_NULL_HENV;
        return FALSE;
    }
    
    // Enable connection pooling if requested
    if (m_bConnectionPooling) {
        SQLSetEnvAttr(m_hEnv, SQL_ATTR_CONNECTION_POOLING, 
                      (void*)SQL_CP_ONE_PER_HENV, 0);
    }
    
    return TRUE;
}

void CDBConnection::CleanupODBC()
{
    if (m_hEnv != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hEnv = SQL_NULL_HENV;
    }
}

BOOL CDBConnection::AllocateHandles()
{
    if (m_hEnv == SQL_NULL_HENV) {
        if (!InitializeODBC()) {
            return FALSE;
        }
    }
    
    // Allocate connection handle
    SQLRETURN nRet = SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hDbc);
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_ENV, m_hEnv);
        return FALSE;
    }
    
    // Set login timeout
    SQLSetConnectAttr(m_hDbc, SQL_ATTR_LOGIN_TIMEOUT, 
                      (SQLPOINTER)(size_t)m_nLoginTimeout, 0);
    
    // Set query timeout
    SQLSetConnectAttr(m_hDbc, SQL_ATTR_QUERY_TIMEOUT,
                      (SQLPOINTER)(size_t)m_nQueryTimeout, 0);
    
    return TRUE;
}

void CDBConnection::FreeHandles()
{
    if (m_hDbc != SQL_NULL_HDBC) {
        SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
        m_hDbc = SQL_NULL_HDBC;
    }
}

/*******************************************************************************
 * Connection Management
 ******************************************************************************/
BOOL CDBConnection::Connect(LPCSTR szConnectionString)
{
    if (IsConnected()) {
        Disconnect();
    }
    
    ClearLastError();
    
    // Validate connection string
    if (!ValidateConnectionString(szConnectionString)) {
        strcpy(m_szLastErrorMsg, "Invalid connection string");
        m_nLastErrorCode = DB_ERR_CONNECTION_FAILED;
        return FALSE;
    }
    
    // Store connection string
    strncpy(m_szConnectionString, szConnectionString, MAX_CONNECTION_STRING_LEN - 1);
    
    // Allocate handles
    if (!AllocateHandles()) {
        return FALSE;
    }
    
    // Connect to database
    SQLCHAR szOutConnStr[1024];
    SQLSMALLINT nOutConnStrLen = 0;
    
    SQLRETURN nRet = SQLDriverConnect(
        m_hDbc,
        NULL,  // hWnd (no dialog)
        (SQLCHAR*)szConnectionString,
        SQL_NTS,
        szOutConnStr,
        sizeof(szOutConnStr),
        &nOutConnStrLen,
        SQL_DRIVER_COMPLETE
    );
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_DBC, m_hDbc);
        FreeHandles();
        return FALSE;
    }
    
    // Get connection info
    SQLGetInfo(m_hDbc, SQL_DATABASE_NAME, m_szDatabaseName, sizeof(m_szDatabaseName), NULL);
    SQLGetInfo(m_hDbc, SQL_SERVER_NAME, m_szServerName, sizeof(m_szServerName), NULL);
    
    // Check for Sybase (legacy support)
    // #ifdef LEGACY_SYBASE_SUPPORT
    if (strstr(m_szServerName, "SYBASE") != NULL || 
        strstr(m_szConnectionString, "SYBASE") != NULL) {
        m_bSybaseMode = TRUE;
        strcpy(m_szSybaseServer, m_szServerName);
    }
    // #endif
    
    m_nConnectionStatus = DB_CONN_STATUS_CONNECTED;
    m_lConnectCount++;
    m_tLastConnectTime = time(NULL);
    
    return TRUE;
}

BOOL CDBConnection::Connect(LPCSTR szDSN, LPCSTR szUser, LPCSTR szPassword)
{
    char szConnStr[MAX_CONNECTION_STRING_LEN];
    sprintf(szConnStr, "DSN=%s;UID=%s;PWD=%s", szDSN, szUser, szPassword);
    return Connect(szConnStr);
}

BOOL CDBConnection::Disconnect()
{
    if (m_hDbc == SQL_NULL_HDBC) {
        return TRUE;
    }
    
    // Rollback any pending transaction
    if (m_bInTransaction) {
        SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_ROLLBACK);
        m_bInTransaction = FALSE;
    }
    
    // Disconnect
    SQLRETURN nRet = SQLDisconnect(m_hDbc);
    
    // Free handles
    FreeHandles();
    
    m_nConnectionStatus = DB_CONN_STATUS_DISCONNECTED;
    memset(m_szDatabaseName, 0, sizeof(m_szDatabaseName));
    memset(m_szServerName, 0, sizeof(m_szServerName));
    
    return SQL_SUCCEEDED(nRet);
}

BOOL CDBConnection::IsConnected() const
{
    if (m_hDbc == SQL_NULL_HDBC) {
        return FALSE;
    }
    
    // Test connection with simple query
    // NOTE: This is expensive, but reliable
    // TODO: Use SQL_ATTR_CONNECTION_DEAD for SQL Server (CR-2015-0811)
    return m_nConnectionStatus == DB_CONN_STATUS_CONNECTED;
}

/*******************************************************************************
 * Query Execution
 ******************************************************************************/
CDbResultSet* CDBConnection::ExecuteQuery(LPCSTR szSQL)
{
    ClearLastError();
    
    if (!IsConnected()) {
        strcpy(m_szLastErrorMsg, "Not connected to database");
        m_nLastErrorCode = DB_ERR_CONNECTION_FAILED;
        return NULL;
    }
    
    // Allocate statement handle
    HSTMT hStmt = SQL_NULL_HSTMT;
    SQLRETURN nRet = SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_DBC, m_hDbc);
        return NULL;
    }
    
    // Set statement timeout
    SQLSetStmtAttr(hStmt, SQL_ATTR_QUERY_TIMEOUT, 
                   (SQLPOINTER)(size_t)m_nQueryTimeout, 0);
    
    // Execute query
    nRet = SQLExecDirect(hStmt, (SQLCHAR*)szSQL, SQL_NTS);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_STMT, hStmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return NULL;
    }
    
    m_lQueryCount++;
    
    // Create result set
    CDbResultSet* pRS = new CDbResultSet(hStmt);
    pRS->FetchResults();
    
    return pRS;
}

int CDBConnection::ExecuteUpdate(LPCSTR szSQL)
{
    ClearLastError();
    
    if (!IsConnected()) {
        strcpy(m_szLastErrorMsg, "Not connected to database");
        m_nLastErrorCode = DB_ERR_CONNECTION_FAILED;
        return -1;
    }
    
    HSTMT hStmt = SQL_NULL_HSTMT;
    SQLRETURN nRet = SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_DBC, m_hDbc);
        return -1;
    }
    
    nRet = SQLExecDirect(hStmt, (SQLCHAR*)szSQL, SQL_NTS);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_STMT, hStmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return -1;
    }
    
    // Get row count
    SQLLEN nRowCount = 0;
    SQLRowCount(hStmt, &nRowCount);
    
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    m_lQueryCount++;
    
    return (int)nRowCount;
}

int CDBConnection::ExecuteStoredProc(LPCSTR szProcName)
{
    char szSQL[MAX_SQL_STMT_LEN];
    sprintf(szSQL, "{call %s}", szProcName);
    return ExecuteUpdate(szSQL);
}

/*******************************************************************************
 * Prepared Statement Support (Added 2007)
 ******************************************************************************/
HSTMT CDBConnection::PrepareStatement(LPCSTR szSQL)
{
    ClearLastError();
    
    if (!IsConnected()) {
        strcpy(m_szLastErrorMsg, "Not connected to database");
        m_nLastErrorCode = DB_ERR_CONNECTION_FAILED;
        return SQL_NULL_HSTMT;
    }
    
    HSTMT hStmt = SQL_NULL_HSTMT;
    SQLRETURN nRet = SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_DBC, m_hDbc);
        return SQL_NULL_HSTMT;
    }
    
    nRet = SQLPrepare(hStmt, (SQLCHAR*)szSQL, SQL_NTS);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_STMT, hStmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return SQL_NULL_HSTMT;
    }
    
    m_vecActiveStatements.push_back(hStmt);
    return hStmt;
}

BOOL CDBConnection::BindParameter(HSTMT hStmt, int nParam, int nType, 
                                   LPVOID pData, int nLen)
{
    SQLSMALLINT nCType = SQL_C_CHAR;
    SQLSMALLINT nSqlType = SQL_CHAR;
    SQLULEN nColSize = nLen;
    SQLSMALLINT nDecimalDigits = 0;
    SQLLEN nInd = nLen;
    
    switch (nType) {
        case SQL_INTEGER:
            nCType = SQL_C_LONG;
            nSqlType = SQL_INTEGER;
            break;
        case SQL_DOUBLE:
            nCType = SQL_C_DOUBLE;
            nSqlType = SQL_DOUBLE;
            break;
        case SQL_TIMESTAMP:
            nCType = SQL_C_TIMESTAMP;
            nSqlType = SQL_TIMESTAMP;
            break;
        default:
            nCType = SQL_C_CHAR;
            nSqlType = SQL_VARCHAR;
            break;
    }
    
    SQLRETURN nRet = SQLBindParameter(
        hStmt, nParam, SQL_PARAM_INPUT,
        nCType, nSqlType, nColSize, nDecimalDigits,
        pData, nLen, &nInd
    );
    
    return SQL_SUCCEEDED(nRet);
}

CDbResultSet* CDBConnection::ExecutePrepared(HSTMT hStmt)
{
    ClearLastError();
    
    SQLRETURN nRet = SQLExecute(hStmt);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_STMT, hStmt);
        return NULL;
    }
    
    m_lQueryCount++;
    
    CDbResultSet* pRS = new CDbResultSet(hStmt);
    pRS->FetchResults();
    
    return pRS;
}

void CDBConnection::CloseStatement(HSTMT hStmt)
{
    if (hStmt != SQL_NULL_HSTMT) {
        // Remove from active list
        for (size_t i = 0; i < m_vecActiveStatements.size(); i++) {
            if (m_vecActiveStatements[i] == hStmt) {
                m_vecActiveStatements.erase(m_vecActiveStatements.begin() + i);
                break;
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    }
}

/*******************************************************************************
 * Transaction Support (Added 2011)
 ******************************************************************************/
BOOL CDBConnection::BeginTransaction()
{
    ClearLastError();
    
    if (!IsConnected()) {
        strcpy(m_szLastErrorMsg, "Not connected to database");
        m_nLastErrorCode = DB_ERR_CONNECTION_FAILED;
        return FALSE;
    }
    
    if (m_bInTransaction) {
        strcpy(m_szLastErrorMsg, "Already in transaction");
        return FALSE;
    }
    
    // Disable auto-commit
    SQLRETURN nRet = SQLSetConnectAttr(
        m_hDbc, SQL_ATTR_AUTOCOMMIT, 
        (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0
    );
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_DBC, m_hDbc);
        return FALSE;
    }
    
    m_bInTransaction = TRUE;
    return TRUE;
}

BOOL CDBConnection::CommitTransaction()
{
    ClearLastError();
    
    if (!m_bInTransaction) {
        strcpy(m_szLastErrorMsg, "Not in transaction");
        return FALSE;
    }
    
    SQLRETURN nRet = SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_COMMIT);
    
    if (!SQL_SUCCEEDED(nRet)) {
        SetErrorFromODBC(SQL_HANDLE_DBC, m_hDbc);
        return FALSE;
    }
    
    // Re-enable auto-commit
    SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, 
                      (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
    
    m_bInTransaction = FALSE;
    return TRUE;
}

BOOL CDBConnection::RollbackTransaction()
{
    ClearLastError();
    
    if (!m_bInTransaction) {
        return TRUE;  // Nothing to rollback
    }
    
    SQLRETURN nRet = SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_ROLLBACK);
    
    // Re-enable auto-commit
    SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, 
                      (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
    
    m_bInTransaction = FALSE;
    return SQL_SUCCEEDED(nRet);
}

BOOL CDBConnection::SetTransactionIsolation(int nLevel)
{
    ClearLastError();
    
    if (!IsConnected()) {
        return FALSE;
    }
    
    SQLRETURN nRet = SQLSetConnectAttr(
        m_hDbc, SQL_ATTR_TXN_ISOLATION,
        (SQLPOINTER)(size_t)nLevel, 0
    );
    
    if (SQL_SUCCEEDED(nRet)) {
        m_nTxnIsolationLevel = nLevel;
        return TRUE;
    }
    
    SetErrorFromODBC(SQL_HANDLE_DBC, m_hDbc);
    return FALSE;
}

/*******************************************************************************
 * Error Handling
 ******************************************************************************/
void CDBConnection::SetErrorFromODBC(SQLSMALLINT nHandleType, SQLHANDLE hHandle)
{
    SQLCHAR szState[6];
    SQLCHAR szMsg[MAX_ERROR_MSG_LEN];
    SQLINTEGER nNativeError;
    SQLSMALLINT nMsgLen;
    
    SQLGetDiagRec(nHandleType, hHandle, 1, szState, &nNativeError,
                  szMsg, MAX_ERROR_MSG_LEN, &nMsgLen);
    
    m_nLastErrorCode = nNativeError;
    strcpy(m_szLastErrorMsg, (char*)szMsg);
    
    // Check for specific error conditions
    if (strcmp((char*)szState, "S1T00") == 0 || strcmp((char*)szState, "HYT00") == 0) {
        m_nLastErrorCode = DB_ERR_TIMEOUT;
    }
    else if (strcmp((char*)szState, "40001") == 0) {
        m_nLastErrorCode = DB_ERR_DEADLOCK;
    }
    else if (strcmp((char*)szState, "23000") == 0) {
        if (nNativeError == 2601) {
            m_nLastErrorCode = DB_ERR_DUPLICATE_KEY;
        } else {
            m_nLastErrorCode = DB_ERR_CONSTRAINT;
        }
    }
}

void CDBConnection::ClearLastError()
{
    m_nLastErrorCode = DB_ERR_SUCCESS;
    memset(m_szLastErrorMsg, 0, sizeof(m_szLastErrorMsg));
}

/*******************************************************************************
 * Utility Functions
 ******************************************************************************/
std::string CDBConnection::EscapeString(LPCSTR szInput)
{
    std::string strResult;
    
    if (szInput == NULL) {
        return strResult;
    }
    
    while (*szInput) {
        if (*szInput == '\'') {
            strResult += "''";  // Escape single quote
        }
        // #ifdef LEGACY_SYBASE_SUPPORT
        else if (*szInput == '[') {
            strResult += "[[]";  // Sybase special handling
        }
        // #endif
        else {
            strResult += *szInput;
        }
        szInput++;
    }
    
    return strResult;
}

std::string CDBConnection::FormatDateTime(const COleDateTime& dt)
{
    if (dt.GetStatus() != COleDateTime::valid) {
        return "";
    }
    
    char szBuf[32];
    sprintf(szBuf, "'%04d-%02d-%02d %02d:%02d:%02d'",
            dt.GetYear(), dt.GetMonth(), dt.GetDay(),
            dt.GetHour(), dt.GetMinute(), dt.GetSecond());
    
    return std::string(szBuf);
}

BOOL CDBConnection::ValidateConnectionString(LPCSTR szConnectionString)
{
    if (szConnectionString == NULL || strlen(szConnectionString) == 0) {
        return FALSE;
    }
    
    // Basic validation - must contain DSN or DRIVER
    if (strstr(szConnectionString, "DSN=") == NULL &&
        strstr(szConnectionString, "DRIVER=") == NULL &&
        strstr(szConnectionString, "SERVER=") == NULL) {
        return FALSE;
    }
    
    return TRUE;
}

BOOL CDBConnection::EnableConnectionPooling(BOOL bEnable)
{
    m_bConnectionPooling = bEnable;
    
    // Connection pooling must be set before any connections are made
    // This is a global ODBC setting
    if (bEnable) {
        SQLSetEnvAttr(SQL_NULL_HENV, SQL_ATTR_CONNECTION_POOLING,
                      (SQLPOINTER)SQL_CP_ONE_PER_HENV, SQL_IS_INTEGER);
    } else {
        SQLSetEnvAttr(SQL_NULL_HENV, SQL_ATTR_CONNECTION_POOLING,
                      (SQLPOINTER)SQL_CP_OFF, SQL_IS_INTEGER);
    }
    
    return TRUE;
}

// $Log: CDBConnection.cpp,v $
// Revision 1.89  2019/04/10 11:45:00  apatel
// Fixed memory leak in prepared statement handling
//
// Revision 1.88  2016/08/22 14:30:00  jthompson
// Added connection pooling support
//
// Revision 1.87  2011/03/08 09:30:00  knakamura
// Added transaction support
//
// Revision 1.86  2007/05/22 14:00:00  mrodriguez
// Added prepared statement support
//
// Revision 1.85  2004/03/15 10:00:00  jthompson
// Initial version
