/*******************************************************************************
 * CTradeOrmMapper.cpp
 * 
 * Trade ORM Mapper Implementation
 * 
 * Copyright (c) 2004-2016 Legacy Trading Systems Inc.
 * All Rights Reserved.
 * 
 * $Header: /cvsroot/trading/db/CTradeOrmMapper.cpp,v 1.89 2019/06/15 14:20:00 apatel Exp $
 * $Revision: 1.89 $
 ******************************************************************************/

#include "db/CTradeOrmMapper.h"
#include "db/CDBConnection.h"
#include "db/CDbException.h"
#include <sstream>
#include <cstring>

/*******************************************************************************
 * Constructor / Destructor
 ******************************************************************************/
CTradeOrmMapper::CTradeOrmMapper()
    : COrmBase()
    , m_lTradeId(0)
    , m_lCounterpartyId(0)
    , m_nProductTypeId(0)
    , m_dNotionalAmount(0.0)
    , m_dNotionalUsd(0.0)
    , m_dMtmValue(0.0)
    , m_cPayFixed('N')
    , m_dFixedRate(0.0)
    , m_dFloatSpread(0.0)
    , m_nVersionNumber(1)
    , m_lOriginalTradeId(0)
    , m_nAmendmentSeqNum(0)
    , m_lPortfolioId(0)
    , m_nSchemaVersion(4)  // Current schema version
{
    memset(m_szTradeRef, 0, sizeof(m_szTradeRef));
    memset(m_szTradeStatus, 0, sizeof(m_szTradeStatus));
    memset(m_szCurrency, 0, sizeof(m_szCurrency));
    memset(m_szBook, 0, sizeof(m_szBook));
    memset(m_szUser, 0, sizeof(m_szUser));
    memset(m_szNotionalCcy, 0, sizeof(m_szNotionalCcy));
    memset(m_szMtmCcy, 0, sizeof(m_szMtmCcy));
    memset(m_szFloatIndex, 0, sizeof(m_szFloatIndex));
    memset(m_szUsiCode, 0, sizeof(m_szUsiCode));
    memset(m_szUtiCode, 0, sizeof(m_szUtiCode));
    memset(m_szClearingStatus, 0, sizeof(m_szClearingStatus));
    memset(m_szClearingHouse, 0, sizeof(m_szClearingHouse));
    
    // Set defaults
    strcpy(m_szTradeStatus, TRADE_STATUS_PENDING);
    strcpy(m_szCurrency, "USD");
    strcpy(m_szNotionalCcy, "USD");
    strcpy(m_szClearingStatus, CLEARING_STATUS_UNCLEARED);
    
    MapFields();
}

CTradeOrmMapper::CTradeOrmMapper(const CTradeOrmMapper& other)
    : COrmBase(other)
    , m_lTradeId(other.m_lTradeId)
    , m_lCounterpartyId(other.m_lCounterpartyId)
    , m_nProductTypeId(other.m_nProductTypeId)
    , m_dtTradeDate(other.m_dtTradeDate)
    , m_dtEffectiveDate(other.m_dtEffectiveDate)
    , m_dtMaturityDate(other.m_dtMaturityDate)
    , m_dNotionalAmount(other.m_dNotionalAmount)
    , m_dNotionalUsd(other.m_dNotionalUsd)
    , m_dMtmValue(other.m_dMtmValue)
    , m_dtMtmDate(other.m_dtMtmDate)
    , m_cPayFixed(other.m_cPayFixed)
    , m_dFixedRate(other.m_dFixedRate)
    , m_dFloatSpread(other.m_dFloatSpread)
    , m_nVersionNumber(other.m_nVersionNumber)
    , m_lOriginalTradeId(other.m_lOriginalTradeId)
    , m_nAmendmentSeqNum(other.m_nAmendmentSeqNum)
    , m_lPortfolioId(other.m_lPortfolioId)
    , m_nSchemaVersion(other.m_nSchemaVersion)
{
    memcpy(m_szTradeRef, other.m_szTradeRef, sizeof(m_szTradeRef));
    memcpy(m_szTradeStatus, other.m_szTradeStatus, sizeof(m_szTradeStatus));
    memcpy(m_szCurrency, other.m_szCurrency, sizeof(m_szCurrency));
    memcpy(m_szBook, other.m_szBook, sizeof(m_szBook));
    memcpy(m_szUser, other.m_szUser, sizeof(m_szUser));
    memcpy(m_szNotionalCcy, other.m_szNotionalCcy, sizeof(m_szNotionalCcy));
    memcpy(m_szMtmCcy, other.m_szMtmCcy, sizeof(m_szMtmCcy));
    memcpy(m_szFloatIndex, other.m_szFloatIndex, sizeof(m_szFloatIndex));
    memcpy(m_szUsiCode, other.m_szUsiCode, sizeof(m_szUsiCode));
    memcpy(m_szUtiCode, other.m_szUtiCode, sizeof(m_szUtiCode));
    memcpy(m_szClearingStatus, other.m_szClearingStatus, sizeof(m_szClearingStatus));
    memcpy(m_szClearingHouse, other.m_szClearingHouse, sizeof(m_szClearingHouse));
    
    MapFields();
}

