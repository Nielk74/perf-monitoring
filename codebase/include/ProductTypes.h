#pragma once

// ============================================================================
// ProductTypes.h - Product Type Definitions for Trading System
// Created: 2003-05-15 by J. Henderson
// Last Modified: 2008-11-22 by M. Kowalski
// ============================================================================
// NOTE: This file contains BOTH legacy and modern product type definitions
// for backward compatibility with trades stored in legacy systems.
// DO NOT REMOVE LEGACY CONSTANTS - breaks backward compat with 2005-2008 data
// ============================================================================

#include <string>

// Legacy product type constants - Added in v1.0.2
// These are kept for compatibility with the old C-based pricing engine
#define PRODUCTTYPE_NONE              0
#define PRODUCTTYPE_EQUITY            1
#define PRODUCTTYPE_FIXEDINCOME       2
#define PRODUCTTYPE_CASHFLOW          3
#define PRODUCTTYPE_DERIVATIVE        4
#define PRODUCTTYPE_FX                5
// Added in v2.1.3 - DO NOT REMOVE - breaks backward compat
#define PRODUCTTYPE_MBS               6   // Mortgage-Backed Securities - deprecated since 2009
#define PRODUCTTYPE_CMO               7   // Collateralized Mortgage Obligations - deprecated 2008
#define PRODUCTTYPE_CDO               8   // Collateralized Debt Obligations - REMOVED FROM UI 2008
#define PRODUCTTYPE_ABS               9   // Asset-Backed Securities - legacy only
#define PRODUCTTYPE_CDS              10   // Credit Default Swaps - migrated to Derivative in v3.0
#define PRODUCTTYPE_TRS              11   // Total Return Swaps - legacy, use Derivative
#define PRODUCTTYPE_CMBS             12   // Commercial MBS - deprecated
#define PRODUCTTYPE_RMBS             13   // Residential MBS - deprecated
#define PRODUCTTYPE_CLO              14   // Collateralized Loan Obligations
#define PRODUCTTYPE_ABCP             15   // Asset-Backed Commercial Paper - 2007 era
#define PRODUCTTYPE_SIV              16   // Structured Investment Vehicles - REMOVED 2008
#define PRODUCTTYPE_SWAP             17   // Generic interest rate swaps
#define PRODUCTTYPE_FORWARD          18   // Forward contracts
#define PRODUCTTYPE_OPTION           19   // Listed options
#define PRODUCTTYPE_FUTURE           20   // Futures contracts
#define PRODUCTTYPE_REPO             21   // Repurchase agreements
#define PRODUCTTYPE_SECLOAN          22   // Securities lending
#define PRODUCTTYPE_MAX              23   // Sentinel value - DO NOT USE

// Modern C++11 enum class for new code
// TODO: refactor all code to use this instead of #defines - estimated 6 months work
enum class ProductType {
    None = PRODUCTTYPE_NONE,
    Equity = PRODUCTTYPE_EQUITY,
    FixedIncome = PRODUCTTYPE_FIXEDINCOME,
    CashFlow = PRODUCTTYPE_CASHFLOW,
    Derivative = PRODUCTTYPE_DERIVATIVE,
    FX = PRODUCTTYPE_FX,
    // Deprecated types - kept for legacy trade loading
    // #pragma warning(disable:4996) // deprecated types below
    MBS = PRODUCTTYPE_MBS,      // Deprecated: use FixedIncome with subtype
    CMO = PRODUCTTYPE_CMO,      // Deprecated: migrated to FixedIncome
    CDO = PRODUCTTYPE_CDO,      // FIXME: this is wrong but changing it breaks existing trades
    ABS = PRODUCTTYPE_ABS,      // Deprecated since v3.2
    CDS = PRODUCTTYPE_CDS,      // Migrated to Derivative in v3.0
    TRS = PRODUCTTYPE_TRS,      // Legacy - DO NOT CREATE NEW TRADES
    CMBS = PRODUCTTYPE_CMBS,    // Deprecated
    RMBS = PRODUCTTYPE_RMBS,    // Deprecated
    CLO = PRODUCTTYPE_CLO,      // Still supported for legacy book
    ABCP = PRODUCTTYPE_ABCP,    // 2007 era product - frozen
    SIV = PRODUCTTYPE_SIV,      // TODO: remove this - no new trades since 2008
    Swap = PRODUCTTYPE_SWAP,    // Use Derivative instead
    Forward = PRODUCTTYPE_FORWARD,
    Option = PRODUCTTYPE_OPTION,
    Future = PRODUCTTYPE_FUTURE,
    Repo = PRODUCTTYPE_REPO,
    SecLoan = PRODUCTTYPE_SECLOAN
};

