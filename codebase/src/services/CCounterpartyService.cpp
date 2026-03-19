#include "services/CCounterpartyService.h"
#include "util/CLogger.h"
#include "db/CDBConnection.h"
#include "db/CCounterpartyOrmEntity.h"
#include "legacy/CCounterpartyAdapter.h"
#include <ctime>
#include <sstream>

CCounterpartyService* CCounterpartyService::s_pInstance = NULL;
CCounterpartyService* g_pCounterpartyService = NULL;

CCounterpartyService::CCounterpartyService()
    : m_pDbConnection(NULL)
    , m_pCounterpartyAdapter(NULL)
    , m_bInitialized(false)
    , m_nCacheHitCount(0)
    , m_nCacheMissCount(0)
{
}

CCounterpartyService::CCounterpartyService(const CCounterpartyService& refOther)
{
}

CCounterpartyService& CCounterpartyService::operator=(const CCounterpartyService& refOther)
{
    return *this;
}

CCounterpartyService::~CCounterpartyService()
{
    Shutdown();
}

CCounterpartyService* CCounterpartyService::GetInstance()
{
    if (s_pInstance == NULL) {
        s_pInstance = new CCounterpartyService();
        g_pCounterpartyService = s_pInstance;
    }
    return s_pInstance;
}

void CCounterpartyService::DestroyInstance()
{
    if (s_pInstance != NULL) {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pCounterpartyService = NULL;
    }
}

bool CCounterpartyService::Initialize(CDBConnection* pDbConnection)
{
    if (m_bInitialized) {
        LOG_WARNING("CCounterpartyService already initialized");
        return true;
    }
    
    if (pDbConnection == NULL) {
        LOG_ERROR("CCounterpartyService: NULL database connection");
        return false;
    }
    
    m_pDbConnection = pDbConnection;
    
    m_pCounterpartyAdapter = new CCounterpartyAdapter();
    if (!m_pCounterpartyAdapter->Initialize()) {
        LOG_WARNING("CCounterpartyService: Failed to initialize adapter, using DB only");
    }
    
    m_bInitialized = true;
    LOG_INFO("CCounterpartyService initialized successfully");
    return true;
}

bool CCounterpartyService::Shutdown()
{
    if (!m_bInitialized) {
        return true;
    }
    
    ClearCache();
    
    if (m_pCounterpartyAdapter != NULL) {
        m_pCounterpartyAdapter->Shutdown();
        delete m_pCounterpartyAdapter;
        m_pCounterpartyAdapter = NULL;
    }
    
    m_pDbConnection = NULL;
    m_bInitialized = false;
    
    LOG_INFO("CCounterpartyService shutdown complete");
    return true;
}

ServiceResult<SCounterpartyData> CCounterpartyService::LoadCounterpartyByCode(const std::string& strCode)
{
    if (!m_bInitialized) {
        return ServiceResult<SCounterpartyData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CounterpartyService not initialized");
    }
    
    if (strCode.empty()) {
        return ServiceResult<SCounterpartyData>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND,
            "Empty counterparty code");
    }
    
    SCounterpartyData cachedData;
    if (GetFromCache(strCode, cachedData)) {
        m_nCacheHitCount++;
        return ServiceResult<SCounterpartyData>::Success(cachedData);
    }
    
    m_nCacheMissCount++;
    
    SCounterpartyData data;
    bool bLoaded = false;
    
    if (m_pDbConnection != NULL && m_pDbConnection->IsConnected()) {
        bLoaded = LoadFromDatabase(strCode, data);
    }
    
    if (!bLoaded && m_pCounterpartyAdapter != NULL) {
        bLoaded = LoadFromAdapter(strCode, data);
    }
    
    if (!bLoaded) {
        std::stringstream ss;
        ss << "Counterparty not found: " << strCode;
        LOG_ERROR(ss.str().c_str());
        return ServiceResult<SCounterpartyData>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND,
            ss.str());
    }
    
    CacheCounterparty(strCode, data);
    
    std::stringstream ss;
    ss << "Loaded counterparty: " << strCode << " (ID: " << data.m_nCounterpartyId << ")";
    LOG_DEBUG(ss.str().c_str());
    
    return ServiceResult<SCounterpartyData>::Success(data);
}

ServiceResult<SCounterpartyData> CCounterpartyService::LoadCounterpartyById(long nId)
{
    if (!m_bInitialized) {
        return ServiceResult<SCounterpartyData>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CounterpartyService not initialized");
    }
    
    if (nId <= 0) {
        return ServiceResult<SCounterpartyData>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND,
            "Invalid counterparty ID");
    }
    
    std::map<long, std::string>::iterator it = m_mapIdToCode.find(nId);
    if (it != m_mapIdToCode.end()) {
        return LoadCounterpartyByCode(it->second);
    }
    
    SCounterpartyData data;
    if (!LoadFromDatabase(nId, data)) {
        std::stringstream ss;
        ss << "Counterparty not found by ID: " << nId;
        LOG_ERROR(ss.str().c_str());
        return ServiceResult<SCounterpartyData>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND,
            ss.str());
    }
    
    CacheCounterparty(data.m_strCode, data);
    m_mapIdToCode[nId] = data.m_strCode;
    
    return ServiceResult<SCounterpartyData>::Success(data);
}