CTradeOrmMapper::~CTradeOrmMapper()
{
}

CTradeOrmMapper& CTradeOrmMapper::operator=(const CTradeOrmMapper& other)
{
    if (this != &other) {
        COrmBase::operator=(other);
        
        m_lTradeId = other.m_lTradeId;
        m_lCounterpartyId = other.m_lCounterpartyId;
        m_nProductTypeId = other.m_nProductTypeId;
        m_dtTradeDate = other.m_dtTradeDate;
        m_dtEffectiveDate = other.m_dtEffectiveDate;
        m_dtMaturityDate = other.m_dtMaturityDate;
        m_dNotionalAmount = other.m_dNotionalAmount;
        m_dNotionalUsd = other.m_dNotionalUsd;
        m_dMtmValue = other.m_dMtmValue;
        m_dtMtmDate = other.m_dtMtmDate;
        m_cPayFixed = other.m_cPayFixed;
        m_dFixedRate = other.m_dFixedRate;
        m_dFloatSpread = other.m_dFloatSpread;
        m_nVersionNumber = other.m_nVersionNumber;
        m_lOriginalTradeId = other.m_lOriginalTradeId;
        m_nAmendmentSeqNum = other.m_nAmendmentSeqNum;
        m_lPortfolioId = other.m_lPortfolioId;
        m_nSchemaVersion = other.m_nSchemaVersion;
        
        memcpy(m_szTradeRef, other.m_szTradeRef, sizeof(m_szTradeRef));
        memcpy(m_szTradeStatus, other.m_szTradeStatus, sizeof(m_szTradeStatus));
        memcpy(m_szCurrency, other.m_szCurrency, sizeof(m_szCurrency));
        memcpy(m_szBook, other.m_szBook, sizeof(m_szBook));
        memcpy(m_szUser, other.m_szUser, sizeof(m_szUser));
        memcpy(m_szNotionalCcy, other.m_szNotionalCcy, sizeof(m_szNotionalCcy));
        memcpy(m_szMtmCcy, other.m_szMtmCcy, sizeof(m_szMtmCcy));
        memcpy(m_szFloatIndex, other.m_szFloatIndex, sizeof(m_szFloatIndex));
        memcpy(m_szUsiCode, other.m_szUsiCode, sizeof(m_szUsiCode));
        memcpy(m_szUtiCode, other.m_szUtiCode, sizeof(m_szUtiCode));
        memcpy(m_szClearingStatus, other.m_szClearingStatus, sizeof(m_szClearingStatus));
        memcpy(m_szClearingHouse, other.m_szClearingHouse, sizeof(m_szClearingHouse));
    }
    return *this;
}

/*******************************************************************************
 * Field Setters
 ******************************************************************************/
