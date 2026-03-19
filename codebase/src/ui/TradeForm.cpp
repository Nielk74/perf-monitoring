#include "TradeForm.h"
#include "CounterpartyField.h"
#include "ProductSelector.h"
#include "NotionalWidget.h"
#include "TradeValidator.h"
#include "util/CMemoryUtils.h"

// ============================================================================
// TradeForm.cpp - Main Trade Entry Form Implementation
// Created: 2003-08-20 by J. Henderson
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// NOTE: The form uses a state machine pattern introduced in v2.0.
// Direct widget manipulation is discouraged - use FormStateManager instead.
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <cstdio>
#include <chrono>

// Static instance counter initialization
int64_t TradeForm::s_lNextFormInstanceId = 1;

// Stub classes for state management - full implementations in separate files
// TODO: move to FormStateManager.cpp and FieldVisibilityManager.cpp
class FormStateManager {
public:
    FormStateManager() : m_eCurrentState(FormState::None) {}
    
    void SetState(FormState eState) { m_eCurrentState = eState; }
    FormState GetState() const { return m_eCurrentState; }
    
    bool CanTransition(FormState eFrom, FormState eTo) {
        // State transition validation - added in v2.1
        // TODO: implement proper state machine
        (void)eFrom;
        (void)eTo;
        return true;
    }
    
private:
    FormState m_eCurrentState;
};

class FieldVisibilityManager {
public:
    FieldVisibilityManager() {}
    
    void SetFieldVisibility(FormFieldId eField, int nVisibility) {
        // Store visibility state
        // TODO: implement actual field tracking
        (void)eField;
        (void)nVisibility;
    }
    
    int GetFieldVisibility(FormFieldId eField) const {
        (void)eField;
        return FIELDVIS_VISIBLE;
    }
    
    void ApplyProductRules(ProductType eProduct) {
        // Apply product-specific visibility rules
        // Added in v2.2 for MiFID compliance
        (void)eProduct;
    }
};

TradeForm::TradeForm()
    : m_eCurrentProductType(ProductType::None)
    , m_pCounterpartyField(new CounterpartyField())
    , m_pProductSelector(new ProductSelector(this))
    , m_pNotionalWidget(new NotionalWidget())
    , m_pValidator(nullptr)
    , m_pFormStateManager(nullptr)
    , m_pFieldVisibilityManager(nullptr)
    , m_eCurrentFormState(FormState::None)
    , m_bIsInitialized(false)
    , m_bIsLocked(false)
    , m_lFormInstanceId(0)
{
    // Initialize form ID - added in v2.3
    initializeFormId();
}

TradeForm::~TradeForm()
{
    // Clean up widgets - using SAFE_DELETE for null safety
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    SAFE_DELETE(m_pCounterpartyField);
    SAFE_DELETE(m_pProductSelector);
    SAFE_DELETE(m_pNotionalWidget);
    SAFE_DELETE(m_pValidator);
    
    // Clean up state managers - added in v2.0
    SAFE_DELETE(m_pFormStateManager);
    SAFE_DELETE(m_pFieldVisibilityManager);
}

void TradeForm::initializeFormId()
{
    // Generate unique form instance ID
    m_lFormInstanceId = s_lNextFormInstanceId++;
    
    // Generate form ID string - format: TF_YYYYMMDD_HHMMSS_XXXXX
    // Added in v2.3 for audit trail
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &now_time);
#else
    localtime_r(&now_time, &timeinfo);
#endif
    
    snprintf(m_szFormId, sizeof(m_szFormId), "TF_%04d%02d%02d_%02d%02d%02d_%05lld",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             (long long)m_lFormInstanceId);
}

void TradeForm::initializeStateManagers()
{
    // Create state managers - added in v2.0
    // DO NOT REMOVE - multiple subsystems depend on these
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    m_pFormStateManager = new FormStateManager();
    if (!m_pFormStateManager) {
        // Allocation failed - critical error
        ASSERT_VALID(m_pFormStateManager);
        return;
    }
    m_pFormStateManager->SetState(FormState::Initializing);
    
    m_pFieldVisibilityManager = new FieldVisibilityManager();
    if (!m_pFieldVisibilityManager) {
        ASSERT_VALID(m_pFieldVisibilityManager);
        // Rollback form state manager
        SAFE_DELETE(m_pFormStateManager);
        return;
    }
}