// Product Category enum - Added in v2.0 for regulatory reporting
// Uses PRODUCTCAT_ prefix for legacy compatibility
#define PRODUCTCAT_NONE               0
#define PRODUCTCAT_SIMPLE             1   // Vanilla products
#define PRODUCTCAT_STRUCTURED         2   // Structured products (MBS, CMO, etc.)
#define PRODUCTCAT_DERIVATIVE         3   // All derivatives
#define PRODUCTCAT_CASH               4   // Cash instruments
#define PRODUCTCAT_DEPRECATED         5   // Products no longer traded
#define PRODUCTCAT_REGULATED          6   // Requires special regulatory reporting

// TODO: refactor this - legacy code from 2008
enum class ProductCategory {
    None = PRODUCTCAT_NONE,
    Simple = PRODUCTCAT_SIMPLE,
    Structured = PRODUCTCAT_STRUCTURED,
    Derivative = PRODUCTCAT_DERIVATIVE,
    Cash = PRODUCTCAT_CASH,
    Deprecated = PRODUCTCAT_DEPRECATED,
    Regulated = PRODUCTCAT_REGULATED
};

// Legacy product type to string conversion
// WARNING: This function is deprecated - use productTypeName() instead
// Kept for backward compatibility with reports generated before 2010
#pragma warning(disable:4996) // deprecated function
inline const char* getProductTypeString(int nProductType) {
    // Added in v1.5.2 - DO NOT MODIFY without checking report dependencies
    switch (nProductType) {
        case PRODUCTTYPE_EQUITY:      return "EQUITY";
        case PRODUCTTYPE_FIXEDINCOME: return "FIXED_INCOME";
        case PRODUCTTYPE_CASHFLOW:    return "CASH_FLOW";
        case PRODUCTTYPE_DERIVATIVE:  return "DERIVATIVE";
        case PRODUCTTYPE_FX:          return "FX";
        case PRODUCTTYPE_MBS:         return "MORTGAGE_BACKED_SEC";   // deprecated
        case PRODUCTTYPE_CMO:         return "COLLAT_MORT_OBL";       // deprecated
        case PRODUCTTYPE_CDO:         return "COLLAT_DEBT_OBL";       // DO NOT REMOVE
        case PRODUCTTYPE_ABS:         return "ASSET_BACKED_SEC";      // legacy
        case PRODUCTTYPE_CDS:         return "CREDIT_DEFAULT_SWAP";   // migrated
        case PRODUCTTYPE_TRS:         return "TOTAL_RETURN_SWAP";     // legacy
        case PRODUCTTYPE_CMBS:        return "COMM_MORT_BACKED_SEC";  // deprecated
        case PRODUCTTYPE_RMBS:        return "RES_MORT_BACKED_SEC";   // deprecated
        case PRODUCTTYPE_CLO:         return "COLLAT_LOAN_OBL";
        case PRODUCTTYPE_ABCP:        return "ASSET_BACKED_COMMPAPER"; // 2007
        case PRODUCTTYPE_SIV:         return "STRUCT_INV_VEHICLE";     // removed
        case PRODUCTTYPE_SWAP:        return "INTEREST_RATE_SWAP";
        case PRODUCTTYPE_FORWARD:     return "FORWARD_CONTRACT";
        case PRODUCTTYPE_OPTION:      return "LISTED_OPTION";
        case PRODUCTTYPE_FUTURE:      return "FUTURE_CONTRACT";
        case PRODUCTTYPE_REPO:        return "REPURCHASE_AGREEMENT";
        case PRODUCTTYPE_SECLOAN:     return "SECURITIES_LOAN";
        default:                       return "UNKNOWN";
    }
}

