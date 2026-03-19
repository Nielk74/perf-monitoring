#include "services/CCreditCheckService.h"
#include "services/CCounterpartyService.h"
#include "util/CLogger.h"
#include <ctime>
#include <sstream>

CCreditCheckService* CCreditCheckService::s_pInstance = NULL;
CCreditCheckService* g_pCreditCheckService = NULL;

CCreditCheckService::CCreditCheckService()
    : m_pCounterpartyService(NULL)
    , m_bInitialized(false)
    , m_nNextReservationId(1)
    , m_nTotalReservationsCreated(0)
    , m_nTotalReservationsReleased(0)
{
}

CCreditCheckService::CCreditCheckService(const CCreditCheckService& refOther)
{
}

CCreditCheckService& CCreditCheckService::operator=(const CCreditCheckService& refOther)
{
    return *this;
}

CCreditCheckService::~CCreditCheckService()
{
    Shutdown();
}

CCreditCheckService* CCreditCheckService::GetInstance()
{
    if (s_pInstance == NULL) {
        s_pInstance = new CCreditCheckService();
        g_pCreditCheckService = s_pInstance;
    }
    return s_pInstance;
}

void CCreditCheckService::DestroyInstance()
{
    if (s_pInstance != NULL) {
        delete s_pInstance;
        s_pInstance = NULL;
        g_pCreditCheckService = NULL;
    }
}

bool CCreditCheckService::Initialize(CCounterpartyService* pCounterpartyService)
{
    if (m_bInitialized) {
        LOG_WARNING("CCreditCheckService already initialized");
        return true;
    }
    
    if (pCounterpartyService == NULL) {
        LOG_ERROR("CCreditCheckService: NULL counterparty service");
        return false;
    }
    
    m_pCounterpartyService = pCounterpartyService;
    m_bInitialized = true;
    
    LOG_INFO("CCreditCheckService initialized successfully");
    return true;
}

bool CCreditCheckService::Shutdown()
{
    if (!m_bInitialized) {
        return true;
    }
    
    ClearAllReservations();
    m_pCounterpartyService = NULL;
    m_bInitialized = false;
    
    LOG_INFO("CCreditCheckService shutdown complete");
    return true;
}

ServiceResult<bool> CCreditCheckService::CheckCreditLimit(const std::string& strCptyCode, double dAmount)
{
    if (!m_bInitialized) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    if (strCptyCode.empty()) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND,
            "Empty counterparty code");
    }
    
    if (dAmount <= 0) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_CREDIT_CHECK_FAILED,
            "Invalid amount for credit check");
    }
    
    if (!ValidateCounterpartyForCredit(strCptyCode)) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_COUNTERPARTY_INACTIVE,
            "Counterparty not valid for credit");
    }
    
    ServiceResult<SCounterpartyLimits> limitsResult = 
        m_pCounterpartyService->GetCounterpartyLimits(strCptyCode);
    
    if (limitsResult.IsFailure()) {
        return ServiceResult<bool>::Failure(
            limitsResult.GetErrorCode(),
            limitsResult.GetErrorMessage());
    }
    
    const SCounterpartyLimits& limits = limitsResult.GetValue();
    
    double dTotalExposure = limits.m_dCurrentExposure;
    
    std::map<std::string, double>::iterator it = m_mapCounterpartyReservedTotals.find(strCptyCode);
    if (it != m_mapCounterpartyReservedTotals.end()) {
        dTotalExposure += it->second;
    }
    
    double dNewExposure = dTotalExposure + dAmount;
    
    if (dNewExposure > limits.m_dCreditLimit) {
        std::stringstream ss;
        ss << "Credit limit exceeded for " << strCptyCode 
           << ". Limit: " << limits.m_dCreditLimit
           << ", Current+Reserved: " << dTotalExposure
           << ", Requested: " << dAmount;
        LOG_WARNING(ss.str().c_str());
        
        // BUG: returns Success(false) instead of Failure — callers checking only IsFailure()
        // will not detect that the limit is exceeded
        return ServiceResult<bool>::Success(false);
    }
    
    std::stringstream ss;
    ss << "Credit check passed for " << strCptyCode << ", amount: " << dAmount;
    LOG_DEBUG(ss.str().c_str());
    
    return ServiceResult<bool>::Success(true);
}

ServiceResult<bool> CCreditCheckService::CheckCreditLimit(long nCptyId, double dAmount)
{
    if (!m_bInitialized || m_pCounterpartyService == NULL) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    ServiceResult<SCounterpartyData> cptyResult = 
        m_pCounterpartyService->LoadCounterpartyById(nCptyId);
    
    if (cptyResult.IsFailure()) {
        return ServiceResult<bool>::Failure(
            cptyResult.GetErrorCode(),
            cptyResult.GetErrorMessage());
    }
    
    return CheckCreditLimit(cptyResult.GetValue().m_strCode, dAmount);
}

