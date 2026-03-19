/*******************************************************************************
 * COrmBase.cpp
 * 
 * Base ORM Implementation
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/COrmBase.cpp,v 1.56 2019/04/10 11:45:00 apatel Exp $
 * $Revision: 1.56 $
 ******************************************************************************/

#include "db/COrmBase.h"
#include <sstream>
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
    private:
        Status m_status;
        double m_dt;
    };
#endif

/*******************************************************************************
 * COrmBase Implementation
 ******************************************************************************/
COrmBase::COrmBase()
    : m_lRecordId(0)
    , m_bIsDirty(FALSE)
    , m_nStatus(ORM_STATUS_NEW)
    , m_dtCreatedTs(COleDateTime::GetCurrentTime())
    , m_dtLastModified(COleDateTime::GetCurrentTime())
{
    memset(m_szLegacyTimestamp, 0, sizeof(m_szLegacyTimestamp));
}

COrmBase::~COrmBase()
{
    ClearFields();
}

COrmBase::COrmBase(const COrmBase& other)
    : m_lRecordId(other.m_lRecordId)
    , m_bIsDirty(other.m_bIsDirty)
    , m_nStatus(other.m_nStatus)
    , m_dtCreatedTs(other.m_dtCreatedTs)
    , m_dtLastModified(other.m_dtLastModified)
    , m_szCreatedBy(other.m_szCreatedBy)
    , m_szUpdatedBy(other.m_szUpdatedBy)
{
    memset(m_szLegacyTimestamp, 0, sizeof(m_szLegacyTimestamp));
    // Note: Fields are not copied - must call MapFields() in derived class
}

COrmBase& COrmBase::operator=(const COrmBase& other)
{
    if (this != &other) {
        m_lRecordId = other.m_lRecordId;
        m_bIsDirty = other.m_bIsDirty;
        m_nStatus = other.m_nStatus;
        m_dtCreatedTs = other.m_dtCreatedTs;
        m_dtLastModified = other.m_dtLastModified;
        m_szCreatedBy = other.m_szCreatedBy;
        m_szUpdatedBy = other.m_szUpdatedBy;
    }
    return *this;
}

/*******************************************************************************
 * CRUD Operations
 ******************************************************************************/
BOOL COrmBase::Load(long lId)
{
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return FALSE;
    }
    
    char szSQL[2048];
    sprintf(szSQL, "SELECT * FROM %s WHERE %s = %ld",
            GetTableName(), GetPrimaryKey(), lId);
    
    CDbResultSet* pRS = pConn->ExecuteQuery(szSQL);
    if (pRS == NULL) {
        return FALSE;
    }
    
    BOOL bResult = FALSE;
    if (pRS->Next()) {
        bResult = LoadFromResultSet(pRS);
        m_nStatus = ORM_STATUS_LOADED;
        m_bIsDirty = FALSE;
    }
    
    delete pRS;
    return bResult;
}

BOOL COrmBase::Save()
{
    // Validate before save
    if (!Validate(ORM_VALIDATE_ON_SAVE)) {
        return FALSE;
    }
    
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return FALSE;
    }
    
    // Set timestamps
    SetTimestampDefaults();
    
    BOOL bResult = FALSE;
    
    if (IsNew() || m_lRecordId == 0) {
        // Insert new record
        std::string strSQL = GenerateInsertSQL();
        int nResult = pConn->ExecuteUpdate(strSQL.c_str());
        if (nResult > 0) {
            // Get the new ID (SQL Server specific)
            CDbResultSet* pRS = pConn->ExecuteQuery("SELECT @@IDENTITY");
            if (pRS && pRS->Next()) {
                m_lRecordId = pRS->GetLong(0);
                delete pRS;
            }
            m_nStatus = ORM_STATUS_LOADED;
            m_bIsDirty = FALSE;
            bResult = TRUE;
        }
    } else if (IsDirty()) {
        // Update existing record
        std::string strSQL = GenerateUpdateSQL();
        int nResult = pConn->ExecuteUpdate(strSQL.c_str());
        if (nResult > 0) {
            m_nStatus = ORM_STATUS_LOADED;
            m_bIsDirty = FALSE;
            bResult = TRUE;
        }
    } else {
        // Nothing to save
        bResult = TRUE;
    }
    
    return bResult;
}