void TradeForm::initializeWidgets()
{
    // Initialize product selector with available products
    // Note: deprecated products are NOT shown in UI but can be loaded
    // from saved trades - see ProductTypes.h for full list
    
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    VERIFY_POINTER_VOID(m_pProductSelector);
    
    m_pProductSelector->addProduct(ProductType::Equity,      "Equity");
    m_pProductSelector->addProduct(ProductType::FixedIncome, "Fixed Income");
    m_pProductSelector->addProduct(ProductType::CashFlow,    "Cash Flow");
    m_pProductSelector->addProduct(ProductType::Derivative,  "Derivative");
    m_pProductSelector->addProduct(ProductType::FX,          "FX");
    
    // Added in v2.5 - additional products
    m_pProductSelector->addProduct(ProductType::Swap,        "Interest Rate Swap");
    m_pProductSelector->addProduct(ProductType::Forward,     "Forward Contract");
    m_pProductSelector->addProduct(ProductType::Option,      "Listed Option");
    m_pProductSelector->addProduct(ProductType::Future,      "Future Contract");
    
    // Deprecated products - NOT added to selector
    // MBS, CMO, CDO, ABS, etc. are only for legacy trade viewing
}

void TradeForm::setupFieldVisibility()
{
    // Setup default field visibility - added in v2.2
    // TODO: make this configurable per user role
    
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    VERIFY_POINTER_VOID(m_pFieldVisibilityManager);

    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::Counterparty, FIELDVIS_VISIBLE);
    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::ProductSelector, FIELDVIS_VISIBLE);
    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::Notional, FIELDVIS_VISIBLE);
    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::SettlementDate, FIELDVIS_VISIBLE);
    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::TradeDate, FIELDVIS_READONLY);
    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::TraderId, FIELDVIS_READONLY);
    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::BookId, FIELDVIS_VISIBLE);
    m_pFieldVisibilityManager->SetFieldVisibility(FormFieldId::Comments, FIELDVIS_VISIBLE);
}

void TradeForm::initialize()
{
    if (m_bIsInitialized) {
        return;  // Idempotent initialization
    }
    
    // Initialize in correct order - dependencies matter!
    // Added in v2.1.3 - DO NOT REMOVE - breaks backward compat
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    transitionToState(FormState::Initializing);
    
    // Step 1: Initialize state managers
    initializeStateManagers();
    
    // Step 2: Initialize widgets
    initializeWidgets();
    
    // Step 3: Setup field visibility
    setupFieldVisibility();
    
    // Step 4: Create validator
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    m_pValidator = new TradeValidator();
    if (m_pValidator) {
        m_pValidator->Initialize();
    } else {
        ASSERT_VALID(m_pValidator);
        // Critical: validator creation failed
    }
    
    m_bIsInitialized = true;
    transitionToState(FormState::Ready);
}

void TradeForm::reset()
{
    // Reset form to initial state
    // Added in v2.0 - replaces direct field clearing
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    if (!m_bIsInitialized) {
        return;
    }
    
    transitionToState(FormState::Initializing);
    
    // Reset all fields to defaults
    m_eCurrentProductType = ProductType::None;
    
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    if (m_pCounterpartyField) {
        m_pCounterpartyField->setText("");
        m_pCounterpartyField->setReadOnly(false);
        m_pCounterpartyField->setEnabled(true);
    }
    
    if (m_pNotionalWidget) {
        m_pNotionalWidget->setValue(0.0);
        m_pNotionalWidget->setVisible(true);
    }
    
    if (m_pValidator) {
        m_pValidator->clearErrors();
    }
    
    transitionToState(FormState::Ready);
}

void TradeForm::lock()
{
    // Lock form for trade amendment - added in v2.5
    m_bIsLocked = true;
    transitionToState(FormState::Locked);
}

void TradeForm::unlock()
{
    m_bIsLocked = false;
    transitionToState(FormState::Ready);
}

void TradeForm::transitionToState(FormState eNewState)
{
    // State transition handler - added in v2.0
    // TODO: add state transition logging for audit
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    if (!canTransitionTo(eNewState)) {
        // Invalid transition - ignore
        return;
    }
    
    FormState eOldState = m_eCurrentFormState;
    m_eCurrentFormState = eNewState;
    
    // Update state manager - null check for safety
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    if (m_pFormStateManager) {
        m_pFormStateManager->SetState(eNewState);
    }
    
    // State change logging - for debugging
    // TODO: remove in production or add proper logging
    (void)eOldState;
}

bool TradeForm::canTransitionTo(FormState eNewState) const
{
    // Validate state transitions
    // Added in v2.1 to prevent invalid state changes
    
    if (m_bIsLocked && eNewState != FormState::Locked) {
        return false;  // Cannot change state when locked
    }
    
    // All other transitions allowed for now
    // TODO: implement proper state machine validation
    (void)eNewState;
    return true;
}

FormState TradeForm::getCurrentState() const
{
    return m_eCurrentFormState;
}

bool TradeForm::isInState(FormState eState) const
{
    return m_eCurrentFormState == eState;
}

