/*******************************************************************************
 * CTradeOrmMapper.h
 * 
 * Trade ORM Mapper - Maps trade entities to database
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CTradeOrmMapper.h,v 1.78 2019/04/10 11:45:00 apatel Exp $
 * $Revision: 1.78 $
 ******************************************************************************/

#ifndef CTRADEORMMAPPER_H
#define CTRADEORMMAPPER_H

#include "db/COrmBase.h"
#include <vector>
#include <map>

// ============================================================================
// BLOTTER COLUMN CONSTANTS (Legacy - DO NOT MODIFY)
// These constants map to TRD_BLOTTER table columns
// ============================================================================
#define BLOTTERCOL_TRADE_ID          0
#define BLOTTERCOL_TRADE_REF         1
#define BLOTTERCOL_COUNTERPARTY      2
#define BLOTTERCOL_PRODUCT_TYPE      3
#define BLOTTERCOL_TRADE_STATUS      4
#define BLOTTERCOL_TRADE_DATE        5
#define BLOTTERCOL_EFFECTIVE_DATE    6
#define BLOTTERCOL_MATURITY_DATE     7
#define BLOTTERCOL_CURRENCY          8
#define BLOTTERCOL_BOOK              9
#define BLOTTERCOL_USER              10
#define BLOTTERCOL_CREATED_TS        11
#define BLOTTERCOL_UPDATED_TS        12

// V3+ columns (added 2011)
#define BLOTTERCOL_NOTIONAL          13
#define BLOTTERCOL_NOTIONAL_CCY      14
#define BLOTTERCOL_NOTIONAL_USD      15
#define BLOTTERCOL_MTM_VALUE         16
#define BLOTTERCOL_MTM_CCY           17
#define BLOTTERCOL_MTM_DATE          18
#define BLOTTERCOL_PAY_FIXED         19
#define BLOTTERCOL_FIXED_RATE        20
#define BLOTTERCOL_FLOAT_INDEX       21
#define BLOTTERCOL_FLOAT_SPREAD      22

// V4+ columns (added 2016)
#define BLOTTERCOL_VERSION_NUM       23
#define BLOTTERCOL_ORIG_TRADE_ID     24
#define BLOTTERCOL_AMEND_SEQ_NUM     25
#define BLOTTERCOL_USI_CODE          26
#define BLOTTERCOL_UTI_CODE          27
#define BLOTTERCOL_CLEARING_STATUS   28
#define BLOTTERCOL_CLEARING_HOUSE    29
#define BLOTTERCOL_PORTFOLIO_ID      30

// Maximum columns (for array sizing)
#define BLOTTERCOL_MAX               31

// ============================================================================
// SQL COLUMN NAME CONSTANTS
// ============================================================================
#define SQLCOL_TRD_ID           "TRD_ID"
#define SQLCOL_TRD_REF_NM       "TRD_REF_NM"
#define SQLCOL_CPTY_ID          "CPTY_ID"
#define SQLCOL_PRD_TYP_ID       "PRD_TYP_ID"
#define SQLCOL_TRD_STTUS_CD     "TRD_STTUS_CD"
#define SQLCOL_TRD_DT           "TRD_DT"
#define SQLCOL_EFF_DT           "EFF_DT"
#define SQLCOL_MAT_DT           "MAT_DT"
#define SQLCOL_CCY_CD           "CCY_CD"
#define SQLCOL_BOOK_NM          "BOOK_NM"
#define SQLCOL_TRD_USR_ID       "TRD_USR_ID"
#define SQLCOL_CRTD_TS          "CRTD_TS"
#define SQLCOL_UPDTD_TS         "UPDTD_TS"
#define SQLCOL_NTNL_AMT         "NTNL_AMT"
#define SQLCOL_NTNL_CCY_CD      "NTNL_CCY_CD"
#define SQLCOL_NTNL_AMT_USD     "NTNL_AMT_USD"
#define SQLCOL_MTM_VAL          "MTM_VAL"
#define SQLCOL_MTM_CCY_CD       "MTM_CCY_CD"
#define SQLCOL_MTM_VAL_DT       "MTM_VAL_DT"
#define SQLCOL_PAY_FIXED_FLG    "PAY_FIXED_FLG"
#define SQLCOL_FIXED_RATE       "FIXED_RATE"
#define SQLCOL_FLT_IDX_CD       "FLT_IDX_CD"
#define SQLCOL_FLT_SPREAD       "FLT_SPREAD"
#define SQLCOL_TRD_VERS_NUM     "TRD_VERS_NUM"
#define SQLCOL_ORIG_TRD_ID      "ORIG_TRD_ID"
#define SQLCOL_AMND_SEQ_NUM     "AMND_SEQ_NUM"
#define SQLCOL_USI_CD           "USI_CD"
#define SQLCOL_UTI_CD           "UTI_CD"
#define SQLCOL_CLR_STS_CD       "CLR_STS_CD"
#define SQLCOL_CLR_HOUSE_CD     "CLR_HOUSE_CD"
#define SQLCOL_PORTFOLIO_ID     "PORTFOLIO_ID"

