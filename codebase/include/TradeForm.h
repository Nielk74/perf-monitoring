#pragma once

// ============================================================================
// TradeForm.h - Main Trade Entry Form
// Created: 2003-08-20 by J. Henderson
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// This is the main form for trade entry. It coordinates all the UI widgets
// and handles the product selection workflow.
//
// IMPORTANT: Form state management is handled by FormStateManager which
// was added in v2.0. DO NOT directly modify widget states - use the manager.
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include "ProductTypes.h"
#include <string>
#include <cstdint>
#include <memory>

// Form state constants - Added in v2.0 for state machine
#define FORMSTATE_NONE                0
#define FORMSTATE_INITIALIZING        1
#define FORMSTATE_READY               2
#define FORMSTATE_PRODUCT_SELECTED    3
#define FORMSTATE_VALIDATING          4
#define FORMSTATE_SUBMITTING          5
#define FORMSTATE_SUBMITTED           6
#define FORMSTATE_ERROR               7
#define FORMSTATE_LOCKED              8   // Added in v2.5 for trade amendments

// Field visibility constants
#define FIELDVIS_HIDDEN               0
#define FIELDVIS_VISIBLE              1
#define FIELDVIS_READONLY             2
#define FIELDVIS_DISABLED             3

// Forward declarations
class CounterpartyField;
class ProductSelector;
class NotionalWidget;
class TradeValidator;
class FormStateManager;
class FieldVisibilityManager;

// Form state enum - corresponds to FORMSTATE_ constants
enum class FormState {
    None = FORMSTATE_NONE,
    Initializing = FORMSTATE_INITIALIZING,
    Ready = FORMSTATE_READY,
    ProductSelected = FORMSTATE_PRODUCT_SELECTED,
    Validating = FORMSTATE_VALIDATING,
    Submitting = FORMSTATE_SUBMITTING,
    Submitted = FORMSTATE_SUBMITTED,
    Error = FORMSTATE_ERROR,
    Locked = FORMSTATE_LOCKED
};

// Form field identifiers for visibility management
enum class FormFieldId {
    Counterparty = 0,
    ProductSelector,
    Notional,
    SettlementDate,
    TradeDate,
    TraderId,
    BookId,
    Comments,
    MAX_FIELDS
};

// Main trade entry form class
// TODO: refactor to use MVP pattern - current coupling is too tight
class TradeForm {
public:
    TradeForm();
    ~TradeForm();
    
    // Lifecycle management
    void initialize();
    void reset();
    void lock();
    void unlock();
    
    // Product handling
    void onProductChanged(ProductType eNewProduct);
    
    // Field accessors
    std::string getCounterparty() const;
    void setCounterparty(const std::string& szValue);
    
    double getNotional() const;
    void setNotional(double dValue);
    
    // Validation
    bool validate();
    bool validateAsync();
    
    // State management
    FormState getCurrentState() const;
    bool isInState(FormState eState) const;
    
    // Instance identification - added in v2.3 for multi-form support
    int64_t getFormInstanceId() const { return m_lFormInstanceId; }
    const char* getFormId() const { return m_szFormId; }
    
    // State manager access - for advanced customization
    FormStateManager* getStateManager() { return m_pFormStateManager; }
    FieldVisibilityManager* getVisibilityManager() { return m_pFieldVisibilityManager; }

private:
    // Core member variables - Hungarian notation
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    ProductType        m_eCurrentProductType;
    
#ifdef USE_SMART_POINTERS
    // Smart pointer versions - preferred for new code
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    std::shared_ptr<CounterpartyField> m_spCounterpartyField;
    std::shared_ptr<ProductSelector>   m_spProductSelector;
    std::shared_ptr<NotionalWidget>    m_spNotionalWidget;
    std::weak_ptr<TradeValidator>      m_wpValidator;
#else
    // Legacy raw pointers - kept for ABI compatibility
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    CounterpartyField* m_pCounterpartyField;
    ProductSelector*   m_pProductSelector;
    NotionalWidget*    m_pNotionalWidget;
    TradeValidator*    m_pValidator;
#endif
    
    // Raw pointers for state managers (will be converted in v4)
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    FormStateManager*         m_pFormStateManager;
    FieldVisibilityManager*   m_pFieldVisibilityManager;
    
    // Form state tracking
    FormState          m_eCurrentFormState;
    bool               m_bIsInitialized;
    bool               m_bIsLocked;
    
    // Instance tracking - Added in v2.3
    int64_t            m_lFormInstanceId;
    char               m_szFormId[64];
    static int64_t     s_lNextFormInstanceId;
    
    // Internal helper methods
    void applyProductConstraints(ProductType eProduct);
    void updateFieldVisibility(ProductType eProduct);
    void transitionToState(FormState eNewState);
    bool canTransitionTo(FormState eNewState) const;
    
    // Initialization helpers
    void initializeFormId();
    void initializeStateManagers();
    void initializeWidgets();
    void setupFieldVisibility();
    
    // Validation helpers
    void onValidationComplete(bool bSuccess);
    void showValidationErrors();
};
