/*******************************************************************************
 * CDBConnection.h
 * 
 * Database Connection Manager - Singleton
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CDBConnection.h,v 1.67 2016/08/22 14:30:00 jthompson Exp $
 * $Revision: 1.67 $
 * 
 * HISTORY:
 * 2004-03-15 - jthompson - Initial version (ODBC 2.x)
 * 2007-05-22 - mrodriguez - Added connection pooling
 * 2011-03-08 - knakamura - Added transaction support
 * 2016-09-01 - apatel - Added async query support
 * 
 * IMPORTANT: This class uses ODBC handles directly. Do not mix with ADO/OLEDB.
 ******************************************************************************/

#ifndef CDBCONNECTION_H
#define CDBCONNECTION_H

// Legacy compiler support
#if _MSC_VER >= 1600
    #define COMPILER_VS2010_OR_LATER
#endif

#ifdef _WIN32
    #include <windows.h>
    #include <sql.h>
    #include <sqlext.h>
    #include <sqltypes.h>
#else
    // Unix ODBC support (deprecated as of 2012)
    #include <sql.h>
    #include <sqlext.h>
#endif

#include <string>
#include <vector>
#include <map>

// Legacy macro definitions
#define MAX_CONNECTION_STRING_LEN  1024
#define MAX_ERROR_MSG_LEN          512
#define MAX_SQL_STMT_LEN           4096
#define DEFAULT_QUERY_TIMEOUT      30
#define DEFAULT_LOGIN_TIMEOUT      15

// Connection status codes
#define DB_CONN_STATUS_DISCONNECTED    0
#define DB_CONN_STATUS_CONNECTED       1
#define DB_CONN_STATUS_BUSY            2
#define DB_CONN_STATUS_ERROR           99

// Transaction isolation levels (Sybase/SQL Server compatible)
#define DB_TXN_READ_UNCOMMITTED    SQL_TXN_READ_UNCOMMITTED
#define DB_TXN_READ_COMMITTED      SQL_TXN_READ_COMMITTED
#define DB_TXN_REPEATABLE_READ     SQL_TXN_REPEATABLE_READ
#define DB_TXN_SERIALIZABLE        SQL_TXN_SERIALIZABLE

// Legacy error codes
#define DB_ERR_SUCCESS             0
#define DB_ERR_CONNECTION_FAILED   1001
#define DB_ERR_LOGIN_FAILED        1002
#define DB_ERR_QUERY_FAILED        1003
#define DB_ERR_NO_DATA             1004
#define DB_ERR_TIMEOUT             1005
#define DB_ERR_DEADLOCK            1205
#define DB_ERR_CONSTRAINT          547
#define DB_ERR_DUPLICATE_KEY       2601

// Forward declarations
class CDbException;
class CDbResultSet;

/*******************************************************************************
 * CDBConnection - Singleton Database Connection Manager
 * 
 * Usage:
 *   CDBConnection* pConn = CDBConnection::GetInstance();
 *   if (pConn->Connect("DSN=trading_db;UID=trader;PWD=xxx")) {
 *       CDbResultSet* pRS = pConn->ExecuteQuery("SELECT * FROM TRD_BLOTTER");
 *       // ...
 *   }
 ******************************************************************************/
class CDBConnection
{
public:
    // Singleton accessor
    static CDBConnection* GetInstance();
    static void DestroyInstance();
    
    // Connection management
    BOOL Connect(LPCSTR szConnectionString);
    BOOL Connect(LPCSTR szDSN, LPCSTR szUser, LPCSTR szPassword);
    BOOL Disconnect();
    BOOL IsConnected() const;
    
    // Connection properties
    LPCSTR GetConnectionString() const { return m_szConnectionString; }
    int GetConnectionStatus() const { return m_nConnectionStatus; }
    LPCSTR GetDatabaseName() const { return m_szDatabaseName; }
    LPCSTR GetServerName() const { return m_szServerName; }
    
    // Query execution
    CDbResultSet* ExecuteQuery(LPCSTR szSQL);
    int ExecuteUpdate(LPCSTR szSQL);
    int ExecuteStoredProc(LPCSTR szProcName);
    
    // Prepared statement support (added 2007)
    HSTMT PrepareStatement(LPCSTR szSQL);
    BOOL BindParameter(HSTMT hStmt, int nParam, int nType, LPVOID pData, int nLen);
    CDbResultSet* ExecutePrepared(HSTMT hStmt);
    void CloseStatement(HSTMT hStmt);
    
    // Transaction support (added 2011)
    BOOL BeginTransaction();
    BOOL CommitTransaction();
    BOOL RollbackTransaction();
    BOOL IsInTransaction() const { return m_bInTransaction; }
    
    // Transaction isolation level
    BOOL SetTransactionIsolation(int nLevel);
    int GetTransactionIsolation() const { return m_nTxnIsolationLevel; }
    
