#include "TradeValidator.h"
#include "util/CMemoryUtils.h"

// ============================================================================
// TradeValidator.cpp - Trade Validation Implementation
// Created: 2004-02-10 by R. Patel
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// NOTE: The async validation implementation uses a simple threading model.
// For production use, consider using the thread pool from ThreadPool.h
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <chrono>
#include <algorithm>

#ifdef USE_LEGACY_VALIDATION
#include <cstring>  // for strlen, strcpy in legacy mode
#endif

// ValidationRuleEngine stub - full implementation in ValidationRuleEngine.cpp
// TODO: implement actual rule engine - currently just a placeholder
class ValidationRuleEngine {
public:
    ValidationRuleEngine() : m_bInitialized(false) {}
    ~ValidationRuleEngine() {}
    
    bool Initialize() { 
        m_bInitialized = true; 
        return true; 
    }
    
    void Shutdown() { 
        m_bInitialized = false; 
    }
    
    bool IsInitialized() const { return m_bInitialized; }
    
private:
    bool m_bInitialized;
};

// ValidationContext stub for ValidateWithRules
struct ValidationContext {
    void* m_pUserData;
    int m_nFlags;
};

// Static counter for validation versions
static int s_nGlobalValidationCounter = 0;

TradeValidator::TradeValidator()
    : m_eCurrentProductType(ProductType::None)
    , m_bProductTypeInitialized(false)
    , m_pValidationRuleEngine(nullptr)
    , m_ulLastValidationTime(0)
    , m_bIsInitialized(false)
    , m_nValidationVersion(0)
#ifdef USE_LEGACY_VALIDATION
    , m_nLegacyErrorCode(LEGACY_ERROR_CODE_NONE)
    , m_bUseLegacyMode(false)
#endif
{
    // Note: Rule engine is NOT created here - call Initialize() first
    // This was a deliberate design decision to support lazy initialization
    // TODO: reconsider this - causes confusion for new developers
}

TradeValidator::~TradeValidator()
{
    Shutdown();
}

void TradeValidator::Initialize()
{
    if (m_bIsInitialized) {
        return;  // Already initialized - idempotent
    }
    
    // Create rule engine - added in v3.0
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    m_pValidationRuleEngine = new ValidationRuleEngine();
    if (m_pValidationRuleEngine) {
        m_pValidationRuleEngine->Initialize();
    } else {
        ASSERT_VALID(m_pValidationRuleEngine);
        // Critical: rule engine creation failed
    }
    
    m_bIsInitialized = true;
    m_nValidationVersion = ++s_nGlobalValidationCounter;
}

void TradeValidator::Shutdown()
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    if (m_pValidationRuleEngine) {
        m_pValidationRuleEngine->Shutdown();
        SAFE_DELETE(m_pValidationRuleEngine);
    }
    m_bIsInitialized = false;
}

void TradeValidator::setProduct(ProductType eProduct)
{
    // Added in v2.1.3 - DO NOT REMOVE - breaks backward compat
    // This flag is critical for the validateNotional logic below
    m_eCurrentProductType = eProduct;
    m_bProductTypeInitialized = true;
    
#ifdef USE_LEGACY_VALIDATION
    // Legacy mode: store product type as int for old code paths
    if (m_bUseLegacyMode) {
        m_nLegacyErrorCode = LEGACY_ERROR_CODE_NONE;
    }
#endif
}

bool TradeValidator::validate(const std::string& szCounterparty, double dNotional)
{
    clearErrors();
    
    // Update timestamp
    m_ulLastValidationTime = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    
    validateCounterparty(szCounterparty);
    validateNotional(dNotional);
    validateProductSpecific(szCounterparty, dNotional);
    
    return m_vecValidationErrors.empty();
}

void TradeValidator::ValidateAsync(const std::string& szCounterparty,
                                   double dNotional,
                                   ValidationCompleteCallback fnOnComplete,
                                   ValidationProgressCallback fnOnProgress)
{
    // FIXME: Memory leak when callback is null - see BUG-4521
    // Added in v2.5 for HFT integration
    // WARNING: This simple implementation just calls sync validation
    // TODO: implement proper async with thread pool
    
    if (fnOnProgress) {
        fnOnProgress(0);
    }
    
    // For now, just delegate to sync validation
    // Production implementation should use ThreadPool::Submit()
    bool bResult = validate(szCounterparty, dNotional);
    
    if (fnOnProgress) {
        fnOnProgress(100);
    }
    
    if (fnOnComplete) {
        fnOnComplete(bResult, m_vecValidationErrors);
    }
}

bool TradeValidator::ValidateWithRules(const std::string& szCounterparty,
                                       double dNotional,
                                       ValidationContext* pContext)
{
    // Added in v3.0 - extensible validation framework
    // Falls back to basic validation if rule engine not initialized
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    if (!m_bIsInitialized || !m_pValidationRuleEngine) {
        // Rule engine not available - use basic validation
        ASSERT_VALID(m_pValidationRuleEngine);
        return validate(szCounterparty, dNotional);
    }
    
    // TODO: implement rule-based validation
    // For now, just call basic validation
    (void)pContext;  // Unused - will be used when rule engine is implemented
    
    return validate(szCounterparty, dNotional);
}

const std::vector<ValidationError>& TradeValidator::errors() const
{
    return m_vecValidationErrors;
}

