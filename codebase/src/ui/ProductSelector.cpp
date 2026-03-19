#include "ProductSelector.h"
#include "TradeForm.h"
#include "util/CMemoryUtils.h"

// ============================================================================
// ProductSelector.cpp - Product Type Selector Implementation
// Created: 2003-08-25 by J. Henderson
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// NOTE: The selector maintains multiple data structures for performance:
// - m_vecProductEntries: ordered list for display
// - m_mapProductToIndex: fast lookup by product type
// - m_mapLabelToIndex: fast lookup by label
//
// This was added in 2008 for performance but is now overkill.
// TODO: refactor to use single container with better indexing
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <algorithm>

// Static list of deprecated products
// Added in v2.1.3 - DO NOT REMOVE - breaks backward compat
static const ProductType s_aDeprecatedProducts[] = {
    ProductType::MBS,
    ProductType::CMO,
    ProductType::CDO,
    ProductType::ABS,
    ProductType::CDS,
    ProductType::TRS,
    ProductType::CMBS,
    ProductType::RMBS,
    ProductType::CLO,
    ProductType::ABCP,
    ProductType::SIV
};
static const int s_nDeprecatedProductCount = sizeof(s_aDeprecatedProducts) / sizeof(s_aDeprecatedProducts[0]);

// Helper to check if product is deprecated
static bool isProductDeprecated(ProductType eType)
{
    for (int i = 0; i < s_nDeprecatedProductCount; ++i) {
        if (s_aDeprecatedProducts[i] == eType) {
            return true;
        }
    }
    return false;
}

ProductSelector::ProductSelector(TradeForm* pParentForm)
    : m_pParentForm(pParentForm)
    , m_eCurrentProduct(ProductType::None)
    , m_nSelectedIndex(PRODSEL_NO_SELECTION)
    , m_eSelectionMode(ProductSelectionMode::Normal)
    , m_bSelectionEnabled(true)
    , m_bShowDeprecated(false)
{
    // Initialize deprecated products list
    for (int i = 0; i < s_nDeprecatedProductCount; ++i) {
        m_vecDeprecatedProducts.push_back(s_aDeprecatedProducts[i]);
    }
}

ProductSelector::~ProductSelector()
{
    // Clear all containers
    Clear();
}

void ProductSelector::Initialize()
{
    // Full initialization - added in v2.0
    m_eCurrentProduct = ProductType::None;
    m_nSelectedIndex = PRODSEL_NO_SELECTION;
    m_eSelectionMode = ProductSelectionMode::Normal;
    m_bSelectionEnabled = true;
    m_bShowDeprecated = false;
}

void ProductSelector::Reset()
{
    // Reset selection but keep product list
    m_eCurrentProduct = ProductType::None;
    m_nSelectedIndex = PRODSEL_NO_SELECTION;
}

void ProductSelector::Clear()
{
    // Clear all products
    m_vecProductEntries.clear();
    m_mapProductToIndex.clear();
    m_mapLabelToIndex.clear();
    m_eCurrentProduct = ProductType::None;
    m_nSelectedIndex = PRODSEL_NO_SELECTION;
}

void ProductSelector::addProduct(ProductType eType, const std::string& szLabel)
{
    addProduct(eType, szLabel, "");
}

void ProductSelector::addProduct(ProductType eType, const std::string& szLabel,
                                 const std::string& szDescription)
{
    ProductSelectorEntry entry;
    entry.m_eProductType = eType;
    entry.m_szLabel = szLabel;
    entry.m_szDescription = szDescription;
    entry.m_bIsDeprecated = isProductDeprecated(eType);
    entry.m_nSortOrder = static_cast<int>(m_vecProductEntries.size());
    
    // Set category based on product type
    // TODO: refactor this - legacy code from 2008
    ProductCategory eCat = getProductCategory(eType);
    entry.m_szCategory = productCategoryName(eCat);
    
    addProductWithMetadata(entry);
}

void ProductSelector::addProductWithMetadata(const ProductSelectorEntry& entry)
{
    // Check if already exists
    if (m_mapProductToIndex.find(entry.m_eProductType) != m_mapProductToIndex.end()) {
        // Product already in list - update it
        int nIndex = m_mapProductToIndex[entry.m_eProductType];
        m_vecProductEntries[nIndex] = entry;
        return;
    }
    
    // Add new entry
    int nIndex = static_cast<int>(m_vecProductEntries.size());
    m_vecProductEntries.push_back(entry);
    
    // Update index maps
    m_mapProductToIndex[entry.m_eProductType] = nIndex;
    m_mapLabelToIndex[entry.m_szLabel] = nIndex;
}

