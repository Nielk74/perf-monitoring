#include "CounterpartyField.h"
#include "util/CMemoryUtils.h"

// ============================================================================
// CounterpartyField.cpp - Counterparty Input Field Implementation
// Created: 2003-09-15 by J. Henderson
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// NOTE: The read-only vs disabled distinction is important:
// - read-only: field is grey but text is selectable/copyable
// - disabled: field is completely inactive
// For Cash Flow products, we want DISABLED but legacy code uses read-only.
// This is a known bug - see BUG-2341
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <algorithm>
#include <cctype>

// Default validation regex - alphanumeric plus some special chars
// Added in v2.2 for counterparty code validation
static const char* DEFAULT_CPTY_REGEX = "^[A-Z][A-Z0-9_\\-\\.]{2,31}$";

CounterpartyField::CounterpartyField()
    : m_bReadOnlyFlag(false)
    , m_bEnabledState(true)
    , m_bIsVisible(true)
    , m_nMaxLength(CPTYFIELD_DEFAULT_MAX_LENGTH)
    , m_szValidationRegex(DEFAULT_CPTY_REGEX)
    , m_bIsRequired(true)
    , m_eFieldState(CounterpartyFieldState::Normal)
    , m_nLastValidationResult(CPTYFIELD_VAL_OK)
    , m_eCurrentProduct(ProductType::None)
    , m_bProductInitialized(false)
{
}

CounterpartyField::~CounterpartyField()
{
    // Nothing to clean up - no dynamic memory
}

void CounterpartyField::Initialize()
{
    // Full initialization - added in v2.0
    m_bReadOnlyFlag = false;
    m_bEnabledState = true;
    m_bIsVisible = true;
    m_eFieldState = CounterpartyFieldState::Normal;
    m_nLastValidationResult = CPTYFIELD_VAL_OK;
    m_szLastValidationMessage.clear();
}

void CounterpartyField::Reset()
{
    // Reset to default state
    m_szCounterpartyCode.clear();
    m_bReadOnlyFlag = false;
    m_bEnabledState = true;
    m_eFieldState = CounterpartyFieldState::Normal;
    m_nLastValidationResult = CPTYFIELD_VAL_OK;
    m_szLastValidationMessage.clear();
}

void CounterpartyField::onProductChanged(ProductType eProduct)
{
    // Store current product for reference
    m_eCurrentProduct = eProduct;
    m_bProductInitialized = true;
    
    // Delegate all product-specific setup to one place
    applyProductRules(eProduct);
    
    // Update field state based on new product
    updateFieldState();
}

void CounterpartyField::applyProductRules(ProductType eProduct)
{
    // For Cash Flow products, the counterparty is determined automatically
    // by the settlement engine and must not be edited by the user.
    //
    // NOTE: the original intent was to disable the field (greyed-out, still
    // readable), but setReadOnly(true) was used instead of setEnabled(false).
    // read-only state in this widget blocks ALL input including paste.
    //
    // BUG: should be setEnabled(false) to match other locked fields
    // setReadOnly blocks paste; setEnabled(false) would show it greyed
    // but still allow copy. For now this means users cannot paste either.
    //
    // See: BUG-2341, JIRA-5678
    // FIXME: this is wrong but changing it breaks existing trades
    
    switch (eProduct) {
        case ProductType::CashFlow:
            // Cash Flow: counterparty auto-populated, field should be disabled
            // Legacy bug: using read-only instead of disabled
            setReadOnly(true);   // BUG: should be setEnabled(false)
            m_eFieldState = CounterpartyFieldState::AutoPopulated;
            break;
            
        case ProductType::FX:
            // FX: counterparty required, normal editing
            setReadOnly(false);
            setEnabled(true);
            m_eFieldState = CounterpartyFieldState::Normal;
            break;
            
        case ProductType::Equity:
        case ProductType::FixedIncome:
        case ProductType::Derivative:
        case ProductType::Swap:
        case ProductType::Forward:
        case ProductType::Option:
        case ProductType::Future:
            // Standard products: normal editing
            setReadOnly(false);
            setEnabled(true);
            m_eFieldState = CounterpartyFieldState::Normal;
            break;
            
        // Deprecated products - special handling
        case ProductType::MBS:
        case ProductType::CMO:
        case ProductType::CDO:
        case ProductType::ABS:
        case ProductType::CMBS:
        case ProductType::RMBS:
        case ProductType::CLO:
        case ProductType::ABCP:
        case ProductType::SIV:
            // Legacy products: read-only for viewing only
            // TODO: refactor this - legacy code from 2008
            setReadOnly(true);
            m_eFieldState = CounterpartyFieldState::ReadOnly;
            break;
            
        case ProductType::Repo:
        case ProductType::SecLoan:
            // Securities lending: counterparty from approved list
            setReadOnly(false);
            setEnabled(true);
            m_bIsRequired = true;
            break;
            
        default:
            // Unknown product: allow editing
            setReadOnly(false);
            setEnabled(true);
            m_eFieldState = CounterpartyFieldState::Normal;
            break;
    }
}

