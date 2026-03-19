/*******************************************************************************
 * COrmBase.h
 * 
 * Base ORM (Object-Relational Mapping) Class
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/COrmBase.h,v 1.45 2016/09/01 10:00:00 apatel Exp $
 * $Revision: 1.45 $
 * 
 * HISTORY:
 * 2004-03-15 - jthompson - Initial version
 * 2007-05-22 - mrodriguez - Added dirty tracking
 * 2011-03-08 - knakamura - Added timestamp support
 * 2016-09-01 - apatel - Added validation framework
 ******************************************************************************/

#ifndef CORMBASE_H
#define CORMBASE_H

#include "db/CDBConnection.h"
#include "db/CDbResultSet.h"
#include "db/CDbException.h"
#include <string>
#include <vector>
#include <map>

// ORM Field types
#define ORM_FIELD_TYPE_INT       1
#define ORM_FIELD_TYPE_LONG      2
#define ORM_FIELD_TYPE_DOUBLE    3
#define ORM_FIELD_TYPE_STRING    4
#define ORM_FIELD_TYPE_DATETIME  5
#define ORM_FIELD_TYPE_BOOL      6
#define ORM_FIELD_TYPE_BLOB      7

// ORM status codes
#define ORM_STATUS_NEW           0
#define ORM_STATUS_LOADED        1
#define ORM_STATUS_MODIFIED      2
#define ORM_STATUS_DELETED       3
#define ORM_STATUS_DETACHED      4

// Column name macros (legacy support)
#define ORM_COL_ID               "ID"
#define ORM_COL_CREATED_TS       "CRTD_TS"
#define ORM_COL_UPDATED_TS       "UPDTD_TS"
#define ORM_COL_CREATED_BY       "CRTD_BY"
#define ORM_COL_UPDATED_BY       "UPDTD_BY"

// Validation flags
#define ORM_VALIDATE_ON_SAVE     0x01
#define ORM_VALIDATE_ON_LOAD     0x02
#define ORM_VALIDATE_STRICT      0x04

// Forward declarations
class COrmField;
class COrmValidator;

/*******************************************************************************
 * COrmBase - Base class for all ORM entities
 * 
 * All entity classes should inherit from COrmBase and implement:
 *   - GetTableName()     : Returns the database table name
 *   - GetPrimaryKey()    : Returns the primary key column name
 *   - MapFields()        : Maps database columns to member variables
 *   - Validate()         : Validates entity before save (optional)
 * 
 * Usage:
 *   class CTradeEntity : public COrmBase {
 *       DECLARE_ORM_ENTITY(CTradeEntity, "TRD_BLOTTER", "TRD_ID")
 *       // ...
 *   };
 ******************************************************************************/
class COrmBase
{
public:
    COrmBase();
    virtual ~COrmBase();
    
    // Copy constructor and assignment
    COrmBase(const COrmBase& other);
    COrmBase& operator=(const COrmBase& other);
    
    // Primary key access
    long GetRecordId() const { return m_lRecordId; }
    void SetRecordId(long lId) { m_lRecordId = lId; m_bIsDirty = TRUE; }
    
    // Status management
    int GetStatus() const { return m_nStatus; }
    void SetStatus(int nStatus) { m_nStatus = nStatus; }
    
    BOOL IsNew() const { return m_nStatus == ORM_STATUS_NEW; }
    BOOL IsLoaded() const { return m_nStatus == ORM_STATUS_LOADED; }
    BOOL IsModified() const { return m_bIsDirty || m_nStatus == ORM_STATUS_MODIFIED; }
    BOOL IsDeleted() const { return m_nStatus == ORM_STATUS_DELETED; }
    BOOL IsDirty() const { return m_bIsDirty; }
    
    void MarkClean() { m_bIsDirty = FALSE; m_nStatus = ORM_STATUS_LOADED; }
    void MarkDirty() { m_bIsDirty = TRUE; }
    void MarkDeleted() { m_nStatus = ORM_STATUS_DELETED; m_bIsDirty = TRUE; }
    
