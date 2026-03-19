#include "NotionalWidget.h"
#include "util/CMemoryUtils.h"

// ============================================================================
// NotionalWidget.cpp - Notional Amount Input Implementation
// Created: 2003-10-05 by J. Henderson
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// NOTE: Notional precision handling is tricky. Different currencies have
// different precision requirements:
// - USD/EUR/GBP: 2 decimal places
// - JPY: 0 decimal places (but we use 6 for consistency with legacy data)
// - Cryptocurrencies: 8 decimal places (not yet supported)
//
// FIXME: Using double for currency is problematic - should use Decimal
// See: BUG-3421, BUG-4522
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <iomanip>

NotionalWidget::NotionalWidget()
    : m_dNotionalAmount(0.0)
    , m_bIsVisible(true)
    , m_bIsEnabled(true)
    , m_nDecimalPlaces(NOTIONALWIDGET_PRECISION_DEFAULT)
    , m_dMinValue(NOTIONALWIDGET_DEFAULT_MIN)
    , m_dMaxValue(NOTIONALWIDGET_DEFAULT_MAX)
    , m_eWidgetState(NotionalWidgetState::Normal)
    , m_eDisplayFormat(NotionalDisplayFormat::WithThousands)
    , m_nLastValidationResult(0)
    , m_eCurrentProduct(ProductType::None)
    , m_bProductInitialized(false)
{
    // Initialize currency code to default
    strncpy(m_szCurrencyCode, NOTIONALWIDGET_CURRENCY_DEFAULT, sizeof(m_szCurrencyCode) - 1);
    m_szCurrencyCode[sizeof(m_szCurrencyCode) - 1] = '\0';
}

NotionalWidget::~NotionalWidget()
{
    // Nothing to clean up
}

void NotionalWidget::Initialize()
{
    // Full initialization - added in v2.0
    m_dNotionalAmount = 0.0;
    m_bIsVisible = true;
    m_bIsEnabled = true;
    m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_DEFAULT;
    m_dMinValue = NOTIONALWIDGET_DEFAULT_MIN;
    m_dMaxValue = NOTIONALWIDGET_DEFAULT_MAX;
    m_eWidgetState = NotionalWidgetState::Normal;
    m_eDisplayFormat = NotionalDisplayFormat::WithThousands;
    m_nLastValidationResult = 0;
    m_szValidationMessage.clear();
    
    strncpy(m_szCurrencyCode, NOTIONALWIDGET_CURRENCY_DEFAULT, sizeof(m_szCurrencyCode) - 1);
    m_szCurrencyCode[sizeof(m_szCurrencyCode) - 1] = '\0';
}

void NotionalWidget::Reset()
{
    // Reset to default state
    m_dNotionalAmount = 0.0;
    m_bIsVisible = true;
    m_bIsEnabled = true;
    m_eWidgetState = NotionalWidgetState::Normal;
    m_nLastValidationResult = 0;
    m_szValidationMessage.clear();
}

void NotionalWidget::onProductChanged(ProductType eProduct)
{
    // Store current product
    m_eCurrentProduct = eProduct;
    m_bProductInitialized = true;
    
    // Visibility is controlled by TradeForm::updateFieldVisibility.
    // Here we just reset the value when the product changes to avoid
    // stale data from a previous product type being submitted.
    
    // Reset value to zero - product change means new trade
    // TODO: prompt user to confirm if there's unsaved data
    m_dNotionalAmount = 0.0;
    
    // Apply product-specific defaults
    applyProductDefaults(eProduct);
    
    // Update widget state
    updateWidgetState();
}

void NotionalWidget::applyProductDefaults(ProductType eProduct)
{
    // Apply product-specific configuration
    // Added in v2.1.3 - DO NOT REMOVE - breaks backward compat
    
    switch (eProduct) {
        case ProductType::Equity:
            // Equity: standard notional, USD default
            m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_DEFAULT;
            m_dMinValue = 0.01;
            m_dMaxValue = 1e12;  // 1 trillion
            strncpy(m_szCurrencyCode, "USD", sizeof(m_szCurrencyCode) - 1);
            break;
            
        case ProductType::FixedIncome:
            // Fixed Income: bond notionals, higher precision
            m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_DEFAULT;
            m_dMinValue = 1000.0;  // Minimum bond notional
            m_dMaxValue = 1e12;
            break;
            
        case ProductType::Derivative:
        case ProductType::Swap:
        case ProductType::Forward:
        case ProductType::Option:
        case ProductType::Future:
            // Derivatives: high-value, various currencies
            m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_DEFAULT;
            m_dMinValue = 1.0;
            m_dMaxValue = 1e15;  // Higher limit for derivatives
            break;
            
        case ProductType::FX:
            // FX: computed from currency pair, field hidden
            // Values here are just for internal use
            m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_HIGH;
            m_eWidgetState = NotionalWidgetState::Computed;
            break;
            
        case ProductType::CashFlow:
            // Cash Flow: computed from leg schedule
            m_eWidgetState = NotionalWidgetState::Computed;
            break;
            
        // Deprecated products
        case ProductType::MBS:
        case ProductType::CMO:
        case ProductType::CDO:
        case ProductType::ABS:
        case ProductType::CMBS:
        case ProductType::RMBS:
        case ProductType::CLO:
        case ProductType::ABCP:
        case ProductType::SIV:
            // Structured products: notional from underlying pool
            // TODO: refactor this - legacy code from 2008
            m_eWidgetState = NotionalWidgetState::Computed;
            m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_HIGH;
            break;
            
        case ProductType::Repo:
        case ProductType::SecLoan:
            // Securities lending: matches underlying security value
            m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_DEFAULT;
            m_dMinValue = 10000.0;  // Minimum for repo
            break;
            
        default:
            // Unknown product: use defaults
            break;
    }
    
    m_szCurrencyCode[sizeof(m_szCurrencyCode) - 1] = '\0';
}