void CTradeOrmMapper::SetTradeRef(LPCSTR szRef)
{
    if (szRef) {
        strncpy(m_szTradeRef, szRef, sizeof(m_szTradeRef) - 1);
        m_szTradeRef[sizeof(m_szTradeRef) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetTradeStatus(LPCSTR szStatus)
{
    if (szStatus) {
        strncpy(m_szTradeStatus, szStatus, sizeof(m_szTradeStatus) - 1);
        m_szTradeStatus[sizeof(m_szTradeStatus) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetCurrency(LPCSTR szCcy)
{
    if (szCcy) {
        strncpy(m_szCurrency, szCcy, sizeof(m_szCurrency) - 1);
        m_szCurrency[sizeof(m_szCurrency) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetBook(LPCSTR szBook)
{
    if (szBook) {
        strncpy(m_szBook, szBook, sizeof(m_szBook) - 1);
        m_szBook[sizeof(m_szBook) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetUser(LPCSTR szUser)
{
    if (szUser) {
        strncpy(m_szUser, szUser, sizeof(m_szUser) - 1);
        m_szUser[sizeof(m_szUser) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetNotionalCurrency(LPCSTR szCcy)
{
    if (szCcy) {
        strncpy(m_szNotionalCcy, szCcy, sizeof(m_szNotionalCcy) - 1);
        m_szNotionalCcy[sizeof(m_szNotionalCcy) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetMtmCurrency(LPCSTR szCcy)
{
    if (szCcy) {
        strncpy(m_szMtmCcy, szCcy, sizeof(m_szMtmCcy) - 1);
        m_szMtmCcy[sizeof(m_szMtmCcy) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetFloatIndex(LPCSTR szIndex)
{
    if (szIndex) {
        strncpy(m_szFloatIndex, szIndex, sizeof(m_szFloatIndex) - 1);
        m_szFloatIndex[sizeof(m_szFloatIndex) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetUsiCode(LPCSTR szCode)
{
    if (szCode) {
        strncpy(m_szUsiCode, szCode, sizeof(m_szUsiCode) - 1);
        m_szUsiCode[sizeof(m_szUsiCode) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetUtiCode(LPCSTR szCode)
{
    if (szCode) {
        strncpy(m_szUtiCode, szCode, sizeof(m_szUtiCode) - 1);
        m_szUtiCode[sizeof(m_szUtiCode) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetClearingStatus(LPCSTR szStatus)
{
    if (szStatus) {
        strncpy(m_szClearingStatus, szStatus, sizeof(m_szClearingStatus) - 1);
        m_szClearingStatus[sizeof(m_szClearingStatus) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

void CTradeOrmMapper::SetClearingHouse(LPCSTR szHouse)
{
    if (szHouse) {
        strncpy(m_szClearingHouse, szHouse, sizeof(m_szClearingHouse) - 1);
        m_szClearingHouse[sizeof(m_szClearingHouse) - 1] = '\0';
        m_bIsDirty = TRUE;
    }
}

/*******************************************************************************
 * ORM Implementation
 ******************************************************************************/
void CTradeOrmMapper::MapFields()
{
    ClearFields();
    
    // V1 fields
    AddField(SQLCOL_TRD_ID, ORM_FIELD_TYPE_LONG, &m_lTradeId, sizeof(m_lTradeId));
    AddField(SQLCOL_TRD_REF_NM, ORM_FIELD_TYPE_STRING, m_szTradeRef, sizeof(m_szTradeRef));
    AddField(SQLCOL_CPTY_ID, ORM_FIELD_TYPE_LONG, &m_lCounterpartyId, sizeof(m_lCounterpartyId));
    AddField(SQLCOL_PRD_TYP_ID, ORM_FIELD_TYPE_INT, &m_nProductTypeId, sizeof(m_nProductTypeId));
    AddField(SQLCOL_TRD_STTUS_CD, ORM_FIELD_TYPE_STRING, m_szTradeStatus, sizeof(m_szTradeStatus));
    AddField(SQLCOL_CCY_CD, ORM_FIELD_TYPE_STRING, m_szCurrency, sizeof(m_szCurrency));
    AddField(SQLCOL_BOOK_NM, ORM_FIELD_TYPE_STRING, m_szBook, sizeof(m_szBook));
    AddField(SQLCOL_TRD_USR_ID, ORM_FIELD_TYPE_STRING, m_szUser, sizeof(m_szUser));
    
    // V3 fields
    // #ifdef SCHEMA_V3_NOTIONAL
    AddField(SQLCOL_NTNL_AMT, ORM_FIELD_TYPE_DOUBLE, &m_dNotionalAmount, sizeof(m_dNotionalAmount));
    AddField(SQLCOL_NTNL_CCY_CD, ORM_FIELD_TYPE_STRING, m_szNotionalCcy, sizeof(m_szNotionalCcy));
    AddField(SQLCOL_NTNL_AMT_USD, ORM_FIELD_TYPE_DOUBLE, &m_dNotionalUsd, sizeof(m_dNotionalUsd));
    AddField(SQLCOL_MTM_VAL, ORM_FIELD_TYPE_DOUBLE, &m_dMtmValue, sizeof(m_dMtmValue));
    AddField(SQLCOL_MTM_CCY_CD, ORM_FIELD_TYPE_STRING, m_szMtmCcy, sizeof(m_szMtmCcy));
    AddField(SQLCOL_PAY_FIXED_FLG, ORM_FIELD_TYPE_STRING, &m_cPayFixed, sizeof(m_cPayFixed));
    AddField(SQLCOL_FIXED_RATE, ORM_FIELD_TYPE_DOUBLE, &m_dFixedRate, sizeof(m_dFixedRate));
    AddField(SQLCOL_FLT_IDX_CD, ORM_FIELD_TYPE_STRING, m_szFloatIndex, sizeof(m_szFloatIndex));
    AddField(SQLCOL_FLT_SPREAD, ORM_FIELD_TYPE_DOUBLE, &m_dFloatSpread, sizeof(m_dFloatSpread));
    // #endif
    
    // V4 fields
    // #ifdef SCHEMA_V4_CURRENT
    AddField(SQLCOL_TRD_VERS_NUM, ORM_FIELD_TYPE_INT, &m_nVersionNumber, sizeof(m_nVersionNumber));
    AddField(SQLCOL_ORIG_TRD_ID, ORM_FIELD_TYPE_LONG, &m_lOriginalTradeId, sizeof(m_lOriginalTradeId));
    AddField(SQLCOL_AMND_SEQ_NUM, ORM_FIELD_TYPE_INT, &m_nAmendmentSeqNum, sizeof(m_nAmendmentSeqNum));
    AddField(SQLCOL_USI_CD, ORM_FIELD_TYPE_STRING, m_szUsiCode, sizeof(m_szUsiCode));
    AddField(SQLCOL_UTI_CD, ORM_FIELD_TYPE_STRING, m_szUtiCode, sizeof(m_szUtiCode));
    AddField(SQLCOL_CLR_STS_CD, ORM_FIELD_TYPE_STRING, m_szClearingStatus, sizeof(m_szClearingStatus));
    AddField(SQLCOL_CLR_HOUSE_CD, ORM_FIELD_TYPE_STRING, m_szClearingHouse, sizeof(m_szClearingHouse));
    AddField(SQLCOL_PORTFOLIO_ID, ORM_FIELD_TYPE_LONG, &m_lPortfolioId, sizeof(m_lPortfolioId));
    // #endif
}

BOOL CTradeOrmMapper::Validate(int nFlags)
{
    m_szValidationError = "";
    
    // Validate trade reference
    if (strlen(m_szTradeRef) == 0) {
        m_szValidationError = "Trade reference is required";
        return FALSE;
    }
    
    // Validate counterparty
    if (m_lCounterpartyId <= 0) {
        m_szValidationError = "Invalid counterparty ID";
        return FALSE;
    }
    
    // Validate product type
    if (m_nProductTypeId <= 0) {
        m_szValidationError = "Invalid product type ID";
        return FALSE;
    }
    
    // Validate status
    if (strcmp(m_szTradeStatus, TRADE_STATUS_PENDING) != 0 &&
        strcmp(m_szTradeStatus, TRADE_STATUS_VERIFIED) != 0 &&
        strcmp(m_szTradeStatus, TRADE_STATUS_BOOKED) != 0 &&
        strcmp(m_szTradeStatus, TRADE_STATUS_CANCELLED) != 0 &&
        strcmp(m_szTradeStatus, TRADE_STATUS_ERROR) != 0) {
        m_szValidationError = "Invalid trade status";
        return FALSE;
    }
    
    // Validate dates
    if (m_dtEffectiveDate.GetStatus() == COleDateTime::valid &&
        m_dtMaturityDate.GetStatus() == COleDateTime::valid) {
        if (m_dtMaturityDate <= m_dtEffectiveDate) {
            m_szValidationError = "Maturity date must be after effective date";
            return FALSE;
        }
    }
    
    // V3+ validation
    // #ifdef SCHEMA_V3_NOTIONAL
    if (m_dNotionalAmount < 0) {
        m_szValidationError = "Notional amount cannot be negative";
        return FALSE;
    }
    // #endif
    
    return TRUE;
}

/*******************************************************************************
 * Legacy Struct Conversion
 ******************************************************************************/
void CTradeOrmMapper::CopyToStruct(LPTRADE_DATA_V1 pData) const
{
    if (pData == NULL) return;
    
    memset(pData, 0, sizeof(TRADE_DATA_V1));
    
    pData->m_lTradeId = m_lTradeId;
    strncpy(pData->m_szTradeRef, m_szTradeRef, sizeof(pData->m_szTradeRef) - 1);
    pData->m_lCounterpartyId = m_lCounterpartyId;
    pData->m_nProductTypeId = m_nProductTypeId;
    strncpy(pData->m_szTradeStatus, m_szTradeStatus, sizeof(pData->m_szTradeStatus) - 1);
    
    // Format dates as strings (legacy format)
    if (m_dtTradeDate.GetStatus() == COleDateTime::valid) {
        sprintf(pData->m_szTradeDate, "%04d-%02d-%02d",
                m_dtTradeDate.GetYear(), m_dtTradeDate.GetMonth(), m_dtTradeDate.GetDay());
    }
    if (m_dtEffectiveDate.GetStatus() == COleDateTime::valid) {
        sprintf(pData->m_szEffectiveDate, "%04d-%02d-%02d",
                m_dtEffectiveDate.GetYear(), m_dtEffectiveDate.GetMonth(), m_dtEffectiveDate.GetDay());
    }
    if (m_dtMaturityDate.GetStatus() == COleDateTime::valid) {
        sprintf(pData->m_szMaturityDate, "%04d-%02d-%02d",
                m_dtMaturityDate.GetYear(), m_dtMaturityDate.GetMonth(), m_dtMaturityDate.GetDay());
    }
    
    strncpy(pData->m_szCurrency, m_szCurrency, sizeof(pData->m_szCurrency) - 1);
    strncpy(pData->m_szBook, m_szBook, sizeof(pData->m_szBook) - 1);
    strncpy(pData->m_szUser, m_szUser, sizeof(pData->m_szUser) - 1);
}

void CTradeOrmMapper::CopyFromStruct(const TRADE_DATA_V1* pData)
{
    if (pData == NULL) return;
    
    m_lTradeId = pData->m_lTradeId;
    SetTradeRef(pData->m_szTradeRef);
    m_lCounterpartyId = pData->m_lCounterpartyId;
    m_nProductTypeId = pData->m_nProductTypeId;
    SetTradeStatus(pData->m_szTradeStatus);
    SetCurrency(pData->m_szCurrency);
    SetBook(pData->m_szBook);
    SetUser(pData->m_szUser);
    
    // Parse dates from strings (legacy format)
    // Note: Simplified parsing - real implementation would be more robust
}

void CTradeOrmMapper::CopyToStructV3(LPTRADE_DATA_V3 pData) const
{
    if (pData == NULL) return;
    
    CopyToStruct(&pData->m_baseData);
    
    pData->m_dNotionalAmount = m_dNotionalAmount;
    strncpy(pData->m_szNotionalCcy, m_szNotionalCcy, sizeof(pData->m_szNotionalCcy) - 1);
    pData->m_dNotionalUsd = m_dNotionalUsd;
    pData->m_dMtmValue = m_dMtmValue;
    strncpy(pData->m_szMtmCcy, m_szMtmCcy, sizeof(pData->m_szMtmCcy) - 1);
    pData->m_cPayFixed = m_cPayFixed;
    pData->m_dFixedRate = m_dFixedRate;
    strncpy(pData->m_szFloatIndex, m_szFloatIndex, sizeof(pData->m_szFloatIndex) - 1);
    pData->m_dFloatSpread = m_dFloatSpread;
}

void CTradeOrmMapper::CopyFromStructV3(const TRADE_DATA_V3* pData)
{
    if (pData == NULL) return;
    
    CopyFromStruct(&pData->m_baseData);
    
    m_dNotionalAmount = pData->m_dNotionalAmount;
    SetNotionalCurrency(pData->m_szNotionalCcy);
    m_dNotionalUsd = pData->m_dNotionalUsd;
    m_dMtmValue = pData->m_dMtmValue;
    SetMtmCurrency(pData->m_szMtmCcy);
    m_cPayFixed = pData->m_cPayFixed;
    m_dFixedRate = pData->m_dFixedRate;
    SetFloatIndex(pData->m_szFloatIndex);
    m_dFloatSpread = pData->m_dFloatSpread;
}

/*******************************************************************************
 * Query Helpers
 ******************************************************************************/
std::vector<CTradeOrmMapper*> CTradeOrmMapper::FindByCounterparty(long lCptyId)
{
    std::vector<CTradeOrmMapper*> vecResults;
    
    char szWhere[128];
    sprintf(szWhere, "CPTY_ID = %ld", lCptyId);
    
    CDbResultSet* pRS = COrmBase::FindBy("TRD_BLOTTER", szWhere);
    if (pRS) {
        while (pRS->Next()) {
            CTradeOrmMapper* pTrade = new CTradeOrmMapper();
            pTrade->LoadFromResultSet(pRS);
            vecResults.push_back(pTrade);
        }
        delete pRS;
    }
    
    return vecResults;
}

std::vector<CTradeOrmMapper*> CTradeOrmMapper::FindByDate(const COleDateTime& dtTrade)
{
    std::vector<CTradeOrmMapper*> vecResults;
    
    char szWhere[128];
    sprintf(szWhere, "TRD_DT = '%04d-%02d-%02d'",
            dtTrade.GetYear(), dtTrade.GetMonth(), dtTrade.GetDay());
    
    CDbResultSet* pRS = COrmBase::FindBy("TRD_BLOTTER", szWhere);
    if (pRS) {
        while (pRS->Next()) {
            CTradeOrmMapper* pTrade = new CTradeOrmMapper();
            pTrade->LoadFromResultSet(pRS);
            vecResults.push_back(pTrade);
        }
        delete pRS;
    }
    
    return vecResults;
}

std::vector<CTradeOrmMapper*> CTradeOrmMapper::FindByStatus(LPCSTR szStatus)
{
    std::vector<CTradeOrmMapper*> vecResults;
    
    char szWhere[128];
    sprintf(szWhere, "TRD_STTUS_CD = '%s'", szStatus);
    
    CDbResultSet* pRS = COrmBase::FindBy("TRD_BLOTTER", szWhere);
    if (pRS) {
        while (pRS->Next()) {
            CTradeOrmMapper* pTrade = new CTradeOrmMapper();
            pTrade->LoadFromResultSet(pRS);
            vecResults.push_back(pTrade);
        }
        delete pRS;
    }
    
    return vecResults;
}

std::vector<CTradeOrmMapper*> CTradeOrmMapper::FindByBook(LPCSTR szBook)
{
    std::vector<CTradeOrmMapper*> vecResults;
    
    char szWhere[128];
    sprintf(szWhere, "BOOK_NM = '%s'", szBook);
    
    CDbResultSet* pRS = COrmBase::FindBy("TRD_BLOTTER", szWhere);
    if (pRS) {
        while (pRS->Next()) {
            CTradeOrmMapper* pTrade = new CTradeOrmMapper();
            pTrade->LoadFromResultSet(pRS);
            vecResults.push_back(pTrade);
        }
        delete pRS;
    }
    
    return vecResults;
}

/*******************************************************************************
 * Bulk Operations
 ******************************************************************************/
int CTradeOrmMapper::UpdateStatusForIds(const std::vector<long>& vecIds, LPCSTR szNewStatus)
{
    if (vecIds.empty()) return 0;
    
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) return -1;
    
    std::stringstream ss;
    ss << "UPDATE TRD_BLOTTER SET TRD_STTUS_CD = '" << szNewStatus 
       << "', UPDTD_TS = GETDATE() WHERE TRD_ID IN (";
    
    for (size_t i = 0; i < vecIds.size(); i++) {
        if (i > 0) ss << ",";
        ss << vecIds[i];
    }
    ss << ")";
    
    return pConn->ExecuteUpdate(ss.str().c_str());
}

int CTradeOrmMapper::DeleteByIds(const std::vector<long>& vecIds)
{
    if (vecIds.empty()) return 0;
    
    CDBConnection* pConn = CDBConnection::GetInstance();
    if (!pConn->IsConnected()) return -1;
    
    std::stringstream ss;
    ss << "DELETE FROM TRD_BLOTTER WHERE TRD_ID IN (";
    
    for (size_t i = 0; i < vecIds.size(); i++) {
        if (i > 0) ss << ",";
        ss << vecIds[i];
    }
    ss << ")";
    
    return pConn->ExecuteUpdate(ss.str().c_str());
}

/*******************************************************************************
 * Global Helper Functions (Legacy)
 ******************************************************************************/
CTradeOrmMapper* GetTradeById(long lTradeId)
{
    return CTradeOrmMapper::LoadById(lTradeId);
}

std::vector<CTradeOrmMapper*> GetAllTradesForDate(const char* szDate)
{
    // Parse date and call FindByDate
    COleDateTime dt;
    dt.ParseDateTime(szDate);
    return CTradeOrmMapper::FindByDate(dt);
}

// $Log: CTradeOrmMapper.cpp,v $
// Revision 1.89  2019/06/15 14:20:00  apatel
// Fixed SQL injection in FindByBook
//
// Revision 1.88  2016/09/01 10:00:00  apatel
// Added V4 fields
//
// Revision 1.87  2011/03/08 09:30:00  knakamura
// Added V3 fields and struct conversion
//
// Revision 1.86  2004/03/15 10:00:00  jthompson
// Initial version