    // Timestamp access
    const COleDateTime& GetCreatedTimestamp() const { return m_dtCreatedTs; }
    const COleDateTime& GetLastModified() const { return m_dtLastModified; }
    
    // User tracking
    LPCSTR GetCreatedBy() const { return m_szCreatedBy.c_str(); }
    LPCSTR GetUpdatedBy() const { return m_szUpdatedBy.c_str(); }
    void SetUpdatedBy(LPCSTR szUser) { m_szUpdatedBy = szUser ? szUser : ""; }
    
    // CRUD operations
    virtual BOOL Load(long lId);
    virtual BOOL Save();
    virtual BOOL Delete();
    
    // Query operations
    static CDbResultSet* FindAll(LPCSTR szTableName);
    static CDbResultSet* FindBy(LPCSTR szTableName, LPCSTR szWhereClause);
    static long Count(LPCSTR szTableName, LPCSTR szWhereClause = NULL);
    static BOOL Exists(long lId, LPCSTR szTableName, LPCSTR szPrimaryKey);
    
    // Abstract methods (must be implemented by derived classes)
    virtual LPCSTR GetTableName() const = 0;
    virtual LPCSTR GetPrimaryKey() const = 0;
    virtual void MapFields() = 0;
    
    // Optional: Custom validation
    virtual BOOL Validate(int nFlags = ORM_VALIDATE_ON_SAVE);
    virtual std::string GetValidationError() const { return m_szValidationError; }
    
    // Field access (for dynamic field mapping)
    void SetFieldValue(LPCSTR szFieldName, long lValue);
    void SetFieldValue(LPCSTR szFieldName, double dValue);
    void SetFieldValue(LPCSTR szFieldName, LPCSTR szValue);
    void SetFieldValue(LPCSTR szFieldName, const COleDateTime& dtValue);
    void SetFieldValue(LPCSTR szFieldName, BOOL bValue);
    
    BOOL GetFieldValue(LPCSTR szFieldName, long& lValue) const;
    BOOL GetFieldValue(LPCSTR szFieldName, double& dValue) const;
    BOOL GetFieldValue(LPCSTR szFieldName, std::string& strValue) const;
    BOOL GetFieldValue(LPCSTR szFieldName, COleDateTime& dtValue) const;
    BOOL GetFieldValue(LPCSTR szFieldName, BOOL& bValue) const;
    
    // SQL generation (for internal use)
    virtual std::string GenerateInsertSQL();
    virtual std::string GenerateUpdateSQL();
    virtual std::string GenerateDeleteSQL();
    virtual std::string GenerateSelectSQL(LPCSTR szWhereClause = NULL);
    
protected:
    // Member variables
    long m_lRecordId;
    BOOL m_bIsDirty;
    int m_nStatus;
    
    // Timestamps
    COleDateTime m_dtCreatedTs;
    COleDateTime m_dtLastModified;
    
    // User tracking
    std::string m_szCreatedBy;
    std::string m_szUpdatedBy;
    
    // Validation error message
    std::string m_szValidationError;
    
    // Field map for dynamic field access
    std::map<std::string, COrmField*> m_mapFields;
    
    // Internal helpers
    void AddField(LPCSTR szName, int nType, void* pData, int nSize);
    void ClearFields();
    BOOL LoadFromResultSet(CDbResultSet* pRS);
    void SetTimestampDefaults();
    
    // Legacy compatibility
    // #ifdef LEGACY_TIMESTAMP_FORMAT
    char m_szLegacyTimestamp[32];  // Old format: "YYYYMMDD HH:MM:SS"
    // #endif
};

/*******************************************************************************
 * COrmField - Represents a single field in an ORM entity
 ******************************************************************************/
class COrmField
{
public:
    COrmField();
    COrmField(LPCSTR szName, int nType, void* pData, int nSize);
    ~COrmField();
    
