#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <memory>
#include <map>

#define SERVICES_ERRORCODE_SUCCESS                0
#define SERVICES_ERRORCODE_UNKNOWN_ERROR          1001
#define SERVICES_ERRORCODE_COUNTERPARTY_NOT_FOUND 1002
#define SERVICES_ERRORCODE_COUNTERPARTY_INACTIVE  1003
#define SERVICES_ERRORCODE_COUNTERPARTY_BLOCKED   1004
#define SERVICES_ERRORCODE_CREDIT_LIMIT_EXCEEDED  1005
#define SERVICES_ERRORCODE_CREDIT_CHECK_FAILED    1006
#define SERVICES_ERRORCODE_CREDIT_RESERVATION_FAILED 1007
#define SERVICES_ERRORCODE_TRADE_NOT_FOUND        1008
#define SERVICES_ERRORCODE_INVALID_TRADE_STATE    1009
#define SERVICES_ERRORCODE_VALIDATION_FAILED      1010
#define SERVICES_ERRORCODE_APPROVAL_REQUIRED      1011
#define SERVICES_ERRORCODE_EXECUTION_FAILED       1012
#define SERVICES_ERRORCODE_SETTLEMENT_FAILED      1013
#define SERVICES_ERRORCODE_NOTIFICATION_FAILED    1014
#define SERVICES_ERRORCODE_DB_ERROR               1015
#define SERVICES_ERRORCODE_NULL_POINTER           1016
#define SERVICES_ERRORCODE_ALREADY_EXISTS         1017
#define SERVICES_ERRORCODE_SERVICE_UNAVAILABLE    1018
#define SERVICES_ERRORCODE_NOT_FOUND              1019

#define TRADELIFECYCLE_STATE_DRAFT               0
#define TRADELIFECYCLE_STATE_PENDING_VALIDATION  1
#define TRADELIFECYCLE_STATE_VALIDATED           2
#define TRADELIFECYCLE_STATE_INVALID             3
#define TRADELIFECYCLE_STATE_PENDING_APPROVAL    4
#define TRADELIFECYCLE_STATE_APPROVED            5
#define TRADELIFECYCLE_STATE_REJECTED            6
#define TRADELIFECYCLE_STATE_EXECUTED            7
#define TRADELIFECYCLE_STATE_SETTLED             8
#define TRADELIFECYCLE_STATE_CANCELLED           9
#define TRADELIFECYCLE_STATE_FAILED              10

enum class TradeLifecycleState {
    Draft = TRADELIFECYCLE_STATE_DRAFT,
    PendingValidation = TRADELIFECYCLE_STATE_PENDING_VALIDATION,
    Validated = TRADELIFECYCLE_STATE_VALIDATED,
    Invalid = TRADELIFECYCLE_STATE_INVALID,
    PendingApproval = TRADELIFECYCLE_STATE_PENDING_APPROVAL,
    Approved = TRADELIFECYCLE_STATE_APPROVED,
    Rejected = TRADELIFECYCLE_STATE_REJECTED,
    Executed = TRADELIFECYCLE_STATE_EXECUTED,
    Settled = TRADELIFECYCLE_STATE_SETTLED,
    Cancelled = TRADELIFECYCLE_STATE_CANCELLED,
    Failed = TRADELIFECYCLE_STATE_FAILED
};

inline const char* GetLifecycleStateName(TradeLifecycleState state) {
    switch (state) {
        case TradeLifecycleState::Draft: return "DRAFT";
        case TradeLifecycleState::PendingValidation: return "PENDING_VALIDATION";
        case TradeLifecycleState::Validated: return "VALIDATED";
        case TradeLifecycleState::Invalid: return "INVALID";
        case TradeLifecycleState::PendingApproval: return "PENDING_APPROVAL";
        case TradeLifecycleState::Approved: return "APPROVED";
        case TradeLifecycleState::Rejected: return "REJECTED";
        case TradeLifecycleState::Executed: return "EXECUTED";
        case TradeLifecycleState::Settled: return "SETTLED";
        case TradeLifecycleState::Cancelled: return "CANCELLED";
        case TradeLifecycleState::Failed: return "FAILED";
        default: return "UNKNOWN";
    }
}