void ProductSelector::removeProduct(ProductType eType)
{
    // Find and remove product
    auto it = m_mapProductToIndex.find(eType);
    if (it == m_mapProductToIndex.end()) {
        return;  // Not found
    }
    
    int nIndex = it->second;
    
    // Remove from label map
    const std::string& szLabel = m_vecProductEntries[nIndex].m_szLabel;
    m_mapLabelToIndex.erase(szLabel);
    
    // Remove from product map
    m_mapProductToIndex.erase(eType);
    
    // Remove from vector
    m_vecProductEntries.erase(m_vecProductEntries.begin() + nIndex);
    
    // Rebuild index maps (indices have changed)
    rebuildIndexMaps();
    
    // Update selection if removed product was selected
    if (m_nSelectedIndex == nIndex) {
        m_nSelectedIndex = PRODSEL_NO_SELECTION;
        m_eCurrentProduct = ProductType::None;
    } else if (m_nSelectedIndex > nIndex) {
        m_nSelectedIndex--;
    }
}

void ProductSelector::rebuildIndexMaps()
{
    // Rebuild both index maps
    // Called after structural changes to product list
    // TODO: optimize this - O(n) rebuild is inefficient
    
    m_mapProductToIndex.clear();
    m_mapLabelToIndex.clear();
    
    for (int i = 0; i < static_cast<int>(m_vecProductEntries.size()); ++i) {
        const ProductSelectorEntry& entry = m_vecProductEntries[i];
        m_mapProductToIndex[entry.m_eProductType] = i;
        m_mapLabelToIndex[entry.m_szLabel] = i;
    }
}

void ProductSelector::setCurrentProduct(ProductType eType)
{
    // Find index for this product
    int nIndex = findProductIndex(eType);
    if (nIndex == PRODSEL_NO_SELECTION) {
        // Product not in list - might be deprecated
        m_eCurrentProduct = eType;
        m_nSelectedIndex = PRODSEL_NO_SELECTION;
        return;
    }
    
    m_eCurrentProduct = eType;
    m_nSelectedIndex = nIndex;
}

ProductType ProductSelector::currentProduct() const
{
    return m_eCurrentProduct;
}

bool ProductSelector::selectByIndex(int nIndex)
{
    if (nIndex < 0 || nIndex >= static_cast<int>(m_vecProductEntries.size())) {
        return false;
    }
    
    // Check if this product should be shown
    if (!shouldShowProduct(m_vecProductEntries[nIndex])) {
        return false;
    }
    
    ProductType eNewProduct = m_vecProductEntries[nIndex].m_eProductType;
    if (eNewProduct == m_eCurrentProduct) {
        return true;  // No change
    }
    
    m_eCurrentProduct = eNewProduct;
    m_nSelectedIndex = nIndex;
    
    notifySelectionChanged(eNewProduct);
    return true;
}

bool ProductSelector::selectByLabel(const std::string& szLabel)
{
    int nIndex = findProductIndexByLabel(szLabel);
    if (nIndex == PRODSEL_NO_SELECTION) {
        return false;
    }
    
    return selectByIndex(nIndex);
}

void ProductSelector::onSelectionChanged(const std::string& szSelectedLabel)
{
    // Main selection change handler - called from UI
    // Added in v1.0 - DO NOT CHANGE without testing
    
    ProductType eNewProduct = labelToProduct(szSelectedLabel);
    if (eNewProduct == m_eCurrentProduct) {
        return;  // No change
    }
    
    // Find index
    int nIndex = findProductIndexByLabel(szSelectedLabel);
    
    m_eCurrentProduct = eNewProduct;
    m_nSelectedIndex = nIndex;
    
    notifySelectionChanged(eNewProduct);
}

void ProductSelector::onSelectionChangedByIndex(int nIndex)
{
    // Alternative handler - selection by index
    // Added in v2.0 for keyboard navigation support
    
    selectByIndex(nIndex);
}