ServiceResult<bool> CCounterpartyService::ValidateCounterpartyStatus(const std::string& strCode)
{
    ServiceResult<SCounterpartyData> result = LoadCounterpartyByCode(strCode);
    if (result.IsFailure()) {
        return ServiceResult<bool>::Failure(result.GetErrorCode(), result.GetErrorMessage());
    }
    
    const SCounterpartyData& data = result.GetValue();
    
    if (data.m_cStatus == CPTY_STATUS_ACTIVE) {
        return ServiceResult<bool>::Success(true);
    }
    
    if (data.m_cStatus == CPTY_STATUS_SUSPENDED) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_BLOCKED,
            "Counterparty is suspended");
    }
    
    if (data.m_cStatus == CPTY_STATUS_TERMINATED) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_BLOCKED,
            "Counterparty is terminated");
    }
    
    return ServiceResult<bool>::Failure(
        SERVICES_ERRORCODE_COUNTERPARTY_INACTIVE,
        "Counterparty is not active");
}

ServiceResult<bool> CCounterpartyService::ValidateCounterpartyStatus(long nId)
{
    ServiceResult<SCounterpartyData> result = LoadCounterpartyById(nId);
    if (result.IsFailure()) {
        return ServiceResult<bool>::Failure(result.GetErrorCode(), result.GetErrorMessage());
    }
    
    const SCounterpartyData& data = result.GetValue();
    
    if (data.m_cStatus == CPTY_STATUS_ACTIVE) {
        return ServiceResult<bool>::Success(true);
    }
    
    return ServiceResult<bool>::Failure(
        SERVICES_ERRORCODE_COUNTERPARTY_INACTIVE,
        "Counterparty is not active");
}

ServiceResult<SCounterpartyLimits> CCounterpartyService::GetCounterpartyLimits(const std::string& strCode)
{
    ServiceResult<SCounterpartyData> result = LoadCounterpartyByCode(strCode);
    if (result.IsFailure()) {
        return ServiceResult<SCounterpartyLimits>::Failure(result.GetErrorCode(), result.GetErrorMessage());
    }
    
    const SCounterpartyData& data = result.GetValue();
    
    SCounterpartyLimits limits;
    limits.m_dCreditLimit = data.m_dCreditLimit;
    limits.m_dCurrentExposure = data.m_dCurrentExposure;
    limits.m_dAvailableCredit = data.m_dCreditLimit - data.m_dCurrentExposure;
    limits.m_dDailyLimit = data.m_dCreditLimit * 0.5;
    limits.m_dProductLimit = data.m_dCreditLimit * 0.3;
    
    return ServiceResult<SCounterpartyLimits>::Success(limits);
}

ServiceResult<SCounterpartyLimits> CCounterpartyService::GetCounterpartyLimits(long nId)
{
    ServiceResult<SCounterpartyData> result = LoadCounterpartyById(nId);
    if (result.IsFailure()) {
        return ServiceResult<SCounterpartyLimits>::Failure(result.GetErrorCode(), result.GetErrorMessage());
    }
    return GetCounterpartyLimits(result.GetValue().m_strCode);
}

ServiceResult<bool> CCounterpartyService::IsCounterpartyEligibleForProduct(const std::string& strCode, ProductType productType)
{
    ServiceResult<SCounterpartyData> result = LoadCounterpartyByCode(strCode);
    if (result.IsFailure()) {
        return ServiceResult<bool>::Failure(result.GetErrorCode(), result.GetErrorMessage());
    }
    
    const SCounterpartyData& data = result.GetValue();
    
    if (!CheckProductEligibility(data, productType)) {
        std::stringstream ss;
        ss << "Counterparty " << strCode << " not eligible for product type " << static_cast<int>(productType);
        LOG_WARNING(ss.str().c_str());
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_BLOCKED,
            "Counterparty not eligible for this product type");
    }
    
    return ServiceResult<bool>::Success(true);
}

ServiceResult<bool> CCounterpartyService::IsCounterpartyEligibleForProduct(long nId, ProductType productType)
{
    ServiceResult<SCounterpartyData> result = LoadCounterpartyById(nId);
    if (result.IsFailure()) {
        return ServiceResult<bool>::Failure(result.GetErrorCode(), result.GetErrorMessage());
    }
    return IsCounterpartyEligibleForProduct(result.GetValue().m_strCode, productType);
}

void CCounterpartyService::ClearCache()
{
    m_mapCounterpartyCache.clear();
    m_mapIdToCode.clear();
    m_nCacheHitCount = 0;
    m_nCacheMissCount = 0;
    LOG_DEBUG("Counterparty cache cleared");
}