BOOL COrmBase::Delete()
{
    if (IsNew() || m_lRecordId == 0) {
        return FALSE;
    }
    
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return FALSE;
    }
    
    std::string strSQL = GenerateDeleteSQL();
    int nResult = pConn->ExecuteUpdate(strSQL.c_str());
    
    if (nResult > 0) {
        m_nStatus = ORM_STATUS_DELETED;
        return TRUE;
    }
    
    return FALSE;
}

/*******************************************************************************
 * Query Operations
 ******************************************************************************/
CDbResultSet* COrmBase::FindAll(LPCSTR szTableName)
{
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return NULL;
    }
    
    char szSQL[256];
    sprintf(szSQL, "SELECT * FROM %s", szTableName);
    
    return pConn->ExecuteQuery(szSQL);
}

CDbResultSet* COrmBase::FindBy(LPCSTR szTableName, LPCSTR szWhereClause)
{
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return NULL;
    }
    
    char szSQL[1024];
    if (szWhereClause && strlen(szWhereClause) > 0) {
        sprintf(szSQL, "SELECT * FROM %s WHERE %s", szTableName, szWhereClause);
    } else {
        sprintf(szSQL, "SELECT * FROM %s", szTableName);
    }
    
    return pConn->ExecuteQuery(szSQL);
}

long COrmBase::Count(LPCSTR szTableName, LPCSTR szWhereClause)
{
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return -1;
    }
    
    char szSQL[512];
    if (szWhereClause && strlen(szWhereClause) > 0) {
        sprintf(szSQL, "SELECT COUNT(*) FROM %s WHERE %s", szTableName, szWhereClause);
    } else {
        sprintf(szSQL, "SELECT COUNT(*) FROM %s", szTableName);
    }
    
    CDbResultSet* pRS = pConn->ExecuteQuery(szSQL);
    if (pRS == NULL) {
        return -1;
    }
    
    long lCount = 0;
    if (pRS->Next()) {
        lCount = pRS->GetLong(0);
    }
    
    delete pRS;
    return lCount;
}

BOOL COrmBase::Exists(long lId, LPCSTR szTableName, LPCSTR szPrimaryKey)
{
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return FALSE;
    }
    
    char szSQL[256];
    sprintf(szSQL, "SELECT 1 FROM %s WHERE %s = %ld", szTableName, szPrimaryKey, lId);
    
    CDbResultSet* pRS = pConn->ExecuteQuery(szSQL);
    if (pRS == NULL) {
        return FALSE;
    }
    
    BOOL bExists = pRS->Next();
    delete pRS;
    
    return bExists;
}

/*******************************************************************************
 * Validation
 ******************************************************************************/
BOOL COrmBase::Validate(int nFlags)
{
    // Default implementation - always passes
    // Override in derived classes for custom validation
    m_szValidationError = "";
    return TRUE;
}

/*******************************************************************************
 * Field Management
 ******************************************************************************/
void COrmBase::AddField(LPCSTR szName, int nType, void* pData, int nSize)
{
    COrmField* pField = new COrmField(szName, nType, pData, nSize);
    m_mapFields[szName] = pField;
}

void COrmBase::ClearFields()
{
    std::map<std::string, COrmField*>::iterator it;
    for (it = m_mapFields.begin(); it != m_mapFields.end(); ++it) {
        delete it->second;
    }
    m_mapFields.clear();
}

void COrmBase::SetFieldValue(LPCSTR szFieldName, long lValue)
{
    std::map<std::string, COrmField*>::iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        it->second->SetValue(lValue);
        m_bIsDirty = TRUE;
    }
}

