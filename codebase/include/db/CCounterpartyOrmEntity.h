/*******************************************************************************
 * CCounterpartyOrmEntity.h
 * 
 * Counterparty ORM Entity
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CCounterpartyOrmEntity.h,v 1.45 2016/09/01 10:00:00 apatel Exp $
 * $Revision: 1.45 $
 ******************************************************************************/

#ifndef CCOUNTERPARTYORMENTITY_H
#define CCOUNTERPARTYORMENTITY_H

#include "db/COrmBase.h"
#include <vector>
#include <map>

// ============================================================================
// COUNTERPARTY COLUMN CONSTANTS
// ============================================================================
#define CPTYCOL_ID              0
#define CPTYCOL_CODE            1
#define CPTYCOL_NAME            2
#define CPTYCOL_STATUS          3
#define CPTYCOL_CREATED_BY      4
#define CPTYCOL_CREATED_TS      5
#define CPTYCOL_UPDATED_BY      6
#define CPTYCOL_UPDATED_TS      7

// V2+ columns (added 2007)
#define CPTYCOL_RATING_CODE     8
#define CPTYCOL_RATING_AGENCY   9
#define CPTYCOL_COUNTRY_CODE    10
#define CPTYCOL_LEI_CODE        11
#define CPTYCOL_IS_INTERNAL     12

// ============================================================================
// COUNTERPARTY STATUS CODES
// ============================================================================
#define CPTY_STATUS_ACTIVE      'A'
#define CPTY_STATUS_INACTIVE    'I'
#define CPTY_STATUS_SUSPENDED   'S'
#define CPTY_STATUS_TERMINATED  'T'

// ============================================================================
// SQL COLUMN NAME CONSTANTS
// ============================================================================
#define SQLCOL_CPTY_ID          "CPTY_ID"
#define SQLCOL_CPTY_CD          "CPTY_CD"
#define SQLCOL_CPTY_NM          "CPTY_NM"
#define SQLCOL_CPTY_STTUS_CD    "CPTY_STTUS_CD"
#define SQLCOL_CRDRT_RATING_CD  "CRDRT_RATING_CD"
#define SQLCOL_CRDRT_AGENCY_NM  "CRDRT_AGENCY_NM"
#define SQLCOL_CPTY_CNTRY_CD    "CPTY_CNTRY_CD"
#define SQLCOL_CPTY_LEI_CD      "CPTY_LEI_CD"
#define SQLCOL_IS_INTRNL_FLG    "IS_INTRNL_FLG"

// ============================================================================
// CREDIT RATING CODES
// ============================================================================
#define RATING_AAA      "AAA"
#define RATING_AA       "AA"
#define RATING_A        "A"
#define RATING_BBB      "BBB"
#define RATING_BB       "BB"
#define RATING_B        "B"
#define RATING_NR       "NR"     // Not Rated

// ============================================================================
// Legacy struct for counterparty data
// ============================================================================
#pragma pack(push, 1)
typedef struct tagCPTY_DATA_V1 {
    long    m_lCounterpartyId;
    char    m_szCounterpartyCode[13];
    char    m_szCounterpartyName[65];
    char    m_cStatus;
    char    m_szCreatedBy[33];
    char    m_szCreatedTs[20];
    char    m_szUpdatedBy[33];
    char    m_szUpdatedTs[20];
} CPTY_DATA_V1, *LPCPTY_DATA_V1;
#pragma pack(pop)

// ============================================================================
// CCounterpartyOrmEntity - Counterparty ORM Entity Class
// ============================================================================
class CCounterpartyOrmEntity : public COrmBase
{
    DECLARE_ORM_ENTITY(CCounterpartyOrmEntity, "CPTY_MSTR", "CPTY_ID")
    
public:
    CCounterpartyOrmEntity();
    CCounterpartyOrmEntity(const CCounterpartyOrmEntity& other);
    virtual ~CCounterpartyOrmEntity();
    
    CCounterpartyOrmEntity& operator=(const CCounterpartyOrmEntity& other);
    
    // Field accessors - ID
    long GetCounterpartyId() const { return m_nCounterpartyId; }
    void SetCounterpartyId(long lId) { m_nCounterpartyId = lId; m_bIsDirty = TRUE; }
    