    LPCSTR GetName() const { return m_szName.c_str(); }
    int GetType() const { return m_nType; }
    int GetSize() const { return m_nSize; }
    
    void SetValue(long lValue);
    void SetValue(double dValue);
    void SetValue(LPCSTR szValue);
    void SetValue(const COleDateTime& dtValue);
    void SetValue(BOOL bValue);
    
    long GetAsLong() const;
    double GetAsDouble() const;
    std::string GetAsString() const;
    COleDateTime GetAsDateTime() const;
    BOOL GetAsBool() const;
    
private:
    std::string m_szName;
    int m_nType;
    void* m_pData;
    int m_nSize;
};

/*******************************************************************************
 * COrmValidator - Validation helper class (added 2016)
 ******************************************************************************/
class COrmValidator
{
public:
    COrmValidator();
    ~COrmValidator();
    
    void AddRequiredField(LPCSTR szFieldName);
    void AddNumericField(LPCSTR szFieldName, double dMin, double dMax);
    void AddStringField(LPCSTR szFieldName, int nMinLen, int nMaxLen);
    void AddDateField(LPCSTR szFieldName, const COleDateTime& dtMin, const COleDateTime& dtMax);
    
    BOOL Validate(COrmBase* pEntity, std::string& strError);
    void Clear();
    
private:
    struct ValidationRule {
        std::string szFieldName;
        int nRuleType;
        double dMinValue;
        double dMaxValue;
        int nMinLength;
        int nMaxLength;
        COleDateTime dtMinDate;
        COleDateTime dtMaxDate;
    };
    
    std::vector<ValidationRule> m_vecRules;
};

/*******************************************************************************
 * ORM Macros for simplified entity declaration
 ******************************************************************************/
#define DECLARE_ORM_ENTITY(className, tableName, primaryKey) \
public: \
    virtual LPCSTR GetTableName() const { return tableName; } \
    virtual LPCSTR GetPrimaryKey() const { return primaryKey; } \
    static className* LoadById(long lId) { \
        className* pObj = new className(); \
        if (pObj->Load(lId)) return pObj; \
        delete pObj; \
        return NULL; \
    } \
    static std::vector<className*> FindAll() { \
        std::vector<className*> vecResults; \
        CDbResultSet* pRS = COrmBase::FindAll(tableName); \
        if (pRS) { \
            while (pRS->Next()) { \
                className* pObj = new className(); \
                pObj->LoadFromResultSet(pRS); \
                vecResults.push_back(pObj); \
            } \
            delete pRS; \
        } \
        return vecResults; \
    }

// Legacy macros (deprecated)
#define ORM_FIELD(name, type, member) AddField(name, type, &member, sizeof(member))
#define ORM_FIELD_STRING(name, member, size) AddField(name, ORM_FIELD_TYPE_STRING, member, size)
#define ORM_FIELD_INT(name, member) AddField(name, ORM_FIELD_TYPE_INT, &member, sizeof(member))
#define ORM_FIELD_LONG(name, member) AddField(name, ORM_FIELD_TYPE_LONG, &member, sizeof(member))
#define ORM_FIELD_DOUBLE(name, member) AddField(name, ORM_FIELD_TYPE_DOUBLE, &member, sizeof(member))
#define ORM_FIELD_DATETIME(name, member) AddField(name, ORM_FIELD_TYPE_DATETIME, &member, sizeof(member))
#define ORM_FIELD_BOOL(name, member) AddField(name, ORM_FIELD_TYPE_BOOL, &member, sizeof(member))

#endif // CORMBASE_H

// $Log: COrmBase.h,v $
// Revision 1.45  2016/09/01 10:00:00  apatel
// Added validation framework
//
// Revision 1.44  2011/03/08 09:30:00  knakamura
// Added timestamp support
//
// Revision 1.43  2007/05/22 14:00:00  mrodriguez
// Added dirty tracking
//
// Revision 1.42  2004/03/15 10:00:00  jthompson
// Initial version
