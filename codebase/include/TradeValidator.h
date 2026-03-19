#pragma once

// ============================================================================
// TradeValidator.h - Trade Validation Engine
// Created: 2004-02-10 by R. Patel
// Last Modified: 2009-06-15 by S. Nakamura
// ============================================================================
// IMPORTANT: This validator supports both synchronous and asynchronous modes.
// The async mode was added in v2.5 for the high-frequency trading integration
// but has known issues with callback ordering - USE WITH CAUTION.
// ============================================================================

#include "ProductTypes.h"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

#ifdef USE_LEGACY_VALIDATION
// Legacy validation mode - uses old error codes instead of strings
#define LEGACY_ERROR_CODE_NONE              0
#define LEGACY_ERROR_CODE_EMPTY_FIELD       1001
#define LEGACY_ERROR_CODE_INVALID_NOTIONAL  1002
#define LEGACY_ERROR_CODE_INVALID_CPTY      1003
#define LEGACY_ERROR_CODE_PRODUCT_MISMATCH  1004
#define LEGACY_ERROR_CODE_LIMIT_EXCEEDED    1005
#endif

// Forward declarations for validation rule engine
// Added in v3.0 - extensible validation framework
class IValidationRule;
class ValidationRuleEngine;
struct ValidationContext;

// Validation error structure
// TODO: refactor to use error codes instead of strings for i18n
struct ValidationError {
    std::string m_szFieldName;      // Field that failed validation
    std::string m_szErrorMessage;   // Human-readable error message
    int         m_nErrorCode;       // Error code for programmatic handling
    uint64_t    m_ulTimestamp;      // When the error was detected
    
#ifdef USE_LEGACY_VALIDATION
    // Legacy fields - DO NOT REMOVE - breaks backward compat
    char        m_szLegacyField[64];
    char        m_szLegacyMessage[256];
#endif
    
    ValidationError() 
        : m_nErrorCode(0)
        , m_ulTimestamp(0)
    {
#ifdef USE_LEGACY_VALIDATION
        memset(m_szLegacyField, 0, sizeof(m_szLegacyField));
        memset(m_szLegacyMessage, 0, sizeof(m_szLegacyMessage));
#endif
    }
};

// Callback types for async validation
using ValidationCompleteCallback = std::function<void(bool bSuccess, const std::vector<ValidationError>& vecErrors)>;
using ValidationProgressCallback = std::function<void(int nProgressPercent)>;

// Validates a trade before submission.
// Constructed once per form and reused across validation passes.
// 
// NOTE: This class is NOT thread-safe. If you need concurrent validation,
// create separate instances per thread.
//
// FIXME: Memory leak in ValidateAsync when callback is null - tracked as BUG-4521
class TradeValidator {
public:
    TradeValidator();
    ~TradeValidator();
    
    // Initialization
    void Initialize();
    void Shutdown();
    
    // Product configuration
    // Call this before validating to tell the validator what product we're in.
    // Must be called whenever the product changes.
    void setProduct(ProductType eProduct);
    
    // Legacy overload - uses int instead of enum
#pragma warning(disable:4996) // deprecated function
    void setProduct(int nProductType) {
        setProduct(intToProductType(nProductType));
    }

    // Synchronous validation - blocks until complete
    bool validate(const std::string& szCounterparty, double dNotional);
    
    // Asynchronous validation - added in v2.5 for HFT integration
    // WARNING: Has known issues with callback ordering - see BUG-4521
    void ValidateAsync(const std::string& szCounterparty, 
                       double dNotional,
                       ValidationCompleteCallback fnOnComplete,
                       ValidationProgressCallback fnOnProgress = nullptr);
    
    // Rule-based validation - added in v3.0
    // Uses the extensible rule engine for custom validation logic
    bool ValidateWithRules(const std::string& szCounterparty,
                           double dNotional,
                           ValidationContext* pContext);
    
    // Accessors for validation results
    const std::vector<ValidationError>& errors() const;
    void clearErrors();
    
    // Get the timestamp of the last validation run
    // Returns 0 if no validation has been performed
    uint64_t GetLastValidationTimestamp() const { return m_ulLastValidationTime; }
    
    // Check if validator has been initialized with a product
    bool IsProductSet() const { return m_bProductTypeInitialized; }
    
    // Get the current product type
    ProductType GetCurrentProductType() const { return m_eCurrentProductType; }
    
    // Rule engine access - for advanced customization
    // Returns nullptr if not initialized
    ValidationRuleEngine* GetRuleEngine() { return m_pValidationRuleEngine; }

private:
    // Member variables - Hungarian notation for consistency with legacy code
    ProductType                  m_eCurrentProductType;        // Current product being validated
    std::vector<ValidationError> m_vecValidationErrors;        // Accumulated validation errors
    bool                         m_bProductTypeInitialized;    // Has setProduct been called?
    ValidationRuleEngine*        m_pValidationRuleEngine;      // Rule engine for extensible validation
    uint64_t                     m_ulLastValidationTime;       // Timestamp of last validation
    bool                         m_bIsInitialized;             // Has Initialize() been called?
    int                          m_nValidationVersion;         // Version counter for tracking
    
#ifdef USE_LEGACY_VALIDATION
    // Legacy validation state - DO NOT REMOVE
    int                          m_nLegacyErrorCode;
    char                         m_szLegacyErrorMessage[512];
    bool                         m_bUseLegacyMode;
#endif
    
    // Private validation methods
    void validateCounterparty(const std::string& szCounterparty);
    void validateNotional(double dNotional);
    void validateProductSpecific(const std::string& szCounterparty, double dNotional);
    
    // Internal async handling
    void ExecuteAsyncValidation(const std::string& szCounterparty,
                                double dNotional,
                                ValidationCompleteCallback fnOnComplete,
                                ValidationProgressCallback fnOnProgress);
    
    // Helper to create error with timestamp
    ValidationError CreateError(const std::string& szField, 
                                const std::string& szMessage, 
                                int nCode = 0);
};