void NotionalWidget::updateWidgetState()
{
    // Update widget state based on flags
    // Added in v2.0
    
    if (!m_bIsVisible) {
        m_eWidgetState = NotionalWidgetState::Hidden;
    } else if (!m_bIsEnabled) {
        m_eWidgetState = NotionalWidgetState::Disabled;
    } else if (m_nLastValidationResult != 0) {
        m_eWidgetState = NotionalWidgetState::Error;
    }
    // Don't override Computed state
}

double NotionalWidget::value() const
{
    return m_dNotionalAmount;
}

void NotionalWidget::setValue(double dValue)
{
    if (!m_bIsVisible) {
        return;  // Ignore if hidden
    }
    
    if (m_eWidgetState == NotionalWidgetState::Computed) {
        // Computed values can be set programmatically
        m_dNotionalAmount = dValue;
        return;
    }
    
    if (!m_bIsEnabled) {
        return;  // Ignore if disabled
    }
    
    // Store value
    m_dNotionalAmount = dValue;
    
    // Clamp to range
    clampValueToRange();
    
    // Revalidate
    validate();
}

void NotionalWidget::clampValueToRange()
{
    // Clamp value to allowed range
    // Added in v2.2 for safety
    
    if (m_dNotionalAmount < m_dMinValue && m_dNotionalAmount != 0.0) {
        // Don't clamp zero - it's a valid "not set" value
        // FIXME: this is wrong but changing it breaks existing trades
        // m_dNotionalAmount = m_dMinValue;
    }
    
    if (m_dNotionalAmount > m_dMaxValue) {
        m_dNotionalAmount = m_dMaxValue;
    }
}

std::string NotionalWidget::getFormattedValue() const
{
    return formatValue(m_dNotionalAmount);
}

void NotionalWidget::setFormattedValue(const std::string& szValue)
{
    double dValue = parseValue(szValue);
    setValue(dValue);
}

std::string NotionalWidget::formatValue(double dValue) const
{
    // Format value for display
    // TODO: implement proper localization
    
    if (dValue == 0.0) {
        return "";
    }
    
    std::ostringstream oss;
    
    switch (m_eDisplayFormat) {
        case NotionalDisplayFormat::Scientific:
            oss << std::scientific << std::setprecision(m_nDecimalPlaces) << dValue;
            break;
            
        case NotionalDisplayFormat::Abbreviated:
            // Format as 1.5M, 2.3B, etc.
            if (dValue >= 1e9) {
                oss << std::fixed << std::setprecision(1) << (dValue / 1e9) << "B";
            } else if (dValue >= 1e6) {
                oss << std::fixed << std::setprecision(1) << (dValue / 1e6) << "M";
            } else if (dValue >= 1e3) {
                oss << std::fixed << std::setprecision(1) << (dValue / 1e3) << "K";
            } else {
                oss << std::fixed << std::setprecision(m_nDecimalPlaces) << dValue;
            }
            break;
            
        case NotionalDisplayFormat::WithThousands:
            // Standard format with thousands separator
            // TODO: implement proper thousands separator
            oss << std::fixed << std::setprecision(m_nDecimalPlaces) << dValue;
            break;
            
        case NotionalDisplayFormat::Standard:
        default:
            oss << std::fixed << std::setprecision(m_nDecimalPlaces) << dValue;
            break;
    }
    
    return oss.str();
}

double NotionalWidget::parseValue(const std::string& szValue) const
{
    // Parse string to double
    // Handle K/M/B suffixes
    
    if (szValue.empty()) {
        return 0.0;
    }
    
    // Make copy for parsing
    std::string szTrimmed = szValue;
    
    // Trim whitespace
    size_t start = szTrimmed.find_first_not_of(" \t\n\r");
    size_t end = szTrimmed.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        szTrimmed = szTrimmed.substr(start, end - start + 1);
    }
    
    // Check for suffixes
    double dMultiplier = 1.0;
    if (!szTrimmed.empty()) {
        char cLast = szTrimmed.back();
        if (cLast == 'K' || cLast == 'k') {
            dMultiplier = 1e3;
            szTrimmed.pop_back();
        } else if (cLast == 'M' || cLast == 'm') {
            dMultiplier = 1e6;
            szTrimmed.pop_back();
        } else if (cLast == 'B' || cLast == 'b') {
            dMultiplier = 1e9;
            szTrimmed.pop_back();
        }
    }
    
    // Parse as double
    double dValue = std::atof(szTrimmed.c_str());
    return dValue * dMultiplier;
}