    // Timeout settings
    void SetQueryTimeout(int nSeconds) { m_nQueryTimeout = nSeconds; }
    int GetQueryTimeout() const { return m_nQueryTimeout; }
    void SetLoginTimeout(int nSeconds) { m_nLoginTimeout = nSeconds; }
    int GetLoginTimeout() const { return m_nLoginTimeout; }
    
    // Error handling
    int GetLastErrorCode() const { return m_nLastErrorCode; }
    LPCSTR GetLastErrorMsg() const { return m_szLastErrorMsg; }
    void ClearLastError();
    
    // Handle access (for legacy code)
    HENV GetEnvironmentHandle() const { return m_hEnv; }
    HDBC GetConnectionHandle() const { return m_hDbc; }
    
    // Connection pooling (added 2007)
    static BOOL EnableConnectionPooling(BOOL bEnable = TRUE);
    static BOOL IsConnectionPoolingEnabled() { return m_bConnectionPooling; }
    
    // Utility functions
    static std::string EscapeString(LPCSTR szInput);
    static std::string FormatDateTime(const COleDateTime& dt);
    static BOOL ValidateConnectionString(LPCSTR szConnectionString);
    
private:
    // Private constructor for singleton
    CDBConnection();
    ~CDBConnection();
    
    // Prevent copying
    CDBConnection(const CDBConnection&);
    CDBConnection& operator=(const CDBConnection&);
    
    // Internal initialization
    BOOL InitializeODBC();
    void CleanupODBC();
    BOOL AllocateHandles();
    void FreeHandles();
    void SetErrorFromODBC(SQLSMALLINT nHandleType, SQLHANDLE hHandle);
    
    // Singleton instance
    static CDBConnection* m_pInstance;
    static BOOL m_bConnectionPooling;
    
    // ODBC handles
    HENV m_hEnv;
    HDBC m_hDbc;
    
    // Connection properties
    char m_szConnectionString[MAX_CONNECTION_STRING_LEN];
    char m_szDatabaseName[64];
    char m_szServerName[128];
    int m_nConnectionStatus;
    
    // Timeout settings
    int m_nQueryTimeout;
    int m_nLoginTimeout;
    
    // Transaction state
    BOOL m_bInTransaction;
    int m_nTxnIsolationLevel;
    
    // Error information
    int m_nLastErrorCode;
    char m_szLastErrorMsg[MAX_ERROR_MSG_LEN];
    
    // Active statements (for cleanup)
    std::vector<HSTMT> m_vecActiveStatements;
    
    // Connection statistics (for debugging)
    long m_lQueryCount;
    long m_lConnectCount;
    time_t m_tLastConnectTime;
    
    // Legacy compatibility members
    // #ifdef LEGACY_SYBASE_SUPPORT
    BOOL m_bSybaseMode;
    char m_szSybaseServer[64];
    // #endif
};

/*******************************************************************************
 * Inline helper functions for legacy code compatibility
 ******************************************************************************/
inline CDBConnection* GetDBConnection() { return CDBConnection::GetInstance(); }
inline BOOL DBConnect(LPCSTR szDSN, LPCSTR szUser, LPCSTR szPwd) {
    return CDBConnection::GetInstance()->Connect(szDSN, szUser, szPwd);
}
inline void DBDisconnect() { CDBConnection::GetInstance()->Disconnect(); }
inline CDbResultSet* DBQuery(LPCSTR szSQL) {
    return CDBConnection::GetInstance()->ExecuteQuery(szSQL);
}
inline int DBUpdate(LPCSTR szSQL) {
    return CDBConnection::GetInstance()->ExecuteUpdate(szSQL);
}

// Legacy macros (deprecated but kept for compatibility)
#define DB_GET_INSTANCE()           CDBConnection::GetInstance()
#define DB_CONNECT(dsn,user,pwd)    CDBConnection::GetInstance()->Connect(dsn,user,pwd)
#define DB_DISCONNECT()             CDBConnection::GetInstance()->Disconnect()
#define DB_QUERY(sql)               CDBConnection::GetInstance()->ExecuteQuery(sql)
#define DB_UPDATE(sql)              CDBConnection::GetInstance()->ExecuteUpdate(sql)
#define DB_LAST_ERROR()             CDBConnection::GetInstance()->GetLastErrorMsg()

#endif // CDBCONNECTION_H

// $Log: CDBConnection.h,v $
// Revision 1.67  2016/08/22 14:30:00  jthompson
// Added async query support and connection pooling improvements
//
// Revision 1.66  2011/03/08 09:30:00  knakamura
// Added transaction isolation level support
//
// Revision 1.65  2007/05/22 14:00:00  mrodriguez
// Added prepared statement support
//
// Revision 1.64  2004/03/15 10:00:00  jthompson
// Initial version
