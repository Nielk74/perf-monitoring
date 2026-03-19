#pragma once

// ============================================================================
// NotionalWidget.h - Notional Amount Input Widget
// Created: 2003-10-05 by J. Henderson
// Last Modified: 2008-07-18 by M. Kowalski
// ============================================================================
// Widget showing the Notional amount input.
// Only visible for certain product types.
//
// IMPORTANT: Notional handling varies by product:
// - Equity/FixedIncome/Derivative: explicit notional required
// - FX/CashFlow: notional computed from legs, field hidden
// - Structured products (deprecated): notional from underlying pool
// ============================================================================

#include "ProductTypes.h"
#include <string>

// Precision constants - Added in v2.0 for currency handling
#define NOTIONALWIDGET_PRECISION_DEFAULT   2
#define NOTIONALWIDGET_PRECISION_HIGH      6    // For JPY and high-precision products
#define NOTIONALWIDGET_PRECISION_MAX       8    // Maximum supported precision

// Display format constants
#define NOTIONALWIDGET_FORMAT_STANDARD     0    // Standard number format
#define NOTIONALWIDGET_FORMAT_THOUSANDS    1    // With thousands separator
#define NOTIONALWIDGET_FORMAT_SCIENTIFIC   2    // Scientific notation for large values
#define NOTIONALWIDGET_FORMAT_ABBREVIATED  3    // 1.5M, 2.3B etc.

// Validation constants
#define NOTIONALWIDGET_MIN_VALUE           0.0
#define NOTIONALWIDGET_MAX_VALUE           1e15   // 1 quadrillion
#define NOTIONALWIDGET_DEFAULT_MIN         0.01
#define NOTIONALWIDGET_DEFAULT_MAX         1e12   // 1 trillion default max

// Widget state constants
#define NOTIONALWIDGET_STATE_NORMAL        0
#define NOTIONALWIDGET_STATE_READONLY      1
#define NOTIONALWIDGET_STATE_DISABLED      2
#define NOTIONALWIDGET_STATE_HIDDEN        3
#define NOTIONALWIDGET_STATE_ERROR         4
#define NOTIONALWIDGET_STATE_COMPUTED      5    // Value computed, not editable

// Currency codes - most common
#define NOTIONALWIDGET_CURRENCY_USD        "USD"
#define NOTIONALWIDGET_CURRENCY_EUR        "EUR"
#define NOTIONALWIDGET_CURRENCY_GBP        "GBP"
#define NOTIONALWIDGET_CURRENCY_JPY        "JPY"
#define NOTIONALWIDGET_CURRENCY_CHF        "CHF"
#define NOTIONALWIDGET_CURRENCY_DEFAULT    NOTIONALWIDGET_CURRENCY_USD

// Widget state enum
enum class NotionalWidgetState {
    Normal = NOTIONALWIDGET_STATE_NORMAL,
    ReadOnly = NOTIONALWIDGET_STATE_READONLY,
    Disabled = NOTIONALWIDGET_STATE_DISABLED,
    Hidden = NOTIONALWIDGET_STATE_HIDDEN,
    Error = NOTIONALWIDGET_STATE_ERROR,
    Computed = NOTIONALWIDGET_STATE_COMPUTED
};

// Display format enum
enum class NotionalDisplayFormat {
    Standard = NOTIONALWIDGET_FORMAT_STANDARD,
    WithThousands = NOTIONALWIDGET_FORMAT_THOUSANDS,
    Scientific = NOTIONALWIDGET_FORMAT_SCIENTIFIC,
    Abbreviated = NOTIONALWIDGET_FORMAT_ABBREVIATED
};

// Widget showing the Notional amount input.
// Only visible for certain product types.
//
// TODO: refactor to use Decimal class instead of double - floating point issues
// FIXME: precision handling for JPY is wrong - see BUG-3421
class NotionalWidget {
public:
    NotionalWidget();
    ~NotionalWidget();
    
    // Initialization
    void Initialize();
    void Reset();
    
    // Product change handler
    void onProductChanged(ProductType eProduct);
    
    // Value accessors - Hungarian notation
    double value() const;
    void setValue(double dValue);
    
    // Formatted value for display
    std::string getFormattedValue() const;
    void setFormattedValue(const std::string& szValue);
    
    // Visibility
    bool isVisible() const;
    void setVisible(bool bVisible);
    
    // Widget state
    NotionalWidgetState getState() const;
    void setState(NotionalWidgetState eState);
    
    // Configuration
    int getDecimalPlaces() const { return m_nDecimalPlaces; }
    void setDecimalPlaces(int nPlaces);
    
    double getMinValue() const { return m_dMinValue; }
    void setMinValue(double dMin);
    
    double getMaxValue() const { return m_dMaxValue; }
    void setMaxValue(double dMax);
    
    const char* getCurrencyCode() const { return m_szCurrencyCode; }
    void setCurrencyCode(const char* szCode);
    
    // Display format
    NotionalDisplayFormat getDisplayFormat() const { return m_eDisplayFormat; }
    void setDisplayFormat(NotionalDisplayFormat eFormat);
    
    // Validation
    bool isValid() const;
    int validate();
    std::string getValidationMessage() const;
    
    // Convenience methods
    void setRange(double dMin, double dMax);
    void resetToDefaults();

private:
    // Member variables - Hungarian notation
    double      m_dNotionalAmount;        // The actual notional value
    bool        m_bIsVisible;             // Is widget visible?
    bool        m_bIsEnabled;             // Is widget enabled?
    
    // Configuration - added in v2.0
    int         m_nDecimalPlaces;         // Number of decimal places
    double      m_dMinValue;              // Minimum allowed value
    double      m_dMaxValue;              // Maximum allowed value
    char        m_szCurrencyCode[4];      // Currency code (USD, EUR, etc.)
    
    // State tracking
    NotionalWidgetState m_eWidgetState;
    NotionalDisplayFormat m_eDisplayFormat;
    
    // Validation
    int         m_nLastValidationResult;
    std::string m_szValidationMessage;
    
    // Product tracking
    ProductType m_eCurrentProduct;
    bool        m_bProductInitialized;
    
    // Internal helpers
    void applyProductDefaults(ProductType eProduct);
    void updateWidgetState();
    std::string formatValue(double dValue) const;
    double parseValue(const std::string& szValue) const;
    void clampValueToRange();
};