void CCounterpartyService::RefreshCache()
{
    ClearCache();
    LOG_INFO("Counterparty cache refreshed");
}

size_t CCounterpartyService::GetCacheSize() const
{
    return m_mapCounterpartyCache.size();
}

bool CCounterpartyService::LoadFromDatabase(const std::string& strCode, SCounterpartyData& refData)
{
    if (m_pDbConnection == NULL || !m_pDbConnection->IsConnected()) {
        return false;
    }
    
    CCounterpartyOrmEntity* pEntity = CCounterpartyOrmEntity::FindByCode(strCode.c_str());
    if (pEntity == NULL) {
        return false;
    }
    
    refData.m_nCounterpartyId = pEntity->GetCounterpartyId();
    refData.m_strCode = pEntity->GetCrdssCounterpartyCode();
    refData.m_strName = pEntity->GetName();
    refData.m_cStatus = pEntity->GetStatus();
    refData.m_strRatingCode = pEntity->GetRatingCode();
    refData.m_strCountryCode = pEntity->GetCountryCode();
    refData.m_dCreditLimit = pEntity->GetCreditLimit();
    refData.m_dCurrentExposure = pEntity->GetCurrentExposure();
    
    refData.m_bIsEligibleForDerivatives = pEntity->IsInvestmentGrade();
    refData.m_bIsEligibleForFX = true;
    
    refData.m_nRiskCategory = 1;
    if (refData.m_strRatingCode == RATING_AAA || refData.m_strRatingCode == RATING_AA) {
        refData.m_nRiskCategory = 1;
    } else if (refData.m_strRatingCode == RATING_A || refData.m_strRatingCode == RATING_BBB) {
        refData.m_nRiskCategory = 2;
    } else {
        refData.m_nRiskCategory = 3;
    }
    
    delete pEntity;
    return true;
}

bool CCounterpartyService::LoadFromDatabase(long nId, SCounterpartyData& refData)
{
    if (m_pDbConnection == NULL || !m_pDbConnection->IsConnected()) {
        return false;
    }
    
    CCounterpartyOrmEntity* pEntity = GetCounterpartyById(nId);
    if (pEntity == NULL) {
        return false;
    }
    
    refData.m_nCounterpartyId = pEntity->GetCounterpartyId();
    refData.m_strCode = pEntity->GetCrdssCounterpartyCode();
    refData.m_strName = pEntity->GetName();
    refData.m_cStatus = pEntity->GetStatus();
    refData.m_strRatingCode = pEntity->GetRatingCode();
    refData.m_strCountryCode = pEntity->GetCountryCode();
    refData.m_dCreditLimit = pEntity->GetCreditLimit();
    refData.m_dCurrentExposure = pEntity->GetCurrentExposure();
    
    delete pEntity;
    return true;
}

bool CCounterpartyService::LoadFromAdapter(const std::string& strCode, SCounterpartyData& refData)
{
    if (m_pCounterpartyAdapter == NULL) {
        return false;
    }
    
    SCounterpartyInfo* pInfo = m_pCounterpartyAdapter->GetCounterpartyById(0);
    if (pInfo == NULL) {
        return false;
    }
    
    refData.m_nCounterpartyId = pInfo->m_nCounterpartyId;
    refData.m_strCode = pInfo->m_szCode;
    refData.m_strName = pInfo->m_szName;
    refData.m_cStatus = pInfo->m_bIsActive ? CPTY_STATUS_ACTIVE : CPTY_STATUS_INACTIVE;
    refData.m_dCreditLimit = pInfo->m_dCreditLimit;
    refData.m_nRiskCategory = pInfo->m_nRiskCategory;
    
    return true;
}

void CCounterpartyService::CacheCounterparty(const std::string& strCode, const SCounterpartyData& refData)
{
    m_mapCounterpartyCache[strCode] = refData;
    m_mapIdToCode[refData.m_nCounterpartyId] = strCode;
}

bool CCounterpartyService::GetFromCache(const std::string& strCode, SCounterpartyData& refData)
{
    std::map<std::string, SCounterpartyData>::iterator it = m_mapCounterpartyCache.find(strCode);
    if (it != m_mapCounterpartyCache.end()) {
        refData = it->second;
        return true;
    }
    return false;
}

bool CCounterpartyService::CheckProductEligibility(const SCounterpartyData& refData, ProductType productType)
{
    switch (productType) {
        case ProductType::Derivative:
        case ProductType::CDS:
        case ProductType::TRS:
        case ProductType::Swap:
            return refData.m_bIsEligibleForDerivatives;
        
        case ProductType::FX:
        case ProductType::Forward:
            return refData.m_bIsEligibleForFX;
        
        case ProductType::Equity:
        case ProductType::FixedIncome:
        case ProductType::CashFlow:
        case ProductType::Repo:
            return true;
        
        default:
            return false;
    }
}