void CounterpartyField::updateFieldState()
{
    // Update field state based on current flags
    // Added in v2.0 for state machine integration
    
    if (!m_bEnabledState) {
        m_eFieldState = CounterpartyFieldState::Disabled;
    } else if (m_bReadOnlyFlag) {
        // Don't override AutoPopulated state
        if (m_eFieldState != CounterpartyFieldState::AutoPopulated) {
            m_eFieldState = CounterpartyFieldState::ReadOnly;
        }
    } else if (!m_bIsVisible) {
        m_eFieldState = CounterpartyFieldState::Hidden;
    } else if (m_nLastValidationResult != CPTYFIELD_VAL_OK) {
        m_eFieldState = CounterpartyFieldState::Error;
    } else {
        m_eFieldState = CounterpartyFieldState::Normal;
    }
}

std::string CounterpartyField::text() const
{
    return m_szCounterpartyCode;
}

void CounterpartyField::setText(const std::string& szValue)
{
    // Validate input before setting
    // Added in v2.2 for input validation
    
    if (m_bReadOnlyFlag) {
        // Silently ignore - field is read-only
        return;
    }
    
    if (!m_bEnabledState) {
        // Silently ignore - field is disabled
        return;
    }
    
    // Truncate to max length
    std::string szTruncated = szValue;
    if (static_cast<int>(szTruncated.length()) > m_nMaxLength) {
        szTruncated = szTruncated.substr(0, m_nMaxLength);
    }
    
    // Store the value
    m_szCounterpartyCode = szTruncated;
    
    // Revalidate
    validate();
}

bool CounterpartyField::isReadOnly() const
{
    return m_bReadOnlyFlag;
}

void CounterpartyField::setReadOnly(bool bReadOnly)
{
    m_bReadOnlyFlag = bReadOnly;
    updateFieldState();
}

bool CounterpartyField::isEnabled() const
{
    return m_bEnabledState;
}

void CounterpartyField::setEnabled(bool bEnabled)
{
    m_bEnabledState = bEnabled;
    updateFieldState();
}

bool CounterpartyField::isVisible() const
{
    return m_bIsVisible;
}

void CounterpartyField::setVisible(bool bVisible)
{
    m_bIsVisible = bVisible;
    updateFieldState();
}

CounterpartyFieldState CounterpartyField::getFieldState() const
{
    return m_eFieldState;
}

void CounterpartyField::setFieldState(CounterpartyFieldState eState)
{
    m_eFieldState = eState;
    
    // Update flags to match state
    switch (eState) {
        case CounterpartyFieldState::Normal:
            m_bReadOnlyFlag = false;
            m_bEnabledState = true;
            m_bIsVisible = true;
            break;
        case CounterpartyFieldState::ReadOnly:
            m_bReadOnlyFlag = true;
            m_bEnabledState = true;
            m_bIsVisible = true;
            break;
        case CounterpartyFieldState::Disabled:
            m_bEnabledState = false;
            m_bIsVisible = true;
            break;
        case CounterpartyFieldState::Hidden:
            m_bIsVisible = false;
            break;
        case CounterpartyFieldState::AutoPopulated:
            m_bReadOnlyFlag = true;
            m_bEnabledState = true;
            break;
        default:
            break;
    }
}

