/*******************************************************************************
 * CCounterpartyOrmEntity.cpp
 * 
 * Counterparty ORM Entity Implementation
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CCounterpartyOrmEntity.cpp,v 1.56 2019/04/10 11:45:00 apatel Exp $
 * $Revision: 1.56 $
 ******************************************************************************/

#include "db/CCounterpartyOrmEntity.h"
#include "db/CDBConnection.h"
#include "db/CDbException.h"
#include <sstream>
#include <cstring>

/*******************************************************************************
 * Constructor / Destructor
 ******************************************************************************/
CCounterpartyOrmEntity::CCounterpartyOrmEntity()
    : COrmBase()
    , m_nCounterpartyId(0)
    , m_cStatus(CPTY_STATUS_ACTIVE)
    , m_bIsInternal(FALSE)
    , m_dCachedExposure(0.0)
    , m_tExposureCacheTime(0)
{
    memset(m_crdsCounterpartyCode, 0, sizeof(m_crdsCounterpartyCode));
    memset(m_szCounterpartyName, 0, sizeof(m_szCounterpartyName));
    memset(m_szRatingCode, 0, sizeof(m_szRatingCode));
    memset(m_szRatingAgency, 0, sizeof(m_szRatingAgency));
    memset(m_szCountryCode, 0, sizeof(m_szCountryCode));
    memset(m_szLeiCode, 0, sizeof(m_szLeiCode));
    
    // Set defaults
    strcpy(m_szRatingCode, RATING_NR);
    strcpy(m_szCountryCode, "US");
    
    MapFields();
}

CCounterpartyOrmEntity::CCounterpartyOrmEntity(const CCounterpartyOrmEntity& other)
    : COrmBase(other)
    , m_nCounterpartyId(other.m_nCounterpartyId)
    , m_cStatus(other.m_cStatus)
    , m_bIsInternal(other.m_bIsInternal)
    , m_dCachedExposure(0.0)
    , m_tExposureCacheTime(0)
{
    memcpy(m_crdsCounterpartyCode, other.m_crdsCounterpartyCode, sizeof(m_crdsCounterpartyCode));
    memcpy(m_szCounterpartyName, other.m_szCounterpartyName, sizeof(m_szCounterpartyName));
    memcpy(m_szRatingCode, other.m_szRatingCode, sizeof(m_szRatingCode));
    memcpy(m_szRatingAgency, other.m_szRatingAgency, sizeof(m_szRatingAgency));
    memcpy(m_szCountryCode, other.m_szCountryCode, sizeof(m_szCountryCode));
    memcpy(m_szLeiCode, other.m_szLeiCode, sizeof(m_szLeiCode));
    
    MapFields();
}

CCounterpartyOrmEntity::~CCounterpartyOrmEntity()
{
}

CCounterpartyOrmEntity& CCounterpartyOrmEntity::operator=(const CCounterpartyOrmEntity& other)
{
    if (this != &other) {
        COrmBase::operator=(other);
        
        m_nCounterpartyId = other.m_nCounterpartyId;
        m_cStatus = other.m_cStatus;
        m_bIsInternal = other.m_bIsInternal;
        
        memcpy(m_crdsCounterpartyCode, other.m_crdsCounterpartyCode, sizeof(m_crdsCounterpartyCode));
        memcpy(m_szCounterpartyName, other.m_szCounterpartyName, sizeof(m_szCounterpartyName));
        memcpy(m_szRatingCode, other.m_szRatingCode, sizeof(m_szRatingCode));
        memcpy(m_szRatingAgency, other.m_szRatingAgency, sizeof(m_szRatingAgency));
        memcpy(m_szCountryCode, other.m_szCountryCode, sizeof(m_szCountryCode));
        memcpy(m_szLeiCode, other.m_szLeiCode, sizeof(m_szLeiCode));
        
        m_dCachedExposure = 0.0;
        m_tExposureCacheTime = 0;
    }
    return *this;
}

/*******************************************************************************
 * Field Setters
 ******************************************************************************/
void CCounterpartyOrmEntity::SetCrdssCounterpartyCode(LPCSTR szCode)
{
    if (szCode) {
        strncpy(m_crdsCounterpartyCode, szCode, sizeof(m_crdsCounterpartyCode) - 1);
        m_crdsCounterpartyCode[sizeof(m_crdsCounterpartyCode) - 1] = '\0';
        
        // Convert to uppercase (legacy CRDS format)
        for (int i = 0; m_crdsCounterpartyCode[i]; i++) {
            m_crdsCounterpartyCode[i] = toupper(m_crdsCounterpartyCode[i]);
        }
        
        m_bIsDirty = TRUE;
    }
}