void ProductSelector::notifySelectionChanged(ProductType eNewProduct)
{
    // Notify parent form so it can update all dependent fields
    // This is the main integration point with TradeForm
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    
    // Null check for parent form with fallback behavior
    if (m_pParentForm) {
        m_pParentForm->onProductChanged(eNewProduct);
    }
    // If m_pParentForm is null, silently ignore - this allows standalone usage
    
    // Also fire the callback if one was registered
    // Used by unit tests and external observers
    // FIXME: callback can fire multiple times - see BUG-1234
    if (m_fnProductChangedCallback) {
        m_fnProductChangedCallback(eNewProduct);
    }
}

void ProductSelector::setProductChangedCallback(ProductChangedCallback fnCallback)
{
    m_fnProductChangedCallback = fnCallback;
}

void ProductSelector::setSelectionFilter(SelectionFilterCallback fnFilter)
{
    m_fnSelectionFilter = fnFilter;
}

void ProductSelector::setMode(ProductSelectionMode eMode)
{
    m_eSelectionMode = eMode;
    
    // Update selection enabled based on mode
    switch (eMode) {
        case ProductSelectionMode::ReadOnly:
        case ProductSelectionMode::Amendment:
            m_bSelectionEnabled = false;
            break;
        default:
            m_bSelectionEnabled = true;
            break;
    }
}

void ProductSelector::setSelectionEnabled(bool bEnabled)
{
    m_bSelectionEnabled = bEnabled;
}

size_t ProductSelector::getProductCount() const
{
    // Count visible products only
    size_t nCount = 0;
    for (const auto& entry : m_vecProductEntries) {
        if (shouldShowProduct(entry)) {
            ++nCount;
        }
    }
    return nCount;
}

ProductType ProductSelector::getProductAt(int nIndex) const
{
    if (nIndex < 0 || nIndex >= static_cast<int>(m_vecProductEntries.size())) {
        return ProductType::None;
    }
    return m_vecProductEntries[nIndex].m_eProductType;
}

std::string ProductSelector::getLabelAt(int nIndex) const
{
    if (nIndex < 0 || nIndex >= static_cast<int>(m_vecProductEntries.size())) {
        return "";
    }
    return m_vecProductEntries[nIndex].m_szLabel;
}

int ProductSelector::findProductIndex(ProductType eType) const
{
    auto it = m_mapProductToIndex.find(eType);
    if (it != m_mapProductToIndex.end()) {
        return it->second;
    }
    return PRODSEL_NO_SELECTION;
}

int ProductSelector::findProductIndexByLabel(const std::string& szLabel) const
{
    auto it = m_mapLabelToIndex.find(szLabel);
    if (it != m_mapLabelToIndex.end()) {
        return it->second;
    }
    return PRODSEL_NO_SELECTION;
}

void ProductSelector::setShowDeprecatedProducts(bool bShow)
{
    m_bShowDeprecated = bShow;
}

bool ProductSelector::shouldShowProduct(const ProductSelectorEntry& entry) const
{
    // Check visibility rules
    if (!entry.m_bIsVisible) {
        return false;
    }
    
    // Check deprecated filter
    if (entry.m_bIsDeprecated && !m_bShowDeprecated) {
        return false;
    }
    
    // Check custom filter
    if (m_fnSelectionFilter) {
        if (!m_fnSelectionFilter(entry.m_eProductType)) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> ProductSelector::getCategories() const
{
    std::vector<std::string> vecCategories;
    
    for (const auto& entry : m_vecProductEntries) {
        if (shouldShowProduct(entry)) {
            if (std::find(vecCategories.begin(), vecCategories.end(), 
                          entry.m_szCategory) == vecCategories.end()) {
                vecCategories.push_back(entry.m_szCategory);
            }
        }
    }
    
    // Sort alphabetically
    std::sort(vecCategories.begin(), vecCategories.end());
    
    return vecCategories;
}

std::vector<ProductType> ProductSelector::getProductsByCategory(const std::string& szCategory) const
{
    std::vector<ProductType> vecProducts;
    
    for (const auto& entry : m_vecProductEntries) {
        if (shouldShowProduct(entry) && entry.m_szCategory == szCategory) {
            vecProducts.push_back(entry.m_eProductType);
        }
    }
    
    return vecProducts;
}

ProductType ProductSelector::labelToProduct(const std::string& szLabel) const
{
    auto it = std::find_if(m_vecProductEntries.begin(), m_vecProductEntries.end(),
        [&szLabel](const ProductSelectorEntry& e) { 
            return e.m_szLabel == szLabel; 
        });

    if (it == m_vecProductEntries.end()) {
        return ProductType::None;
    }
    return it->m_eProductType;
}