ServiceResult<double> CCreditCheckService::GetAvailableCredit(const std::string& strCptyCode)
{
    if (!m_bInitialized || m_pCounterpartyService == NULL) {
        return ServiceResult<double>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    ServiceResult<SCounterpartyLimits> limitsResult = 
        m_pCounterpartyService->GetCounterpartyLimits(strCptyCode);
    
    if (limitsResult.IsFailure()) {
        return ServiceResult<double>::Failure(
            limitsResult.GetErrorCode(),
            limitsResult.GetErrorMessage());
    }
    
    const SCounterpartyLimits& limits = limitsResult.GetValue();
    
    double dReserved = 0.0;
    std::map<std::string, double>::iterator it = m_mapCounterpartyReservedTotals.find(strCptyCode);
    if (it != m_mapCounterpartyReservedTotals.end()) {
        dReserved = it->second;
    }
    
    double dAvailable = limits.m_dCreditLimit - limits.m_dCurrentExposure - dReserved;
    
    if (dAvailable < 0) {
        dAvailable = 0;
    }
    
    return ServiceResult<double>::Success(dAvailable);
}

ServiceResult<double> CCreditCheckService::GetAvailableCredit(long nCptyId)
{
    if (!m_bInitialized || m_pCounterpartyService == NULL) {
        return ServiceResult<double>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    ServiceResult<SCounterpartyData> cptyResult = 
        m_pCounterpartyService->LoadCounterpartyById(nCptyId);
    
    if (cptyResult.IsFailure()) {
        return ServiceResult<double>::Failure(
            cptyResult.GetErrorCode(),
            cptyResult.GetErrorMessage());
    }
    
    return GetAvailableCredit(cptyResult.GetValue().m_strCode);
}

ServiceResult<std::string> CCreditCheckService::ReserveCredit(
    const std::string& strCptyCode, 
    double dAmount, 
    const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<std::string>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    if (strCptyCode.empty() || strTradeId.empty()) {
        return ServiceResult<std::string>::Failure(
            SERVICES_ERRORCODE_CREDIT_RESERVATION_FAILED,
            "Invalid counterparty or trade ID");
    }
    
    if (dAmount <= 0) {
        return ServiceResult<std::string>::Failure(
            SERVICES_ERRORCODE_CREDIT_RESERVATION_FAILED,
            "Invalid amount for credit reservation");
    }
    
    ServiceResult<bool> checkResult = CheckCreditLimit(strCptyCode, dAmount);
    if (checkResult.IsFailure()) {
        return ServiceResult<std::string>::Failure(
            checkResult.GetErrorCode(),
            checkResult.GetErrorMessage());
    }
    
    if (HasReservation(strTradeId)) {
        std::stringstream ss;
        ss << "Credit already reserved for trade: " << strTradeId;
        LOG_WARNING(ss.str().c_str());
        return ServiceResult<std::string>::Failure(
            SERVICES_ERRORCODE_ALREADY_EXISTS,
            ss.str());
    }
    
    SCreditReservation reservation;
    reservation.m_strTradeId = strTradeId;
    reservation.m_strCounterpartyCode = strCptyCode;
    reservation.m_dReservedAmount = dAmount;
    reservation.m_nReservationTime = static_cast<long>(std::time(NULL));
    reservation.m_nReservationStatus = 1;
    
    std::string strReservationId = GenerateReservationId();
    
    m_mapCreditReservations[strTradeId] = reservation;
    m_mapReservationIdToTradeId[m_nNextReservationId] = strTradeId;
    
    double& dReservedTotal = m_mapCounterpartyReservedTotals[strCptyCode];
    dReservedTotal += dAmount;
    
    m_nTotalReservationsCreated++;
    
    std::stringstream ss;
    ss << "Credit reserved: " << dAmount << " for " << strCptyCode 
       << " trade " << strTradeId << " (reservation: " << strReservationId << ")";
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<std::string>::Success(strReservationId);
}

ServiceResult<bool> CCreditCheckService::ReleaseCreditReservation(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    std::map<std::string, SCreditReservation>::iterator it = 
        m_mapCreditReservations.find(strTradeId);
    
    if (it == m_mapCreditReservations.end()) {
        std::stringstream ss;
        ss << "No credit reservation found for trade: " << strTradeId;
        LOG_WARNING(ss.str().c_str());
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            ss.str());
    }
    
    const SCreditReservation& reservation = it->second;
    const std::string& strCptyCode = reservation.m_strCounterpartyCode;
    double dAmount = reservation.m_dReservedAmount;
    
    std::map<std::string, double>::iterator totalIt = 
        m_mapCounterpartyReservedTotals.find(strCptyCode);
    if (totalIt != m_mapCounterpartyReservedTotals.end()) {
        totalIt->second -= dAmount;
        if (totalIt->second <= 0) {
            m_mapCounterpartyReservedTotals.erase(totalIt);
        }
    }
    
    m_mapCreditReservations.erase(it);
    m_nTotalReservationsReleased++;
    
    std::stringstream ss;
    ss << "Credit reservation released for trade: " << strTradeId 
       << " (amount: " << dAmount << ")";
    LOG_INFO(ss.str().c_str());
    
    return ServiceResult<bool>::Success(true);
}

ServiceResult<bool> CCreditCheckService::ReleaseCreditReservation(long nReservationId)
{
    std::map<long, std::string>::iterator it = 
        m_mapReservationIdToTradeId.find(nReservationId);
    
    if (it == m_mapReservationIdToTradeId.end()) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            "Reservation ID not found");
    }
    
    return ReleaseCreditReservation(it->second);
}

