#pragma once

// ============================================================================
// CReferenceWrapper.h - Safe Reference Wrapper Template
// Created: 2010-01-20 by T. Modernizer
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// Template class for managing references safely.
// Provides null-safe reference access with validity tracking.
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <cassert>
#include <utility>
#include <type_traits>

// ============================================================================
// CRefWrapper - Safe reference wrapper template
// DEPRECATED: raw pointer access - use GetSharedPtr() instead
// ============================================================================
template<typename T>
class CRefWrapper {
public:
    using ValueType = T;
    using ReferenceType = T&;
    using ConstReferenceType = const T&;
    using PointerType = T*;
    using ConstPointerType = const T*;

    // Default constructor - creates invalid wrapper
    CRefWrapper()
        : m_pValue(nullptr)
        , m_bIsValid(false)
    {}
    
    // Constructor from reference
    explicit CRefWrapper(ReferenceType rValue)
        : m_pValue(&rValue)
        , m_bIsValid(true)
    {}
    
    // Constructor from pointer
    explicit CRefWrapper(PointerType pValue)
        : m_pValue(pValue)
        , m_bIsValid(pValue != nullptr)
    {}
    
    // Copy constructor
    CRefWrapper(const CRefWrapper& other)
        : m_pValue(other.m_pValue)
        , m_bIsValid(other.m_bIsValid)
    {}
    
    // Move constructor
    CRefWrapper(CRefWrapper&& other) noexcept
        : m_pValue(other.m_pValue)
        , m_bIsValid(other.m_bIsValid)
    {
        other.m_pValue = nullptr;
        other.m_bIsValid = false;
    }
    
    // Assignment from reference
    CRefWrapper& operator=(ReferenceType rValue)
    {
        m_pValue = &rValue;
        m_bIsValid = true;
        return *this;
    }
    
    // Assignment from pointer
    CRefWrapper& operator=(PointerType pValue)
    {
        m_pValue = pValue;
        m_bIsValid = (pValue != nullptr);
        return *this;
    }
    
    // Copy assignment
    CRefWrapper& operator=(const CRefWrapper& other)
    {
        if (this != &other) {
            m_pValue = other.m_pValue;
            m_bIsValid = other.m_bIsValid;
        }
        return *this;
    }
    
    // Move assignment
    CRefWrapper& operator=(CRefWrapper&& other) noexcept
    {
        if (this != &other) {
            m_pValue = other.m_pValue;
            m_bIsValid = other.m_bIsValid;
            other.m_pValue = nullptr;
            other.m_bIsValid = false;
        }
        return *this;
    }
    
    // Reset to invalid state
    void Reset()
    {
        m_pValue = nullptr;
        m_bIsValid = false;
    }
    
    // Reset with new reference
    void Reset(ReferenceType rValue)
    {
        m_pValue = &rValue;
        m_bIsValid = true;
    }
    
    // Reset with new pointer
    void Reset(PointerType pValue)
    {
        m_pValue = pValue;
        m_bIsValid = (pValue != nullptr);
    }
    
    // Check validity
    bool IsValid() const { return m_bIsValid; }
    bool IsNull() const { return !m_bIsValid || m_pValue == nullptr; }
    