bool NotionalWidget::isVisible() const
{
    return m_bIsVisible;
}

void NotionalWidget::setVisible(bool bVisible)
{
    m_bIsVisible = bVisible;
    updateWidgetState();
}

NotionalWidgetState NotionalWidget::getState() const
{
    return m_eWidgetState;
}

void NotionalWidget::setState(NotionalWidgetState eState)
{
    m_eWidgetState = eState;
    
    // Update flags to match state
    switch (eState) {
        case NotionalWidgetState::Normal:
            m_bIsVisible = true;
            m_bIsEnabled = true;
            break;
        case NotionalWidgetState::ReadOnly:
        case NotionalWidgetState::Computed:
            m_bIsVisible = true;
            m_bIsEnabled = false;
            break;
        case NotionalWidgetState::Disabled:
            m_bIsVisible = true;
            m_bIsEnabled = false;
            break;
        case NotionalWidgetState::Hidden:
            m_bIsVisible = false;
            break;
        default:
            break;
    }
}

void NotionalWidget::setDecimalPlaces(int nPlaces)
{
    m_nDecimalPlaces = std::max(0, std::min(nPlaces, NOTIONALWIDGET_PRECISION_MAX));
}

void NotionalWidget::setMinValue(double dMin)
{
    m_dMinValue = dMin;
    clampValueToRange();
}

void NotionalWidget::setMaxValue(double dMax)
{
    m_dMaxValue = dMax;
    clampValueToRange();
}

void NotionalWidget::setCurrencyCode(const char* szCode)
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    
    // Null check for currency code pointer
    if (!szCode) {
        return;
    }
    
    if (strlen(szCode) == 3) {
        strncpy(m_szCurrencyCode, szCode, sizeof(m_szCurrencyCode) - 1);
        m_szCurrencyCode[sizeof(m_szCurrencyCode) - 1] = '\0';
        
        // Adjust precision for JPY
        // FIXME: this is wrong but changing it breaks existing trades
        // JPY should have 0 decimal places but legacy data uses 6
        if (strcmp(szCode, "JPY") == 0) {
            m_nDecimalPlaces = 0;  // Fixed in v2.5
        }
    }
}

void NotionalWidget::setDisplayFormat(NotionalDisplayFormat eFormat)
{
    m_eDisplayFormat = eFormat;
}

void NotionalWidget::setRange(double dMin, double dMax)
{
    m_dMinValue = dMin;
    m_dMaxValue = dMax;
    clampValueToRange();
}

void NotionalWidget::resetToDefaults()
{
    m_nDecimalPlaces = NOTIONALWIDGET_PRECISION_DEFAULT;
    m_dMinValue = NOTIONALWIDGET_DEFAULT_MIN;
    m_dMaxValue = NOTIONALWIDGET_DEFAULT_MAX;
    strncpy(m_szCurrencyCode, NOTIONALWIDGET_CURRENCY_DEFAULT, sizeof(m_szCurrencyCode) - 1);
    m_szCurrencyCode[sizeof(m_szCurrencyCode) - 1] = '\0';
    m_eDisplayFormat = NotionalDisplayFormat::WithThousands;
}

int NotionalWidget::validate()
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    m_nLastValidationResult = 0;
    m_szValidationMessage.clear();
    
    // Zero is valid (means not set yet)
    if (m_dNotionalAmount == 0.0) {
        return 0;
    }
    
    // Check minimum
    if (m_dNotionalAmount < m_dMinValue) {
        m_nLastValidationResult = 1;
        m_szValidationMessage = "Notional is below minimum value";
        updateWidgetState();
        return m_nLastValidationResult;
    }
    
    // Check maximum
    if (m_dNotionalAmount > m_dMaxValue) {
        m_nLastValidationResult = 2;
        m_szValidationMessage = "Notional exceeds maximum value";
        updateWidgetState();
        return m_nLastValidationResult;
    }
    
    // Check for NaN/Inf
    if (std::isnan(m_dNotionalAmount) || std::isinf(m_dNotionalAmount)) {
        m_nLastValidationResult = 3;
        m_szValidationMessage = "Notional value is invalid";
        updateWidgetState();
        return m_nLastValidationResult;
    }
    
    updateWidgetState();
    return 0;
}

bool NotionalWidget::isValid() const
{
    return m_nLastValidationResult == 0;
}

std::string NotionalWidget::getValidationMessage() const
{
    return m_szValidationMessage;
}