// ============================================================================
// TRADE STATUS CODES
// ============================================================================
#define TRADE_STATUS_PENDING    "PE"
#define TRADE_STATUS_VERIFIED   "VF"
#define TRADE_STATUS_BOOKED     "BK"
#define TRADE_STATUS_CANCELLED  "CN"
#define TRADE_STATUS_ERROR      "ER"

// ============================================================================
// CLEARING STATUS CODES (V4+)
// ============================================================================
#define CLEARING_STATUS_UNCLEARED   "UNC"
#define CLEARING_STATUS_CLEARED     "CLR"
#define CLEARING_STATUS_REJECTED    "RJC"
#define CLEARING_STATUS_PENDING     "PND"

// ============================================================================
// Legacy struct for trade data (V1 compatibility)
// ============================================================================
#pragma pack(push, 1)
typedef struct tagTRADE_DATA_V1 {
    long    m_lTradeId;
    char    m_szTradeRef[25];
    long    m_lCounterpartyId;
    short   m_nProductTypeId;
    char    m_szTradeStatus[3];
    char    m_szTradeDate[12];
    char    m_szEffectiveDate[12];
    char    m_szMaturityDate[12];
    char    m_szCurrency[4];
    char    m_szBook[17];
    char    m_szUser[17];
    char    m_szCreatedTs[20];
    char    m_szUpdatedTs[20];
} TRADE_DATA_V1, *LPTRADE_DATA_V1;
#pragma pack(pop)

// ============================================================================
// Extended trade data struct (V3+)
// ============================================================================
#pragma pack(push, 1)
typedef struct tagTRADE_DATA_V3 {
    TRADE_DATA_V1 m_baseData;
    double  m_dNotionalAmount;
    char    m_szNotionalCcy[4];
    double  m_dNotionalUsd;
    double  m_dMtmValue;
    char    m_szMtmCcy[4];
    char    m_szMtmDate[12];
    char    m_cPayFixed;
    double  m_dFixedRate;
    char    m_szFloatIndex[9];
    double  m_dFloatSpread;
} TRADE_DATA_V3, *LPTRADE_DATA_V3;
#pragma pack(pop)

// ============================================================================
// CTradeOrmMapper - Maps trade entities to/from database
// ============================================================================
class CTradeOrmMapper : public COrmBase
{
    DECLARE_ORM_ENTITY(CTradeOrmMapper, "TRD_BLOTTER", "TRD_ID")
    
public:
    CTradeOrmMapper();
    CTradeOrmMapper(const CTradeOrmMapper& other);
    virtual ~CTradeOrmMapper();
    
    CTradeOrmMapper& operator=(const CTradeOrmMapper& other);
    
    // Field accessors - Trade ID
    long GetTradeId() const { return m_lTradeId; }
    void SetTradeId(long lId) { m_lTradeId = lId; m_bIsDirty = TRUE; }
    
    // Field accessors - Trade Reference
    LPCSTR GetTradeRef() const { return m_szTradeRef; }
    void SetTradeRef(LPCSTR szRef);
    
    // Field accessors - Counterparty
    long GetCounterpartyId() const { return m_lCounterpartyId; }
    void SetCounterpartyId(long lId) { m_lCounterpartyId = lId; m_bIsDirty = TRUE; }
    
    // Field accessors - Product Type
    short GetProductTypeId() const { return m_nProductTypeId; }
    void SetProductTypeId(short nId) { m_nProductTypeId = nId; m_bIsDirty = TRUE; }
    