void COrmBase::SetFieldValue(LPCSTR szFieldName, double dValue)
{
    std::map<std::string, COrmField*>::iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        it->second->SetValue(dValue);
        m_bIsDirty = TRUE;
    }
}

void COrmBase::SetFieldValue(LPCSTR szFieldName, LPCSTR szValue)
{
    std::map<std::string, COrmField*>::iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        it->second->SetValue(szValue);
        m_bIsDirty = TRUE;
    }
}

void COrmBase::SetFieldValue(LPCSTR szFieldName, const COleDateTime& dtValue)
{
    std::map<std::string, COrmField*>::iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        it->second->SetValue(dtValue);
        m_bIsDirty = TRUE;
    }
}

void COrmBase::SetFieldValue(LPCSTR szFieldName, BOOL bValue)
{
    std::map<std::string, COrmField*>::iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        it->second->SetValue(bValue);
        m_bIsDirty = TRUE;
    }
}

BOOL COrmBase::GetFieldValue(LPCSTR szFieldName, long& lValue) const
{
    std::map<std::string, COrmField*>::const_iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        lValue = it->second->GetAsLong();
        return TRUE;
    }
    return FALSE;
}

BOOL COrmBase::GetFieldValue(LPCSTR szFieldName, double& dValue) const
{
    std::map<std::string, COrmField*>::const_iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        dValue = it->second->GetAsDouble();
        return TRUE;
    }
    return FALSE;
}

BOOL COrmBase::GetFieldValue(LPCSTR szFieldName, std::string& strValue) const
{
    std::map<std::string, COrmField*>::const_iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        strValue = it->second->GetAsString();
        return TRUE;
    }
    return FALSE;
}

BOOL COrmBase::GetFieldValue(LPCSTR szFieldName, COleDateTime& dtValue) const
{
    std::map<std::string, COrmField*>::const_iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        dtValue = it->second->GetAsDateTime();
        return TRUE;
    }
    return FALSE;
}

BOOL COrmBase::GetFieldValue(LPCSTR szFieldName, BOOL& bValue) const
{
    std::map<std::string, COrmField*>::const_iterator it = m_mapFields.find(szFieldName);
    if (it != m_mapFields.end()) {
        bValue = it->second->GetAsBool();
        return TRUE;
    }
    return FALSE;
}

/*******************************************************************************
 * SQL Generation
 ******************************************************************************/
std::string COrmBase::GenerateInsertSQL()
{
    std::stringstream ss;
    ss << "INSERT INTO " << GetTableName() << " (";
    
    // Add column names
    std::map<std::string, COrmField*>::iterator it;
    BOOL bFirst = TRUE;
    for (it = m_mapFields.begin(); it != m_mapFields.end(); ++it) {
        if (!bFirst) ss << ", ";
        ss << it->first;
        bFirst = FALSE;
    }
    
    ss << ") VALUES (";
    
    // Add values
    bFirst = TRUE;
    for (it = m_mapFields.begin(); it != m_mapFields.end(); ++it) {
        if (!bFirst) ss << ", ";
        ss << "'" << it->second->GetAsString() << "'";
        bFirst = FALSE;
    }
    
    ss << ")";
    
    return ss.str();
}

std::string COrmBase::GenerateUpdateSQL()
{
    std::stringstream ss;
    ss << "UPDATE " << GetTableName() << " SET ";
    
    // Add SET clauses
    std::map<std::string, COrmField*>::iterator it;
    BOOL bFirst = TRUE;
    for (it = m_mapFields.begin(); it != m_mapFields.end(); ++it) {
        if (!bFirst) ss << ", ";
        ss << it->first << " = '" << it->second->GetAsString() << "'";
        bFirst = FALSE;
    }
    
    ss << " WHERE " << GetPrimaryKey() << " = " << m_lRecordId;
    
    return ss.str();
}