    // Override GetRecordId for ORM base compatibility
    long GetRecordId() const { return m_nCounterpartyId; }
    void SetRecordId(long lId) { SetCounterpartyId(lId); }
    
    // Field accessors - Code (CRDS format)
    LPCSTR GetCrdssCounterpartyCode() const { return m_crdsCounterpartyCode; }
    void SetCrdssCounterpartyCode(LPCSTR szCode);
    
    // Field accessors - Name
    LPCSTR GetName() const { return m_szCounterpartyName; }
    void SetName(LPCSTR szName);
    
    // Field accessors - Status
    char GetStatus() const { return m_cStatus; }
    void SetStatus(char cStatus) { m_cStatus = cStatus; m_bIsDirty = TRUE; }
    BOOL IsActive() const { return m_cStatus == CPTY_STATUS_ACTIVE; }
    BOOL IsSuspended() const { return m_cStatus == CPTY_STATUS_SUSPENDED; }
    
    // V2+ Field accessors - Rating
    LPCSTR GetRatingCode() const { return m_szRatingCode; }
    void SetRatingCode(LPCSTR szCode);
    LPCSTR GetRatingAgency() const { return m_szRatingAgency; }
    void SetRatingAgency(LPCSTR szAgency);
    BOOL IsInvestmentGrade() const;
    
    // V2+ Field accessors - Country
    LPCSTR GetCountryCode() const { return m_szCountryCode; }
    void SetCountryCode(LPCSTR szCode);
    
    // V2+ Field accessors - LEI
    LPCSTR GetLeiCode() const { return m_szLeiCode; }
    void SetLeiCode(LPCSTR szCode);
    
    // V2+ Field accessors - Internal flag
    BOOL IsInternal() const { return m_bIsInternal; }
    void SetInternal(BOOL bInternal) { m_bIsInternal = bInternal; m_bIsDirty = TRUE; }
    
    // ORM overrides
    virtual void MapFields();
    virtual BOOL Validate(int nFlags = ORM_VALIDATE_ON_SAVE);
    
    // Legacy struct conversion
    void CopyToStruct(LPCPTY_DATA_V1 pData) const;
    void CopyFromStruct(const CPTY_DATA_V1* pData);
    
    // Query helpers
    static CCounterpartyOrmEntity* FindByCode(LPCSTR szCode);
    static std::vector<CCounterpartyOrmEntity*> FindByStatus(char cStatus);
    static std::vector<CCounterpartyOrmEntity*> FindActive();
    static std::vector<CCounterpartyOrmEntity*> FindByRating(LPCSTR szMinRating);
    
    // Limit checking (V2+)
    BOOL CheckCreditLimit(double dAmount, double& dCurrentExposure) const;
    double GetCurrentExposure() const;
    double GetCreditLimit() const;
    
    // Legacy compatibility
    // #ifdef LEGACY_CRDS_SUPPORT
    LPCSTR GetCrdsCode() const { return m_crdsCounterpartyCode; }
    void SetCrdsCode(LPCSTR szCode) { SetCrdssCounterpartyCode(szCode); }
    // #endif
    
private:
    // V1 fields
    long        m_nCounterpartyId;
    char        m_crdsCounterpartyCode[13];  // CRDS format code
    char        m_szCounterpartyName[65];
    char        m_cStatus;
    
    // V2 fields
    char        m_szRatingCode[9];
    char        m_szRatingAgency[33];
    char        m_szCountryCode[3];
    char        m_szLeiCode[21];
    BOOL        m_bIsInternal;
    
    // Cached values (not persisted)
    mutable double m_dCachedExposure;
    mutable time_t m_tExposureCacheTime;
};

// Legacy typedefs
typedef CCounterpartyOrmEntity CCounterparty;
typedef CCounterpartyOrmEntity CCounterpartyRecord;

// Global helper functions
CCounterpartyOrmEntity* GetCounterpartyById(long lId);
CCounterpartyOrmEntity* GetCounterpartyByCode(LPCSTR szCode);

#endif // CCOUNTERPARTYORMENTITY_H

// $Log: CCounterpartyOrmEntity.h,v $
// Revision 1.45  2016/09/01 10:00:00  apatel
// Added LEI code support
//
// Revision 1.44  2007/05/22 14:00:00  mrodriguez
// Added rating and country fields
//
// Revision 1.43  2004/03/15 10:00:00  jthompson
// Initial version