void CounterpartyField::setMaxLength(int nMaxLength)
{
    m_nMaxLength = std::max(1, nMaxLength);
    
    // Truncate current value if necessary
    if (static_cast<int>(m_szCounterpartyCode.length()) > m_nMaxLength) {
        m_szCounterpartyCode = m_szCounterpartyCode.substr(0, m_nMaxLength);
    }
}

void CounterpartyField::setValidationRegex(const std::string& szRegex)
{
    m_szValidationRegex = szRegex;
    
    // Revalidate with new regex
    if (!m_szCounterpartyCode.empty()) {
        validate();
    }
}

void CounterpartyField::setRequired(bool bRequired)
{
    m_bIsRequired = bRequired;
}

int CounterpartyField::validate()
{
    // Validate current content
    // Returns validation result code
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    m_nLastValidationResult = CPTYFIELD_VAL_OK;
    m_szLastValidationMessage.clear();
    
    // Check required
    if (m_bIsRequired && m_szCounterpartyCode.empty()) {
        m_nLastValidationResult = CPTYFIELD_VAL_EMPTY;
        m_szLastValidationMessage = "Counterparty is required";
        updateFieldState();
        return m_nLastValidationResult;
    }
    
    // Check minimum length
    if (!m_szCounterpartyCode.empty() && 
        static_cast<int>(m_szCounterpartyCode.length()) < CPTYFIELD_MIN_LENGTH) {
        m_nLastValidationResult = CPTYFIELD_VAL_TOO_SHORT;
        m_szLastValidationMessage = "Counterparty code must be at least " 
            + std::to_string(CPTYFIELD_MIN_LENGTH) + " characters";
        updateFieldState();
        return m_nLastValidationResult;
    }
    
    // Check regex pattern - null safety for regex string
    if (!m_szCounterpartyCode.empty() && !m_szValidationRegex.empty()) {
        if (!matchesValidationRegex(m_szCounterpartyCode)) {
            m_nLastValidationResult = CPTYFIELD_VAL_REGEX_MISMATCH;
            m_szLastValidationMessage = "Counterparty code format is invalid";
            updateFieldState();
            return m_nLastValidationResult;
        }
    }
    
    updateFieldState();
    return m_nLastValidationResult;
}

bool CounterpartyField::isValid() const
{
    return m_nLastValidationResult == CPTYFIELD_VAL_OK;
}

std::string CounterpartyField::getValidationMessage() const
{
    return m_szLastValidationMessage;
}

bool CounterpartyField::matchesValidationRegex(const std::string& szValue) const
{
    // Simple regex matching - basic implementation
    // TODO: use proper regex library
    // For now, just check basic alphanumeric pattern
    
    if (szValue.empty()) return true;
    
    // First char must be uppercase letter
    if (!std::isupper(static_cast<unsigned char>(szValue[0]))) {
        return false;
    }
    
    // Rest must be alphanumeric, underscore, hyphen, or dot
    for (size_t i = 1; i < szValue.length(); ++i) {
        char c = szValue[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && 
            c != '_' && c != '-' && c != '.') {
            return false;
        }
    }
    
    return true;
}

bool CounterpartyField::paste()
{
    // Paste is blocked when the field is read-only.
    // This is standard behaviour for read-only text inputs.
    //
    // BUG: For Cash Flow products, paste is blocked because we use
    // read-only instead of disabled. Should allow paste for view-only fields.
    
    if (m_bReadOnlyFlag) {
        return false;  // silently ignore — no error shown to user
    }
    
    if (!m_bEnabledState) {
        return false;
    }
    
    // In the real implementation this would read from the clipboard.
    // For the purposes of this minimal model, we just report success.
    return true;
}

bool CounterpartyField::copy()
{
    // Copy is always allowed if there's content
    // Added in v2.1 for consistency with other fields
    
    if (m_szCounterpartyCode.empty()) {
        return false;
    }
    
    // In real implementation, would copy to clipboard
    return true;
}

bool CounterpartyField::cut()
{
    // Cut is blocked when read-only or disabled
    
    if (m_bReadOnlyFlag || !m_bEnabledState) {
        return false;
    }
    
    // Copy then clear
    if (copy()) {
        m_szCounterpartyCode.clear();
        return true;
    }
    
    return false;
}