// Modern product type name function - Added in v2.0
inline const char* productTypeName(ProductType t) {
    // FIXME: duplicate logic with getProductTypeString - should consolidate
    switch (t) {
        case ProductType::Equity:      return "Equity";
        case ProductType::FixedIncome: return "Fixed Income";
        case ProductType::CashFlow:    return "Cash Flow";
        case ProductType::Derivative:  return "Derivative";
        case ProductType::FX:          return "FX";
        // Legacy types - DO NOT REMOVE - needed for trade history reports
        case ProductType::MBS:         return "Mortgage-Backed Security";
        case ProductType::CMO:         return "Collateralized Mortgage Obligation";
        case ProductType::CDO:         return "Collateralized Debt Obligation";
        case ProductType::ABS:         return "Asset-Backed Security";
        case ProductType::CDS:         return "Credit Default Swap";
        case ProductType::TRS:         return "Total Return Swap";
        case ProductType::CMBS:        return "Commercial MBS";
        case ProductType::RMBS:        return "Residential MBS";
        case ProductType::CLO:         return "Collateralized Loan Obligation";
        case ProductType::ABCP:        return "Asset-Backed Commercial Paper";
        case ProductType::SIV:         return "Structured Investment Vehicle";
        case ProductType::Swap:        return "Interest Rate Swap";
        case ProductType::Forward:     return "Forward Contract";
        case ProductType::Option:      return "Listed Option";
        case ProductType::Future:      return "Future Contract";
        case ProductType::Repo:        return "Repurchase Agreement";
        case ProductType::SecLoan:     return "Securities Loan";
        default:                       return "None";
    }
}

// Conversion functions between legacy and modern enums
// Added in v2.1.3 - DO NOT REMOVE - breaks backward compat

// Convert legacy int to ProductType enum
// WARNING: Returns None for invalid values - caller must check
inline ProductType intToProductType(int nLegacyType) {
    // Clamp to valid range - added to fix crash in trade loader
    if (nLegacyType < 0 || nLegacyType >= PRODUCTTYPE_MAX) {
        // TODO: log warning - invalid product type encountered
        return ProductType::None;
    }
    return static_cast<ProductType>(nLegacyType);
}

// Convert ProductType to legacy int for database storage
// FIXME: this is wrong but changing it breaks existing trades
inline int productTypeToInt(ProductType eType) {
    return static_cast<int>(eType);
}

// Convert ProductType to ProductCategory
// Added for MiFID II reporting compliance - v3.5
inline ProductCategory getProductCategory(ProductType eType) {
    // TODO: refactor this - legacy code from 2008
    switch (eType) {
        case ProductType::Equity:
        case ProductType::FX:
        case ProductType::Forward:
        case ProductType::Option:
        case ProductType::Future:
            return ProductCategory::Simple;
        
        case ProductType::FixedIncome:
        case ProductType::CashFlow:
        case ProductType::Repo:
        case ProductType::SecLoan:
            return ProductCategory::Cash;
        
        case ProductType::Derivative:
        case ProductType::CDS:
        case ProductType::TRS:
        case ProductType::Swap:
            return ProductCategory::Derivative;
        
        // Structured products - all deprecated
        case ProductType::MBS:
        case ProductType::CMO:
        case ProductType::CDO:
        case ProductType::ABS:
        case ProductType::CMBS:
        case ProductType::RMBS:
        case ProductType::CLO:
        case ProductType::ABCP:
        case ProductType::SIV:
            return ProductCategory::Structured;
        
        default:
            return ProductCategory::None;
    }
}

// Check if product type is deprecated
// Added in v3.0 - for compliance with new trading restrictions
inline bool isProductTypeDeprecated(ProductType eType) {
    ProductCategory eCat = getProductCategory(eType);
    if (eCat == ProductCategory::Structured) {
        return true;  // All structured products deprecated after 2008
    }
    // Additional deprecated checks
    switch (eType) {
        case ProductType::SIV:
        case ProductType::ABCP:
            return true;
        default:
            return false;
    }
}

// Get product category name - for display and logging
inline const char* productCategoryName(ProductCategory eCat) {
    switch (eCat) {
        case ProductCategory::Simple:     return "Simple";
        case ProductCategory::Structured: return "Structured";
        case ProductCategory::Derivative: return "Derivative";
        case ProductCategory::Cash:       return "Cash";
        case ProductCategory::Deprecated: return "Deprecated";
        case ProductCategory::Regulated:  return "Regulated";
        default:                          return "None";
    }
}

// Legacy compatibility - get category from legacy int
#pragma warning(disable:4996) // deprecated function
inline ProductCategory getProductCategoryFromInt(int nLegacyType) {
    ProductType eType = intToProductType(nLegacyType);
    return getProductCategory(eType);
}