    // Safe access - returns reference, asserts if invalid
    ReferenceType GetRef()
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return *m_pValue;
    }
    
    ConstReferenceType GetRef() const
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return *m_pValue;
    }
    
    // Pointer access
    PointerType GetPtr()
    {
        return m_bIsValid ? m_pValue : nullptr;
    }
    
    ConstPointerType GetPtr() const
    {
        return m_bIsValid ? m_pValue : nullptr;
    }
    
    // Unsafe access - use with caution
    ReferenceType GetRefUnsafe()
    {
        // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
        return *m_pValue;
    }
    
    ConstReferenceType GetRefUnsafe() const
    {
        return *m_pValue;
    }
    
    // Get or default
    ReferenceType GetOrDefault(ReferenceType rDefault)
    {
        return m_bIsValid && m_pValue ? *m_pValue : rDefault;
    }
    
    ConstReferenceType GetOrDefault(ConstReferenceType rDefault) const
    {
        return m_bIsValid && m_pValue ? *m_pValue : rDefault;
    }
    
    // Dereference operators
    ReferenceType operator*()
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return *m_pValue;
    }
    
    ConstReferenceType operator*() const
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return *m_pValue;
    }
    
    PointerType operator->()
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return m_pValue;
    }
    
    ConstPointerType operator->() const
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return m_pValue;
    }
    
    // Boolean conversion
    explicit operator bool() const { return m_bIsValid && m_pValue != nullptr; }
    
    // Comparison operators
    bool operator==(const CRefWrapper& other) const
    {
        return m_pValue == other.m_pValue;
    }
    
    bool operator!=(const CRefWrapper& other) const
    {
        return m_pValue != other.m_pValue;
    }
    
    bool operator==(ConstPointerType pOther) const
    {
        return m_pValue == pOther;
    }
    
    bool operator!=(ConstPointerType pOther) const
    {
        return m_pValue != pOther;
    }
    
    // Swap
    void Swap(CRefWrapper& other) noexcept
    {
        std::swap(m_pValue, other.m_pValue);
        std::swap(m_bIsValid, other.m_bIsValid);
    }

private:
    PointerType m_pValue;
    bool        m_bIsValid;
};

// ============================================================================
// Non-member functions
// ============================================================================

template<typename T>
void swap(CRefWrapper<T>& lhs, CRefWrapper<T>& rhs) noexcept
{
    lhs.Swap(rhs);
}

template<typename T>
CRefWrapper<T> MakeRefWrapper(T& rValue)
{
    return CRefWrapper<T>(rValue);
}

template<typename T>
CRefWrapper<T> MakeRefWrapper(T* pValue)
{
    return CRefWrapper<T>(pValue);
}

// ============================================================================
// CConstRefWrapper - Const reference wrapper
// ============================================================================
template<typename T>
class CConstRefWrapper {
public:
    using ValueType = T;
    using ConstReferenceType = const T&;
    using ConstPointerType = const T*;

    CConstRefWrapper()
        : m_pValue(nullptr)
        , m_bIsValid(false)
    {}
    
    explicit CConstRefWrapper(ConstReferenceType rValue)
        : m_pValue(&rValue)
        , m_bIsValid(true)
    {}
    
    explicit CConstRefWrapper(ConstPointerType pValue)
        : m_pValue(pValue)
        , m_bIsValid(pValue != nullptr)
    {}
    
    CConstRefWrapper(const CRefWrapper<T>& other)
        : m_pValue(other.GetPtr())
        , m_bIsValid(other.IsValid())
    {}
    
    bool IsValid() const { return m_bIsValid; }
    bool IsNull() const { return !m_bIsValid || m_pValue == nullptr; }
    
    ConstReferenceType GetRef() const
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return *m_pValue;
    }
    
    ConstPointerType GetPtr() const
    {
        return m_bIsValid ? m_pValue : nullptr;
    }
    
    ConstReferenceType GetOrDefault(ConstReferenceType rDefault) const
    {
        return m_bIsValid && m_pValue ? *m_pValue : rDefault;
    }
    
    ConstReferenceType operator*() const
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return *m_pValue;
    }
    
    ConstPointerType operator->() const
    {
        assert(m_bIsValid && m_pValue != nullptr);
        return m_pValue;
    }
    
    explicit operator bool() const { return m_bIsValid && m_pValue != nullptr; }

private:
    ConstPointerType m_pValue;
    bool             m_bIsValid;
};

// ============================================================================
// Helper macros for reference wrappers
// ============================================================================

#define REF_WRAP(var) CRefWrapper<decltype(var)>(var)
#define CONST_REF_WRAP(var) CConstRefWrapper<decltype(var)>(var)
#define SAFE_REF_GET(refWrapper, defaultVal) ((refWrapper).IsValid() ? (refWrapper).GetRef() : (defaultVal))