ServiceResult<bool> CCreditCheckService::ConfirmCreditReservation(const std::string& strTradeId)
{
    if (!m_bInitialized) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    std::map<std::string, SCreditReservation>::iterator it = 
        m_mapCreditReservations.find(strTradeId);
    
    if (it == m_mapCreditReservations.end()) {
        return ServiceResult<bool>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            "No credit reservation found");
    }
    
    it->second.m_nReservationStatus = 2;
    
    std::stringstream ss;
    ss << "Credit reservation confirmed for trade: " << strTradeId;
    LOG_DEBUG(ss.str().c_str());
    
    return ServiceResult<bool>::Success(true);
}

ServiceResult<double> CCreditCheckService::GetTotalReservedCredit(const std::string& strCptyCode)
{
    std::map<std::string, double>::iterator it = 
        m_mapCounterpartyReservedTotals.find(strCptyCode);
    
    if (it != m_mapCounterpartyReservedTotals.end()) {
        return ServiceResult<double>::Success(it->second);
    }
    
    return ServiceResult<double>::Success(0.0);
}

ServiceResult<double> CCreditCheckService::GetTotalReservedCredit(long nCptyId)
{
    if (!m_bInitialized || m_pCounterpartyService == NULL) {
        return ServiceResult<double>::Failure(
            SERVICES_ERRORCODE_SERVICE_UNAVAILABLE,
            "CreditCheckService not initialized");
    }
    
    ServiceResult<SCounterpartyData> cptyResult = 
        m_pCounterpartyService->LoadCounterpartyById(nCptyId);
    
    if (cptyResult.IsFailure()) {
        return ServiceResult<double>::Failure(
            cptyResult.GetErrorCode(),
            cptyResult.GetErrorMessage());
    }
    
    return GetTotalReservedCredit(cptyResult.GetValue().m_strCode);
}

ServiceResult<SCreditReservation> CCreditCheckService::GetReservationDetails(const std::string& strTradeId)
{
    std::map<std::string, SCreditReservation>::iterator it = 
        m_mapCreditReservations.find(strTradeId);
    
    if (it == m_mapCreditReservations.end()) {
        return ServiceResult<SCreditReservation>::Failure(
            SERVICES_ERRORCODE_NOT_FOUND,
            "Reservation not found");
    }
    
    return ServiceResult<SCreditReservation>::Success(it->second);
}

void CCreditCheckService::ClearAllReservations()
{
    m_mapCreditReservations.clear();
    m_mapCounterpartyReservedTotals.clear();
    m_mapReservationIdToTradeId.clear();
    LOG_INFO("All credit reservations cleared");
}

void CCreditCheckService::ClearExpiredReservations(long nExpirationTime)
{
    long nCurrentTime = static_cast<long>(std::time(NULL));
    long nCutoffTime = nCurrentTime - nExpirationTime;
    
    std::vector<std::string> vecExpiredTrades;
    
    for (std::map<std::string, SCreditReservation>::iterator it = 
         m_mapCreditReservations.begin(); it != m_mapCreditReservations.end(); ++it) {
        if (it->second.m_nReservationTime < nCutoffTime) {
            vecExpiredTrades.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < vecExpiredTrades.size(); i++) {
        ReleaseCreditReservation(vecExpiredTrades[i]);
    }
    
    std::stringstream ss;
    ss << "Cleared " << vecExpiredTrades.size() << " expired credit reservations";
    LOG_INFO(ss.str().c_str());
}

size_t CCreditCheckService::GetReservationCount() const
{
    return m_mapCreditReservations.size();
}

bool CCreditCheckService::HasReservation(const std::string& strTradeId) const
{
    return m_mapCreditReservations.find(strTradeId) != m_mapCreditReservations.end();
}

bool CCreditCheckService::ValidateCounterpartyForCredit(const std::string& strCptyCode)
{
    if (m_pCounterpartyService == NULL) {
        return false;
    }
    
    ServiceResult<bool> statusResult = 
        m_pCounterpartyService->ValidateCounterpartyStatus(strCptyCode);
    
    return statusResult.IsSuccess();
}

bool CCreditCheckService::UpdateAvailableCredit(const std::string& strCptyCode, double dAmount)
{
    return true;
}

std::string CCreditCheckService::GenerateReservationId()
{
    std::stringstream ss;
    ss << "CR_" << m_nNextReservationId++;
    return ss.str();
}