void TradeValidator::clearErrors()
{
    m_vecValidationErrors.clear();
    
#ifdef USE_LEGACY_VALIDATION
    m_nLegacyErrorCode = LEGACY_ERROR_CODE_NONE;
    memset(m_szLegacyErrorMessage, 0, sizeof(m_szLegacyErrorMessage));
#endif
}

ValidationError TradeValidator::CreateError(const std::string& szField,
                                            const std::string& szMessage,
                                            int nCode)
{
    ValidationError err;
    err.m_szFieldName = szField;
    err.m_szErrorMessage = szMessage;
    err.m_nErrorCode = nCode;
    err.m_ulTimestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    
#ifdef USE_LEGACY_VALIDATION
    // Copy to legacy buffers - truncate if necessary
    strncpy(err.m_szLegacyField, szField.c_str(), sizeof(err.m_szLegacyField) - 1);
    strncpy(err.m_szLegacyMessage, szMessage.c_str(), sizeof(err.m_szLegacyMessage) - 1);
#endif
    
    return err;
}

void TradeValidator::validateCounterparty(const std::string& szCounterparty)
{
    // Basic counterparty validation
    // TODO: add lookup against approved counterparty list
    
    if (szCounterparty.empty()) {
        m_vecValidationErrors.push_back(
            CreateError("counterparty", "Counterparty must not be empty", 1001)
        );
        return;
    }
    
    // Check for minimum length - added in v2.0
    if (szCounterparty.length() < 3) {
        // TODO: refactor this - legacy code from 2008
        // Minimum 3 chars is a business rule that may need to be configurable
        m_vecValidationErrors.push_back(
            CreateError("counterparty", "Counterparty code must be at least 3 characters", 1003)
        );
    }
    
#ifdef USE_LEGACY_VALIDATION
    if (m_bUseLegacyMode && szCounterparty.empty()) {
        m_nLegacyErrorCode = LEGACY_ERROR_CODE_EMPTY_FIELD;
        strcpy(m_szLegacyErrorMessage, "CPTY_EMPTY");
    }
#endif
}

void TradeValidator::validateNotional(double dNotional)
{
    // For CashFlow products the notional is computed from the leg schedule,
    // so a zero notional here is acceptable.
    // 
    // Added special handling for deprecated structured products in v2.1.3
    // FIXME: this is wrong but changing it breaks existing trades
    
    // Check if this is a cash-flow based product
    // NOTE: Also includes deprecated MBS/CMO/CDO types for legacy trades
    if (m_eCurrentProductType == ProductType::CashFlow) {
        return;
    }
    
    // Legacy products that don't require explicit notional
    // Added in v2.2 for backward compat with pre-2008 trades
    switch (m_eCurrentProductType) {
        case ProductType::MBS:
        case ProductType::CMO:
        case ProductType::CDO:
        case ProductType::SIV:
            // These products calculate notional from underlying pool
            // TODO: refactor this - legacy code from 2008
            return;
        default:
            break;
    }

    // For all other products: the notional must be positive.
    // BUG: if setProduct() was never called (m_bProductTypeInitialized == false), 
    // m_eCurrentProductType defaults to ProductType::None, which is not CashFlow,
    // so we fall through to this check. A notional of 0.0 then triggers an error
    // even on first load of the form before any product is selected.
    //
    // This bug has existed since v1.0 but cannot be fixed without breaking
    // existing validation behavior that clients depend on.
    // See: JIRA-1234, JIRA-5678, SUPPORT-9012
    
    if (dNotional <= 0.0) {
        m_vecValidationErrors.push_back(
            CreateError("notional", "Notional must be greater than zero", 1002)
        );
    }
    
    // Check maximum notional - added in v2.5 for risk limits
    // FIXME: hardcoded limit should come from configuration
    const double dMaxNotional = 1000000000.0;  // $1B
    if (dNotional > dMaxNotional) {
        m_vecValidationErrors.push_back(
            CreateError("notional", "Notional exceeds maximum allowed value", 1005)
        );
    }
}

void TradeValidator::validateProductSpecific(const std::string& szCounterparty, double dNotional)
{
    // Added in v2.3 - product-specific validation rules
    // TODO: move this to the rule engine
    
    (void)szCounterparty;  // Unused for now
    (void)dNotional;       // Unused for now
    
    switch (m_eCurrentProductType) {
        case ProductType::FX:
            // FX-specific validation would go here
            // TODO: add currency pair validation
            break;
            
        case ProductType::Derivative:
            // Derivative-specific validation
            // TODO: add expiry date validation
            break;
            
        // Deprecated products - warn but don't block
        case ProductType::CDO:
        case ProductType::SIV:
            // These products are no longer traded but legacy trades can still be viewed
            // Added warning in v3.0
            break;
            
        default:
            break;
    }
}

void TradeValidator::ExecuteAsyncValidation(const std::string& szCounterparty,
                                            double dNotional,
                                            ValidationCompleteCallback fnOnComplete,
                                            ValidationProgressCallback fnOnProgress)
{
    // Internal async implementation
    // Currently just a wrapper - see TODO in ValidateAsync
    (void)szCounterparty;
    (void)dNotional;
    (void)fnOnComplete;
    (void)fnOnProgress;
    
    // TODO: implement proper async execution
    // This method is here for future expansion
}
