/*******************************************************************************
 * CDbException.h
 * 
 * Database Exception Class
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CDbException.h,v 1.23 2016/09/01 10:00:00 apatel Exp $
 * $Revision: 1.23 $
 ******************************************************************************/

#ifndef CDBEXCEPTION_H
#define CDBEXCEPTION_H

#include <string>
#include <exception>

// Exception error codes
#define DB_EXC_GENERAL_ERROR        0
#define DB_EXC_CONNECTION_FAILED    1001
#define DB_EXC_LOGIN_FAILED         1002
#define DB_EXC_QUERY_FAILED         1003
#define DB_EXC_TIMEOUT              1004
#define DB_EXC_DEADLOCK             1005
#define DB_EXC_CONSTRAINT_VIOLATION 1006
#define DB_EXC_UNIQUE_VIOLATION     1007
#define DB_EXC_FOREIGN_KEY          1008
#define DB_EXC_NULL_VIOLATION       1009
#define DB_EXC_TRANSACTION_FAILED   1010
#define DB_EXC_NOT_CONNECTED        1011
#define DB_EXC_NO_DATA              1012
#define DB_EXC_INVALID_PARAM        1013
#define DB_EXC_OUT_OF_MEMORY        1014
#define DB_EXC_CURSOR_ERROR         1015
#define DB_EXC_BULK_INSERT_FAILED   1016
#define DB_EXC_VALIDATION_ERROR     1017
#define DB_EXC_OPTIMISTIC_LOCK      1018
#define DB_EXC_SCHEMA_MISMATCH      1019

// Legacy macros for exception throwing
#define THROW_DB_EXCEPTION(code, msg) throw CDbException(code, msg, __FILE__, __LINE__)
#define THROW_DB_EX_IF(cond, code, msg) if (cond) throw CDbException(code, msg, __FILE__, __LINE__)
#define DB_CHECK_CONNECTION(conn) \
    if (!(conn)->IsConnected()) { \
        throw CDbException(DB_EXC_NOT_CONNECTED, "Not connected to database", __FILE__, __LINE__); \
    }

/*******************************************************************************
 * CDbException - Database Exception Class
 * 
 * Usage:
 *   try {
 *       CDbResultSet* pRS = pConn->ExecuteQuery("SELECT ...");
 *   }
 *   catch (CDbException& e) {
 *       printf("Error %d: %s\n", e.GetErrorCode(), e.GetErrorMsg());
 *   }
 ******************************************************************************/
class CDbException : public std::exception
{
public:
    // Constructors
    CDbException();
    CDbException(int nErrorCode, LPCSTR szErrorMsg);
    CDbException(int nErrorCode, LPCSTR szErrorMsg, LPCSTR szFile, int nLine);
    CDbException(const CDbException& other);
    
    // Destructor
    virtual ~CDbException() throw();
    
    // Assignment
    CDbException& operator=(const CDbException& other);
    
    // Accessors
    int GetErrorCode() const { return m_nErrorCode; }
    LPCSTR GetErrorMsg() const { return m_szErrorMsg; }
    LPCSTR GetFile() const { return m_szFile; }
    int GetLine() const { return m_nLine; }
    LPCSTR GetSQLState() const { return m_szSQLState; }
    LPCSTR GetProcedure() const { return m_szProcedure; }
    
    // std::exception override
    virtual const char* what() const throw() { return m_szErrorMsg; }
    
    // Set additional info
    void SetSQLState(LPCSTR szState);
    void SetProcedure(LPCSTR szProc);
    void SetNativeError(int nError) { m_nNativeError = nError; }
    
    // Utility methods
    BOOL IsConnectionError() const;
    BOOL IsDeadlock() const;
    BOOL IsTimeout() const;
    BOOL IsConstraintViolation() const;
    BOOL IsRecoverable() const;
    
    // Get formatted error message
    std::string GetFormattedMessage() const;
    
    // Static helper to create from ODBC error
    static CDbException FromODBC(SQLSMALLINT nHandleType, SQLHANDLE hHandle);
    
private:
    int m_nErrorCode;
    char m_szErrorMsg[512];
    char m_szFile[256];
    int m_nLine;
    
    // Additional error info
    char m_szSQLState[8];
    char m_szProcedure[128];
    int m_nNativeError;
};

// Legacy exception class aliases
typedef CDbException CDatabaseException;
typedef CDbException CSqlException;

// Legacy macros
#define DB_EXCEPTION CDbException

#endif // CDBEXCEPTION_H

// $Log: CDbException.h,v $
// Revision 1.23  2016/09/01 10:00:00  apatel
// Added optimistic lock and schema mismatch errors
//
// Revision 1.22  2011/03/08 09:30:00  knakamura
// Added additional error codes
//
// Revision 1.21  2004/03/15 10:00:00  jthompson
// Initial version