    // Field accessors - Status
    LPCSTR GetTradeStatus() const { return m_szTradeStatus; }
    void SetTradeStatus(LPCSTR szStatus);
    BOOL IsPending() const { return strcmp(m_szTradeStatus, TRADE_STATUS_PENDING) == 0; }
    BOOL IsVerified() const { return strcmp(m_szTradeStatus, TRADE_STATUS_VERIFIED) == 0; }
    BOOL IsBooked() const { return strcmp(m_szTradeStatus, TRADE_STATUS_BOOKED) == 0; }
    BOOL IsCancelled() const { return strcmp(m_szTradeStatus, TRADE_STATUS_CANCELLED) == 0; }
    
    // Field accessors - Dates
    const COleDateTime& GetTradeDate() const { return m_dtTradeDate; }
    void SetTradeDate(const COleDateTime& dt) { m_dtTradeDate = dt; m_bIsDirty = TRUE; }
    const COleDateTime& GetEffectiveDate() const { return m_dtEffectiveDate; }
    void SetEffectiveDate(const COleDateTime& dt) { m_dtEffectiveDate = dt; m_bIsDirty = TRUE; }
    const COleDateTime& GetMaturityDate() const { return m_dtMaturityDate; }
    void SetMaturityDate(const COleDateTime& dt) { m_dtMaturityDate = dt; m_bIsDirty = TRUE; }
    
    // Field accessors - Currency
    LPCSTR GetCurrency() const { return m_szCurrency; }
    void SetCurrency(LPCSTR szCcy);
    
    // Field accessors - Book
    LPCSTR GetBook() const { return m_szBook; }
    void SetBook(LPCSTR szBook);
    
    // Field accessors - User
    LPCSTR GetUser() const { return m_szUser; }
    void SetUser(LPCSTR szUser);
    
    // V3+ Field accessors - Notional
    double GetNotionalAmount() const { return m_dNotionalAmount; }
    void SetNotionalAmount(double dAmount) { m_dNotionalAmount = dAmount; m_bIsDirty = TRUE; }
    LPCSTR GetNotionalCurrency() const { return m_szNotionalCcy; }
    void SetNotionalCurrency(LPCSTR szCcy);
    double GetNotionalUsd() const { return m_dNotionalUsd; }
    void SetNotionalUsd(double dAmount) { m_dNotionalUsd = dAmount; m_bIsDirty = TRUE; }
    
    // V3+ Field accessors - MTM
    double GetMtmValue() const { return m_dMtmValue; }
    void SetMtmValue(double dValue) { m_dMtmValue = dValue; m_bIsDirty = TRUE; }
    LPCSTR GetMtmCurrency() const { return m_szMtmCcy; }
    void SetMtmCurrency(LPCSTR szCcy);
    const COleDateTime& GetMtmDate() const { return m_dtMtmDate; }
    void SetMtmDate(const COleDateTime& dt) { m_dtMtmDate = dt; m_bIsDirty = TRUE; }
    
    // V3+ Field accessors - Swap legs
    BOOL IsPayFixed() const { return m_cPayFixed == 'Y'; }
    void SetPayFixed(BOOL bPayFixed) { m_cPayFixed = bPayFixed ? 'Y' : 'N'; m_bIsDirty = TRUE; }
    double GetFixedRate() const { return m_dFixedRate; }
    void SetFixedRate(double dRate) { m_dFixedRate = dRate; m_bIsDirty = TRUE; }
    LPCSTR GetFloatIndex() const { return m_szFloatIndex; }
    void SetFloatIndex(LPCSTR szIndex);
    double GetFloatSpread() const { return m_dFloatSpread; }
    void SetFloatSpread(double dSpread) { m_dFloatSpread = dSpread; m_bIsDirty = TRUE; }
    
    // V4+ Field accessors - Version control
    int GetVersionNumber() const { return m_nVersionNumber; }
    void SetVersionNumber(int nVer) { m_nVersionNumber = nVer; m_bIsDirty = TRUE; }
    long GetOriginalTradeId() const { return m_lOriginalTradeId; }
    void SetOriginalTradeId(long lId) { m_lOriginalTradeId = lId; m_bIsDirty = TRUE; }
    int GetAmendmentSeqNum() const { return m_nAmendmentSeqNum; }
    void SetAmendmentSeqNum(int nSeq) { m_nAmendmentSeqNum = nSeq; m_bIsDirty = TRUE; }
    