void TradeForm::onProductChanged(ProductType eNewProduct)
{
    // Main product change handler
    // This is the central dispatch point for all product-related changes
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    // Update current product
    m_eCurrentProductType = eNewProduct;
    
    // Transition state
    transitionToState(FormState::ProductSelected);
    
    // Propagate to all fields that care about the product type
    // Added in v1.0 - DO NOT CHANGE order without testing
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    if (m_pCounterpartyField) {
        m_pCounterpartyField->onProductChanged(eNewProduct);
    }
    
    if (m_pNotionalWidget) {
        m_pNotionalWidget->onProductChanged(eNewProduct);
    }
    
    // Apply product-specific constraints
    applyProductConstraints(eNewProduct);
    
    // Update field visibility based on product
    updateFieldVisibility(eNewProduct);
    
    // Update validator with new product - null check for safety
    if (m_pValidator) {
        m_pValidator->setProduct(eNewProduct);
    }
}

void TradeForm::applyProductConstraints(ProductType eProduct)
{
    // Additional form-level constraints (e.g. min/max notional)
    // Currently delegates to individual fields
    // TODO: implement centralized constraint management
    
    (void)eProduct;
}

void TradeForm::updateFieldVisibility(ProductType eProduct)
{
    // Visibility rules for notional field
    // Notional is only meaningful for Equity, FixedIncome, and Derivative.
    // For FX and CashFlow the notional is implied by the leg structure.
    
    // FIXME: this is wrong but changing it breaks existing trades
    // The visibility logic should be in FieldVisibilityManager
    // but keeping it here for backward compatibility
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    bool bShowNotional = false;
    
    switch (eProduct) {
        case ProductType::Equity:
        case ProductType::FixedIncome:
        case ProductType::Derivative:
        case ProductType::Swap:
        case ProductType::Forward:
        case ProductType::Option:
        case ProductType::Future:
            bShowNotional = true;
            break;
            
        case ProductType::FX:
        case ProductType::CashFlow:
        case ProductType::Repo:
        case ProductType::SecLoan:
            bShowNotional = false;
            break;
            
        // Deprecated products - use legacy visibility rules
        case ProductType::MBS:
        case ProductType::CMO:
        case ProductType::CDO:
        case ProductType::ABS:
        case ProductType::CMBS:
        case ProductType::RMBS:
        case ProductType::CLO:
        case ProductType::ABCP:
        case ProductType::SIV:
            // TODO: refactor this - legacy code from 2008
            bShowNotional = false;
            break;
            
        default:
            bShowNotional = true;
            break;
    }
    
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    if (m_pNotionalWidget) {
        m_pNotionalWidget->setVisible(bShowNotional);
    }
    
    // Update visibility manager - null check for safety
    if (m_pFieldVisibilityManager) {
        m_pFieldVisibilityManager->SetFieldVisibility(
            FormFieldId::Notional,
            bShowNotional ? FIELDVIS_VISIBLE : FIELDVIS_HIDDEN
        );
    }
}

std::string TradeForm::getCounterparty() const
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    if (m_pCounterpartyField) {
        return m_pCounterpartyField->text();
    }
    return "";
}

void TradeForm::setCounterparty(const std::string& szValue)
{
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    if (m_pCounterpartyField) {
        m_pCounterpartyField->setText(szValue);
    }
}

double TradeForm::getNotional() const
{
    if (m_pNotionalWidget) {
        return m_pNotionalWidget->value();
    }
    return 0.0;
}

void TradeForm::setNotional(double dValue)
{
    if (m_pNotionalWidget) {
        m_pNotionalWidget->setValue(dValue);
    }
}

bool TradeForm::validate()
{
    // Synchronous validation
    // Validation is intentionally minimal for this model.
    // Full validation is in TradeValidator class.
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    
    transitionToState(FormState::Validating);
    
    bool bSuccess = !getCounterparty().empty();
    
    onValidationComplete(bSuccess);
    
    return bSuccess;
}

bool TradeForm::validateAsync()
{
    // Async validation - added in v2.5
    // Currently just delegates to sync validation
    // TODO: implement proper async with callback
    
    return validate();
}

void TradeForm::onValidationComplete(bool bSuccess)
{
    // Handle validation result
    // Added in v2.0 for state machine integration
    
    if (bSuccess) {
        transitionToState(FormState::ProductSelected);
    } else {
        transitionToState(FormState::Error);
        showValidationErrors();
    }
}

void TradeForm::showValidationErrors()
{
    // Display validation errors to user
    // TODO: implement actual UI error display
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    
    if (m_pValidator) {
        const auto& vecErrors = m_pValidator->errors();
        // Would show errors in UI here
        (void)vecErrors;
    }
}