inline TradeLifecycleState IntToLifecycleState(int nState) {
    if (nState < 0 || nState > TRADELIFECYCLE_STATE_FAILED) {
        return TradeLifecycleState::Draft;
    }
    return static_cast<TradeLifecycleState>(nState);
}

template<typename T>
struct ServiceResult {
    bool m_bSuccess;
    int m_nErrorCode;
    std::string m_strErrorMessage;
    T m_value;
    
    ServiceResult() : m_bSuccess(false), m_nErrorCode(SERVICES_ERRORCODE_UNKNOWN_ERROR), m_value(T()) {}
    
    static ServiceResult<T> Success(const T& value) {
        ServiceResult<T> result;
        result.m_bSuccess = true;
        result.m_nErrorCode = SERVICES_ERRORCODE_SUCCESS;
        result.m_value = value;
        return result;
    }
    
    static ServiceResult<T> Failure(int errorCode, const std::string& errorMsg) {
        ServiceResult<T> result;
        result.m_bSuccess = false;
        result.m_nErrorCode = errorCode;
        result.m_strErrorMessage = errorMsg;
        return result;
    }
    
    bool IsSuccess() const { return m_bSuccess; }
    bool IsFailure() const { return !m_bSuccess; }
    const T& GetValue() const { return m_value; }
    int GetErrorCode() const { return m_nErrorCode; }
    const std::string& GetErrorMessage() const { return m_strErrorMessage; }
};

template<>
struct ServiceResult<void> {
    bool m_bSuccess;
    int m_nErrorCode;
    std::string m_strErrorMessage;
    
    ServiceResult() : m_bSuccess(false), m_nErrorCode(SERVICES_ERRORCODE_UNKNOWN_ERROR) {}
    
    static ServiceResult<void> Success() {
        ServiceResult<void> result;
        result.m_bSuccess = true;
        result.m_nErrorCode = SERVICES_ERRORCODE_SUCCESS;
        return result;
    }
    
    static ServiceResult<void> Failure(int errorCode, const std::string& errorMsg) {
        ServiceResult<void> result;
        result.m_bSuccess = false;
        result.m_nErrorCode = errorCode;
        result.m_strErrorMessage = errorMsg;
        return result;
    }
    
    bool IsSuccess() const { return m_bSuccess; }
    bool IsFailure() const { return !m_bSuccess; }
    int GetErrorCode() const { return m_nErrorCode; }
    const std::string& GetErrorMessage() const { return m_strErrorMessage; }
};

struct SCounterpartyLimits {
    double m_dCreditLimit;
    double m_dDailyLimit;
    double m_dProductLimit;
    double m_dCurrentExposure;
    double m_dAvailableCredit;
    
    SCounterpartyLimits()
        : m_dCreditLimit(0.0)
        , m_dDailyLimit(0.0)
        , m_dProductLimit(0.0)
        , m_dCurrentExposure(0.0)
        , m_dAvailableCredit(0.0)
    {}
};

struct SCreditReservation {
    std::string m_strTradeId;
    std::string m_strCounterpartyCode;
    double m_dReservedAmount;
    long m_nReservationTime;
    int m_nReservationStatus;
    
    SCreditReservation()
        : m_dReservedAmount(0.0)
        , m_nReservationTime(0)
        , m_nReservationStatus(0)
    {}
};

struct SSettlementInfo {
    long m_nSettlementId;
    std::string m_strTradeId;
    double m_dSettlementAmount;
    std::string m_strSettlementDate;
    int m_nSettlementStatus;
    std::string m_strSettlementCurrency;
    
    SSettlementInfo()
        : m_nSettlementId(0)
        , m_dSettlementAmount(0.0)
        , m_nSettlementStatus(0)
    {}
};