    // V4+ Field accessors - Regulatory IDs
    LPCSTR GetUsiCode() const { return m_szUsiCode; }
    void SetUsiCode(LPCSTR szCode);
    LPCSTR GetUtiCode() const { return m_szUtiCode; }
    void SetUtiCode(LPCSTR szCode);
    
    // V4+ Field accessors - Clearing
    LPCSTR GetClearingStatus() const { return m_szClearingStatus; }
    void SetClearingStatus(LPCSTR szStatus);
    BOOL IsCleared() const { return strcmp(m_szClearingStatus, CLEARING_STATUS_CLEARED) == 0; }
    LPCSTR GetClearingHouse() const { return m_szClearingHouse; }
    void SetClearingHouse(LPCSTR szHouse);
    long GetPortfolioId() const { return m_lPortfolioId; }
    void SetPortfolioId(long lId) { m_lPortfolioId = lId; m_bIsDirty = TRUE; }
    
    // ORM overrides
    virtual void MapFields();
    virtual BOOL Validate(int nFlags = ORM_VALIDATE_ON_SAVE);
    
    // Legacy struct conversion
    void CopyToStruct(LPTRADE_DATA_V1 pData) const;
    void CopyFromStruct(const TRADE_DATA_V1* pData);
    void CopyToStructV3(LPTRADE_DATA_V3 pData) const;
    void CopyFromStructV3(const TRADE_DATA_V3* pData);
    
    // Query helpers
    static std::vector<CTradeOrmMapper*> FindByCounterparty(long lCptyId);
    static std::vector<CTradeOrmMapper*> FindByDate(const COleDateTime& dtTrade);
    static std::vector<CTradeOrmMapper*> FindByStatus(LPCSTR szStatus);
    static std::vector<CTradeOrmMapper*> FindByBook(LPCSTR szBook);
    
    // Bulk operations
    static int UpdateStatusForIds(const std::vector<long>& vecIds, LPCSTR szNewStatus);
    static int DeleteByIds(const std::vector<long>& vecIds);
    
private:
    // V1 fields
    long        m_lTradeId;
    char        m_szTradeRef[25];
    long        m_lCounterpartyId;
    short       m_nProductTypeId;
    char        m_szTradeStatus[3];
    COleDateTime m_dtTradeDate;
    COleDateTime m_dtEffectiveDate;
    COleDateTime m_dtMaturityDate;
    char        m_szCurrency[4];
    char        m_szBook[17];
    char        m_szUser[17];
    
    // V3 fields
    double      m_dNotionalAmount;
    char        m_szNotionalCcy[4];
    double      m_dNotionalUsd;
    double      m_dMtmValue;
    char        m_szMtmCcy[4];
    COleDateTime m_dtMtmDate;
    char        m_cPayFixed;
    double      m_dFixedRate;
    char        m_szFloatIndex[9];
    double      m_dFloatSpread;
    
    // V4 fields
    int         m_nVersionNumber;
    long        m_lOriginalTradeId;
    int         m_nAmendmentSeqNum;
    char        m_szUsiCode[53];
    char        m_szUtiCode[53];
    char        m_szClearingStatus[4];
    char        m_szClearingHouse[17];
    long        m_lPortfolioId;
    
    // Schema version tracking
    int         m_nSchemaVersion;
};

// Legacy typedef for backwards compatibility
typedef CTradeOrmMapper CTradeEntity;
typedef CTradeOrmMapper CTradeRecord;

// Global helper functions (legacy)
CTradeOrmMapper* GetTradeById(long lTradeId);
std::vector<CTradeOrmMapper*> GetAllTradesForDate(const char* szDate);

#endif // CTRADEORMMAPPER_H

// $Log: CTradeOrmMapper.h,v $
// Revision 1.78  2019/04/10 11:45:00  apatel
// Added portfolio support
//
// Revision 1.77  2016/09/01 10:00:00  apatel
// Added V4 fields (regulatory IDs, clearing)
//
// Revision 1.76  2011/03/08 09:30:00  knakamura
// Added V3 fields (notional, MTM, swap legs)
//
// Revision 1.75  2004/03/15 10:00:00  jthompson
// Initial version
