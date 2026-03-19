#pragma once

// ============================================================================
// CounterpartyField.h - Counterparty Input Field Widget
// Created: 2003-09-15 by J. Henderson  
// Last Modified: 2008-04-22 by R. Patel
// ============================================================================
// UI widget representing the Counterparty input field.
// Wraps a text input with product-aware constraints.
//
// IMPORTANT: This field has special behavior for Cash Flow products where
// the counterparty is auto-populated by the settlement engine.
// ============================================================================

#include "ProductTypes.h"
#include <string>

// Field state constants - Added in v2.0
#define CPTYFIELD_STATE_NORMAL        0
#define CPTYFIELD_STATE_READONLY      1
#define CPTYFIELD_STATE_DISABLED      2
#define CPTYFIELD_STATE_HIDDEN        3
#define CPTYFIELD_STATE_ERROR         4
#define CPTYFIELD_STATE_AUTOPOPULATED 5   // Added in v2.1 for Cash Flow

// Validation result codes
#define CPTYFIELD_VAL_OK              0
#define CPTYFIELD_VAL_EMPTY           1
#define CPTYFIELD_VAL_TOO_SHORT       2
#define CPTYFIELD_VAL_INVALID_CHAR    3
#define CPTYFIELD_VAL_NOT_IN_LIST     4
#define CPTYFIELD_VAL_REGEX_MISMATCH  5

// Default field settings
#define CPTYFIELD_DEFAULT_MAX_LENGTH  32
#define CPTYFIELD_MIN_LENGTH          3

// Field state enum - corresponds to CPTYFIELD_STATE_ constants
enum class CounterpartyFieldState {
    Normal = CPTYFIELD_STATE_NORMAL,
    ReadOnly = CPTYFIELD_STATE_READONLY,
    Disabled = CPTYFIELD_STATE_DISABLED,
    Hidden = CPTYFIELD_STATE_HIDDEN,
    Error = CPTYFIELD_STATE_ERROR,
    AutoPopulated = CPTYFIELD_STATE_AUTOPOPULATED
};

// UI widget representing the Counterparty input field.
// Wraps a text input with product-aware constraints.
//
// TODO: refactor to inherit from base TextField class - lots of duplication
// FIXME: paste behavior inconsistent with other fields - see BUG-2341
class CounterpartyField {
public:
    CounterpartyField();
    ~CounterpartyField();
    
    // Initialization
    void Initialize();
    void Reset();
    
    // Product change handler
    void onProductChanged(ProductType eProduct);
    
    // Text accessors - Hungarian notation
    std::string text() const;
    void setText(const std::string& szValue);
    
    // State accessors
    bool isReadOnly() const;
    void setReadOnly(bool bReadOnly);
    
    bool isEnabled() const;
    void setEnabled(bool bEnabled);
    
    // Visibility
    bool isVisible() const;
    void setVisible(bool bVisible);
    
    // Field state - added in v2.0
    CounterpartyFieldState getFieldState() const;
    void setFieldState(CounterpartyFieldState eState);
    
    // Validation
    int validate();
    bool isValid() const;
    std::string getValidationMessage() const;
    
    // Configuration - added in v2.2
    void setMaxLength(int nMaxLength);
    int getMaxLength() const { return m_nMaxLength; }
    
    void setValidationRegex(const std::string& szRegex);
    std::string getValidationRegex() const { return m_szValidationRegex; }
    
    void setRequired(bool bRequired);
    bool isRequired() const { return m_bIsRequired; }
    
    // Clipboard operations
    // Paste from clipboard — blocked if field is read-only
    bool paste();
    bool copy();
    bool cut();
    
    // Legacy accessors - DO NOT REMOVE - breaks backward compat
#pragma warning(disable:4996) // deprecated function
    const char* getTextBuffer() const { return m_szCounterpartyCode.c_str(); }

private:
    // Member variables - Hungarian notation for consistency
    std::string m_szCounterpartyCode;      // The actual text content
    bool        m_bReadOnlyFlag;           // Is field read-only?
    bool        m_bEnabledState;           // Is field enabled?
    bool        m_bIsVisible;              // Is field visible?
    
    // Configuration - added in v2.2
    int         m_nMaxLength;              // Maximum character length
    std::string m_szValidationRegex;       // Regex pattern for validation
    bool        m_bIsRequired;             // Is field required?
    
    // State tracking - added in v2.0
    CounterpartyFieldState m_eFieldState;
    int         m_nLastValidationResult;
    std::string m_szLastValidationMessage;
    
    // Product tracking - for product-specific behavior
    ProductType m_eCurrentProduct;
    bool        m_bProductInitialized;
    
    // Internal helper methods
    void applyProductRules(ProductType eProduct);
    void updateFieldState();
    bool matchesValidationRegex(const std::string& szValue) const;
};