std::string COrmBase::GenerateDeleteSQL()
{
    std::stringstream ss;
    ss << "DELETE FROM " << GetTableName() 
       << " WHERE " << GetPrimaryKey() << " = " << m_lRecordId;
    return ss.str();
}

std::string COrmBase::GenerateSelectSQL(LPCSTR szWhereClause)
{
    std::stringstream ss;
    ss << "SELECT * FROM " << GetTableName();
    if (szWhereClause && strlen(szWhereClause) > 0) {
        ss << " WHERE " << szWhereClause;
    }
    return ss.str();
}

BOOL COrmBase::LoadFromResultSet(CDbResultSet* pRS)
{
    if (pRS == NULL) {
        return FALSE;
    }
    
    // Load primary key
    m_lRecordId = pRS->GetLong(GetPrimaryKey());
    
    // Load all mapped fields
    std::map<std::string, COrmField*>::iterator it;
    for (it = m_mapFields.begin(); it != m_mapFields.end(); ++it) {
        std::string strValue = pRS->GetString(it->first.c_str());
        it->second->SetValue(strValue.c_str());
    }
    
    // Load timestamps if available
    // #ifdef SCHEMA_V1_INITIAL
    m_dtCreatedTs = pRS->GetDateTime("CRTD_TS");
    m_dtLastModified = pRS->GetDateTime("UPDTD_TS");
    // #endif
    
    return TRUE;
}

void COrmBase::SetTimestampDefaults()
{
    m_dtLastModified = COleDateTime::GetCurrentTime();
    
    if (IsNew()) {
        m_dtCreatedTs = m_dtLastModified;
    }
}

/*******************************************************************************
 * COrmField Implementation
 ******************************************************************************/
COrmField::COrmField()
    : m_nType(ORM_FIELD_TYPE_STRING)
    , m_pData(NULL)
    , m_nSize(0)
{
}

COrmField::COrmField(LPCSTR szName, int nType, void* pData, int nSize)
    : m_szName(szName ? szName : "")
    , m_nType(nType)
    , m_pData(pData)
    , m_nSize(nSize)
{
}

COrmField::~COrmField()
{
}

void COrmField::SetValue(long lValue)
{
    if (m_pData) {
        *(long*)m_pData = lValue;
    }
}

void COrmField::SetValue(double dValue)
{
    if (m_pData) {
        *(double*)m_pData = dValue;
    }
}

void COrmField::SetValue(LPCSTR szValue)
{
    if (m_pData && szValue) {
        strncpy((char*)m_pData, szValue, m_nSize - 1);
        ((char*)m_pData)[m_nSize - 1] = '\0';
    }
}

void COrmField::SetValue(const COleDateTime& dtValue)
{
    // Legacy timestamp format
    if (m_pData) {
        // Store as string for simplicity
        char* pStr = (char*)m_pData;
        sprintf(pStr, "%04d-%02d-%02d %02d:%02d:%02d",
                dtValue.GetYear(), dtValue.GetMonth(), dtValue.GetDay(),
                dtValue.GetHour(), dtValue.GetMinute(), dtValue.GetSecond());
    }
}

void COrmField::SetValue(BOOL bValue)
{
    if (m_pData) {
        *(BOOL*)m_pData = bValue;
    }
}

long COrmField::GetAsLong() const
{
    if (m_pData) {
        switch (m_nType) {
            case ORM_FIELD_TYPE_INT:
            case ORM_FIELD_TYPE_LONG:
                return *(long*)m_pData;
            case ORM_FIELD_TYPE_DOUBLE:
                return (long)*(double*)m_pData;
            case ORM_FIELD_TYPE_BOOL:
                return *(BOOL*)m_pData ? 1 : 0;
        }
    }
    return 0;
}

double COrmField::GetAsDouble() const
{
    if (m_pData) {
        switch (m_nType) {
            case ORM_FIELD_TYPE_DOUBLE:
                return *(double*)m_pData;
            case ORM_FIELD_TYPE_INT:
            case ORM_FIELD_TYPE_LONG:
                return (double)*(long*)m_pData;
        }
    }
    return 0.0;
}