void CCounterpartyOrmEntity::SetName(LPCSTR szName)
{
    if (szName) {
        strncpy(m_szCounterpartyName, szName, sizeof(m_szCounterpartyName) - 1);
        m_szCounterpartyName[sizeof(m_szCounterpartyName) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CCounterpartyOrmEntity::SetRatingCode(LPCSTR szCode)
{
    if (szCode) {
        strncpy(m_szRatingCode, szCode, sizeof(m_szRatingCode) - 1);
        m_szRatingCode[sizeof(m_szRatingCode) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CCounterpartyOrmEntity::SetRatingAgency(LPCSTR szAgency)
{
    if (szAgency) {
        strncpy(m_szRatingAgency, szAgency, sizeof(m_szRatingAgency) - 1);
        m_szRatingAgency[sizeof(m_szRatingAgency) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CCounterpartyOrmEntity::SetCountryCode(LPCSTR szCode)
{
    if (szCode) {
        strncpy(m_szCountryCode, szCode, sizeof(m_szCountryCode) - 1);
        m_szCountryCode[sizeof(m_szCountryCode) - 1] = '\0';
        
        // Convert to uppercase
        for (int i = 0; m_szCountryCode[i]; i++) {
            m_szCountryCode[i] = toupper(m_szCountryCode[i]);
        }
        
        m_bIsDirty = TRUE;
    }
}

void CCounterpartyOrmEntity::SetLeiCode(LPCSTR szCode)
{
    if (szCode) {
        strncpy(m_szLeiCode, szCode, sizeof(m_szLeiCode) - 1);
        m_szLeiCode[sizeof(m_szLeiCode) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

/*******************************************************************************
 * ORM Implementation
 ******************************************************************************/
void CCounterpartyOrmEntity::MapFields()
{
    ClearFields();
    
    // V1 fields
    AddField(SQLCOL_CPTY_ID, ORM_FIELD_TYPE_LONG, &m_nCounterpartyId, sizeof(m_nCounterpartyId));
    AddField(SQLCOL_CPTY_CD, ORM_FIELD_TYPE_STRING, m_crdsCounterpartyCode, sizeof(m_crdsCounterpartyCode));
    AddField(SQLCOL_CPTY_NM, ORM_FIELD_TYPE_STRING, m_szCounterpartyName, sizeof(m_szCounterpartyName));
    AddField(SQLCOL_CPTY_STTUS_CD, ORM_FIELD_TYPE_STRING, &m_cStatus, sizeof(m_cStatus));
    
    // V2 fields
    // #ifdef SCHEMA_V2_COUNTERPARTY
    AddField(SQLCOL_CRDRT_RATING_CD, ORM_FIELD_TYPE_STRING, m_szRatingCode, sizeof(m_szRatingCode));
    AddField(SQLCOL_CRDRT_AGENCY_NM, ORM_FIELD_TYPE_STRING, m_szRatingAgency, sizeof(m_szRatingAgency));
    AddField(SQLCOL_CPTY_CNTRY_CD, ORM_FIELD_TYPE_STRING, m_szCountryCode, sizeof(m_szCountryCode));
    AddField(SQLCOL_CPTY_LEI_CD, ORM_FIELD_TYPE_STRING, m_szLeiCode, sizeof(m_szLeiCode));
    AddField(SQLCOL_IS_INTRNL_FLG, ORM_FIELD_TYPE_STRING, &m_bIsInternal, sizeof(m_bIsInternal));
    // #endif
}

BOOL CCounterpartyOrmEntity::Validate(int nFlags)
{
    m_szValidationError = "";
    
    // Validate counterparty code
    if (strlen(m_crdsCounterpartyCode) == 0) {
        m_szValidationError = "Counterparty code is required";
        return FALSE;
    }
    
    // Validate counterparty name
    if (strlen(m_szCounterpartyName) == 0) {
        m_szValidationError = "Counterparty name is required";
        return FALSE;
    }
    
    // Validate status
    if (m_cStatus != CPTY_STATUS_ACTIVE &&
        m_cStatus != CPTY_STATUS_INACTIVE &&
        m_cStatus != CPTY_STATUS_SUSPENDED &&
        m_cStatus != CPTY_STATUS_TERMINATED) {
        m_szValidationError = "Invalid counterparty status";
        return FALSE;
    }
    
    // Validate rating code (if provided)
    if (strlen(m_szRatingCode) > 0) {
        // Check against valid rating codes
        // Simplified validation
        if (strcmp(m_szRatingCode, RATING_NR) != 0 &&
            strcmp(m_szRatingCode, RATING_AAA) != 0 &&
            strcmp(m_szRatingCode, RATING_AA) != 0 &&
            strcmp(m_szRatingCode, RATING_A) != 0 &&
            strcmp(m_szRatingCode, RATING_BBB) != 0 &&
            strcmp(m_szRatingCode, RATING_BB) != 0 &&
            strcmp(m_szRatingCode, RATING_B) != 0) {
            // Accept other ratings as valid
        }
    }
    
    // Validate country code (if provided)
    if (strlen(m_szCountryCode) > 0 && strlen(m_szCountryCode) != 2) {
        m_szValidationError = "Country code must be 2 characters";
        return FALSE;
    }
    
    return TRUE;
}

/*******************************************************************************
 * Rating Helpers
 ******************************************************************************/
BOOL CCounterpartyOrmEntity::IsInvestmentGrade() const
{
    // Investment grade: AAA, AA, A, BBB
    if (strcmp(m_szRatingCode, RATING_AAA) == 0 ||
        strcmp(m_szRatingCode, RATING_AA) == 0 ||
        strncmp(m_szRatingCode, "AA", 2) == 0 ||
        strcmp(m_szRatingCode, RATING_A) == 0 ||
        strncmp(m_szRatingCode, "A+", 2) == 0 ||
        strncmp(m_szRatingCode, "A-", 2) == 0 ||
        strcmp(m_szRatingCode, RATING_BBB) == 0 ||
        strncmp(m_szRatingCode, "BBB", 3) == 0) {
        return TRUE;
    }
    return FALSE;
}

/*******************************************************************************
 * Limit Checking (V2+)
 ******************************************************************************/
BOOL CCounterpartyOrmEntity::CheckCreditLimit(double dAmount, double& dCurrentExposure) const
{
    dCurrentExposure = GetCurrentExposure();
    double dLimit = GetCreditLimit();
    
    if (dLimit <= 0) {
        // No limit set - allow
        return TRUE;
    }
    
    return (dCurrentExposure + dAmount) <= dLimit;
}

double CCounterpartyOrmEntity::GetCurrentExposure() const
{
    // Check cache (5 minute TTL)
    time_t tNow = time(NULL);
    if (m_tExposureCacheTime > 0 && (tNow - m_tExposureCacheTime) < 300) {
        return m_dCachedExposure;
    }
    
    // Query database for current exposure
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return 0.0;
    }
    
    char szSQL[256];
    sprintf(szSQL, 
        "SELECT ISNULL(SUM(ABS(NTNL_AMT)), 0) "
        "FROM TRD_BLOTTER "
        "WHERE CPTY_ID = %ld AND TRD_STTUS_CD IN ('PE', 'VF', 'BK')",
        m_nCounterpartyId);
    
    CDbResultSet* pRS = pConn->ExecuteQuery(szSQL);
    if (pRS && pRS->Next()) {
        m_dCachedExposure = pRS->GetDouble(0);
        m_tExposureCacheTime = tNow;
        delete pRS;
    }
    
    return m_dCachedExposure;
}

double CCounterpartyOrmEntity::GetCreditLimit() const
{
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return 0.0;
    }
    
    char szSQL[256];
    sprintf(szSQL,
        "SELECT LMT_AMT FROM CPTY_LMT "
        "WHERE CPTY_ID = %ld AND LMT_TYP_CD = 'CRD_EXPO' "
        "AND (EXP_DT IS NULL OR EXP_DT > GETDATE())",
        m_nCounterpartyId);
    
    double dLimit = 0.0;
    CDbResultSet* pRS = pConn->ExecuteQuery(szSQL);
    if (pRS && pRS->Next()) {
        dLimit = pRS->GetDouble(0);
        delete pRS;
    }
    
    return dLimit;
}

/*******************************************************************************
 * Legacy Struct Conversion
 ******************************************************************************/
void CCounterpartyOrmEntity::CopyToStruct(LPCPTY_DATA_V1 pData) const
{
    if (pData == NULL) return;
    
    memset(pData, 0, sizeof(CPTY_DATA_V1));
    
    pData->m_lCounterpartyId = m_nCounterpartyId;
    strncpy(pData->m_szCounterpartyCode, m_crdsCounterpartyCode, sizeof(pData->m_szCounterpartyCode) - 1);
    strncpy(pData->m_szCounterpartyName, m_szCounterpartyName, sizeof(pData->m_szCounterpartyName) - 1);
    pData->m_cStatus = m_cStatus;
}

void CCounterpartyOrmEntity::CopyFromStruct(const CPTY_DATA_V1* pData)
{
    if (pData == NULL) return;
    
    m_nCounterpartyId = pData->m_lCounterpartyId;
    SetCrdssCounterpartyCode(pData->m_szCounterpartyCode);
    SetName(pData->m_szCounterpartyName);
    m_cStatus = pData->m_cStatus;
}

/*******************************************************************************
 * Query Helpers
 ******************************************************************************/
CCounterpartyOrmEntity* CCounterpartyOrmEntity::FindByCode(LPCSTR szCode)
{
    if (szCode == NULL || strlen(szCode) == 0) {
        return NULL;
    }
    
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) {
        return NULL;
    }
    
    std::string strEscapedCode = CDBConnection::EscapeString(szCode);
    
    char szSQL[256];
    sprintf(szSQL, "SELECT * FROM CPTY_MSTR WHERE CPTY_CD = '%s'", strEscapedCode.c_str());
    
    CDbResultSet* pRS = pConn->ExecuteQuery(szSQL);
    if (pRS && pRS->Next()) {
        CCounterpartyOrmEntity* pEntity = new CCounterpartyOrmEntity();
        pEntity->LoadFromResultSet(pRS);
        delete pRS;
        return pEntity;
    }
    
    if (pRS) delete pRS;
    return NULL;
}

std::vector<CCounterpartyOrmEntity*> CCounterpartyOrmEntity::FindByStatus(char cStatus)
{
    std::vector<CCounterpartyOrmEntity*> vecResults;
    
    char szWhere[64];
    sprintf(szWhere, "CPTY_STTUS_CD = '%c'", cStatus);
    
    CDbResultSet* pRS = COrmBase::FindBy("CPTY_MSTR", szWhere);
    if (pRS) {
        while (pRS->Next()) {
            CCounterpartyOrmEntity* pEntity = new CCounterpartyOrmEntity();
            pEntity->LoadFromResultSet(pRS);
            vecResults.push_back(pEntity);
        }
        delete pRS;
    }
    
    return vecResults;
}

std::vector<CCounterpartyOrmEntity*> CCounterpartyOrmEntity::FindActive()
{
    return FindByStatus(CPTY_STATUS_ACTIVE);
}

std::vector<CCounterpartyOrmEntity*> CCounterpartyOrmEntity::FindByRating(LPCSTR szMinRating)
{
    std::vector<CCounterpartyOrmEntity*> vecResults;
    
    // Simplified - in production would have proper rating comparison
    char szWhere[128];
    sprintf(szWhere, "CRDRT_RATING_CD >= '%s'", szMinRating);
    
    CDbResultSet* pRS = COrmBase::FindBy("CPTY_MSTR", szWhere);
    if (pRS) {
        while (pRS->Next()) {
            CCounterpartyOrmEntity* pEntity = new CCounterpartyOrmEntity();
            pEntity->LoadFromResultSet(pRS);
            vecResults.push_back(pEntity);
        }
        delete pRS;
    }
    
    return vecResults;
}

/*******************************************************************************
 * Global Helper Functions
 ******************************************************************************/
CCounterpartyOrmEntity* GetCounterpartyById(long lId)
{
    return CCounterpartyOrmEntity::LoadById(lId);
}

CCounterpartyOrmEntity* GetCounterpartyByCode(LPCSTR szCode)
{
    return CCounterpartyOrmEntity::FindByCode(szCode);
}

// $Log: CCounterpartyOrmEntity.cpp,v $
// Revision 1.56  2019/04/10 11:45:00  apatel
// Fixed SQL injection in FindByCode
//
// Revision 1.55  2016/09/01 10:00:00  apatel
// Added LEI code support
//
// Revision 1.54  2007/05/22 14:00:00  mrodriguez
// Added limit checking
//
// Revision 1.53  2004/03/15 10:00:00  jthompson
// Initial version
