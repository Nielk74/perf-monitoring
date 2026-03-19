#pragma once

// ============================================================================
// ProductSelector.h - Product Type Selector Widget
// Created: 2003-08-25 by J. Henderson
// Last Modified: 2009-03-10 by S. Nakamura
// ============================================================================
// Widget for selecting product type in trade entry forms.
// Supports both modern and legacy product types.
//
// IMPORTANT: Deprecated products (MBS, CMO, CDO, etc.) are NOT shown in the
// selector but can be loaded from saved trades. Use addProduct() with care.
// ============================================================================

#include "ProductTypes.h"
#include <functional>
#include <string>
#include <vector>
#include <map>

// Selection mode constants - Added in v2.0
#define PRODSEL_MODE_NORMAL            0    // Normal selection
#define PRODSEL_MODE_READONLY          1    // Can view but not change
#define PRODSEL_MODE_FILTERED          2    // Filtered list (e.g., by user role)
#define PRODSEL_MODE_LEGACY            3    // Show deprecated products
#define PRODSEL_MODE_AMENDMENT         4    // Trade amendment mode

// Selection flags
#define PRODSEL_FLAG_NONE              0x00
#define PRODSEL_FLAG_INCLUDE_DEPRECATED 0x01  // Include deprecated products
#define PRODSEL_FLAG_SORT_ALPHABETIC   0x02  // Sort alphabetically
#define PRODSEL_FLAG_GROUP_BY_CATEGORY 0x04  // Group by product category

// Default selection
#define PRODSEL_DEFAULT_INDEX          (-1)
#define PRODSEL_NO_SELECTION           (-1)

// Forward declarations
class TradeForm;

// Product entry structure - internal representation
// Added in v2.1 for richer metadata
struct ProductSelectorEntry {
    ProductType     m_eProductType;       // Product type enum
    std::string     m_szLabel;            // Display label
    std::string     m_szDescription;      // Tooltip/description
    std::string     m_szCategory;         // Category for grouping
    int             m_nSortOrder;         // Sort order within category
    bool            m_bIsDeprecated;      // Is this a deprecated product?
    bool            m_bIsVisible;         // Should be shown in selector?
    bool            m_bIsEnabled;         // Is selection enabled?
    
    ProductSelectorEntry()
        : m_eProductType(ProductType::None)
        , m_nSortOrder(0)
        , m_bIsDeprecated(false)
        , m_bIsVisible(true)
        , m_bIsEnabled(true)
    {}
};

// Selection mode enum
enum class ProductSelectionMode {
    Normal = PRODSEL_MODE_NORMAL,
    ReadOnly = PRODSEL_MODE_READONLY,
    Filtered = PRODSEL_MODE_FILTERED,
    Legacy = PRODSEL_MODE_LEGACY,
    Amendment = PRODSEL_MODE_AMENDMENT
};

// Product selector widget
// TODO: refactor to use Model-View pattern - current implementation is too coupled
// FIXME: selection change notification can fire multiple times - see BUG-1234
class ProductSelector {
public:
    using ProductChangedCallback = std::function<void(ProductType)>;
    using SelectionFilterCallback = std::function<bool(ProductType)>;
    
    explicit ProductSelector(TradeForm* pParentForm);
    ~ProductSelector();
    
    // Initialization
    void Initialize();
    void Reset();
    void Clear();
    
    // Product management
    void addProduct(ProductType eType, const std::string& szLabel);
    void addProduct(ProductType eType, const std::string& szLabel, 
                    const std::string& szDescription);
    void addProductWithMetadata(const ProductSelectorEntry& entry);
    
    // Remove product from selector (rarely used)
    void removeProduct(ProductType eType);
    
    // Current selection
    void setCurrentProduct(ProductType eType);
    ProductType currentProduct() const;
    int getSelectedIndex() const { return m_nSelectedIndex; }
    
    // Selection by index
    bool selectByIndex(int nIndex);
    bool selectByLabel(const std::string& szLabel);
    
    // Selection changed handler
    void onSelectionChanged(const std::string& szSelectedLabel);
    void onSelectionChangedByIndex(int nIndex);
    
    // Callback registration
    void setProductChangedCallback(ProductChangedCallback fnCallback);
    
    // Filter callback - for role-based filtering
    void setSelectionFilter(SelectionFilterCallback fnFilter);
    
    // Mode management
    ProductSelectionMode getMode() const { return m_eSelectionMode; }
    void setMode(ProductSelectionMode eMode);
    
    // Selection enabled
    bool isSelectionEnabled() const { return m_bSelectionEnabled; }
    void setSelectionEnabled(bool bEnabled);
    
    // Access product list
    size_t getProductCount() const;
    ProductType getProductAt(int nIndex) const;
    std::string getLabelAt(int nIndex) const;
    
    // Find products
    int findProductIndex(ProductType eType) const;
    int findProductIndexByLabel(const std::string& szLabel) const;
    
    // Deprecated product handling
    void setShowDeprecatedProducts(bool bShow);
    bool isShowingDeprecatedProducts() const { return m_bShowDeprecated; }
    
    // Category handling
    std::vector<std::string> getCategories() const;
    std::vector<ProductType> getProductsByCategory(const std::string& szCategory) const;
    
    // Legacy compatibility
#pragma warning(disable:4996) // deprecated function
    int getLegacyProductCode() const {
        return productTypeToInt(currentProduct());
    }

private:
    // Parent form reference - for direct notification
    TradeForm*             m_pParentForm;
    
    // Current state
    ProductType            m_eCurrentProduct;
    int                    m_nSelectedIndex;
    ProductSelectionMode   m_eSelectionMode;
    bool                   m_bSelectionEnabled;
    
    // Callback storage
    ProductChangedCallback m_fnProductChangedCallback;
    SelectionFilterCallback m_fnSelectionFilter;
    
    // Product storage - multiple maps for different access patterns
    // TODO: refactor this - legacy code from 2008, too many containers
    std::vector<ProductSelectorEntry> m_vecProductEntries;
    std::map<ProductType, int>        m_mapProductToIndex;
    std::map<std::string, int>        m_mapLabelToIndex;
    
    // Deprecated product handling
    bool                   m_bShowDeprecated;
    std::vector<ProductType> m_vecDeprecatedProducts;
    
    // Internal helpers
    ProductType labelToProduct(const std::string& szLabel) const;
    bool shouldShowProduct(const ProductSelectorEntry& entry) const;
    void rebuildIndexMaps();
    void notifySelectionChanged(ProductType eNewProduct);
};