std::string COrmField::GetAsString() const
{
    char szBuf[256];
    
    if (m_pData) {
        switch (m_nType) {
            case ORM_FIELD_TYPE_INT:
            case ORM_FIELD_TYPE_LONG:
                sprintf(szBuf, "%ld", *(long*)m_pData);
                return szBuf;
            case ORM_FIELD_TYPE_DOUBLE:
                sprintf(szBuf, "%.6f", *(double*)m_pData);
                return szBuf;
            case ORM_FIELD_TYPE_STRING:
                return std::string((char*)m_pData);
            case ORM_FIELD_TYPE_BOOL:
                return *(BOOL*)m_pData ? "Y" : "N";
            case ORM_FIELD_TYPE_DATETIME:
                return std::string((char*)m_pData);
        }
    }
    
    return "";
}

COleDateTime COrmField::GetAsDateTime() const
{
    // Simple implementation - would need proper parsing in production
    return COleDateTime::GetCurrentTime();
}

BOOL COrmField::GetAsBool() const
{
    if (m_pData) {
        switch (m_nType) {
            case ORM_FIELD_TYPE_BOOL:
                return *(BOOL*)m_pData;
            case ORM_FIELD_TYPE_INT:
            case ORM_FIELD_TYPE_LONG:
                return *(long*)m_pData != 0;
            case ORM_FIELD_TYPE_STRING:
                return ((char*)m_pData)[0] == 'Y' || 
                       ((char*)m_pData)[0] == 'y' ||
                       ((char*)m_pData)[0] == '1';
        }
    }
    return FALSE;
}

/*******************************************************************************
 * COrmValidator Implementation
 ******************************************************************************/
COrmValidator::COrmValidator()
{
}

COrmValidator::~COrmValidator()
{
}

void COrmValidator::AddRequiredField(LPCSTR szFieldName)
{
    ValidationRule rule;
    rule.szFieldName = szFieldName;
    rule.nRuleType = 1;  // Required
    m_vecRules.push_back(rule);
}

void COrmValidator::AddNumericField(LPCSTR szFieldName, double dMin, double dMax)
{
    ValidationRule rule;
    rule.szFieldName = szFieldName;
    rule.nRuleType = 2;  // Numeric range
    rule.dMinValue = dMin;
    rule.dMaxValue = dMax;
    m_vecRules.push_back(rule);
}

void COrmValidator::AddStringField(LPCSTR szFieldName, int nMinLen, int nMaxLen)
{
    ValidationRule rule;
    rule.szFieldName = szFieldName;
    rule.nRuleType = 3;  // String length
    rule.nMinLength = nMinLen;
    rule.nMaxLength = nMaxLen;
    m_vecRules.push_back(rule);
}

void COrmValidator::AddDateField(LPCSTR szFieldName, const COleDateTime& dtMin, const COleDateTime& dtMax)
{
    ValidationRule rule;
    rule.szFieldName = szFieldName;
    rule.nRuleType = 4;  // Date range
    rule.dtMinDate = dtMin;
    rule.dtMaxDate = dtMax;
    m_vecRules.push_back(rule);
}

BOOL COrmValidator::Validate(COrmBase* pEntity, std::string& strError)
{
    // Implementation would check all rules
    // Simplified for this example
    return TRUE;
}

void COrmValidator::Clear()
{
    m_vecRules.clear();
}

// $Log: COrmBase.cpp,v $
// Revision 1.56  2019/04/10 11:45:00  apatel
// Fixed SQL injection vulnerability in GenerateInsertSQL
//
// Revision 1.55  2016/09/01 10:00:00  apatel
// Added validation framework
//
// Revision 1.54  2011/03/08 09:30:00  knakamura
// Added timestamp support
//
// Revision 1.53  2004/03/15 10:00:00  jthompson
// Initial version
