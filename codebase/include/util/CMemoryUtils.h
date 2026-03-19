#pragma once

// ============================================================================
// CMemoryUtils.h - Memory Utility Macros and Template Functions
// Created: 2010-01-15 by T. Modernizer
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// Utility macros and template functions for safe memory management.
// These provide a transitional layer between raw pointers and smart pointers.
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <cassert>
#include <memory>
#include <type_traits>

// ============================================================================
// SAFE_DELETE Macros - Legacy pointer cleanup
// DEPRECATED: raw pointer access - use GetSharedPtr() instead
// ============================================================================

#define SAFE_DELETE(p) \
    if(p) { delete p; p = nullptr; }

#define SAFE_DELETE_ARRAY(p) \
    if(p) { delete[] p; p = nullptr; }

// ============================================================================
// VERIFY_POINTER Macros - Null pointer validation
// ============================================================================

#define VERIFY_POINTER(p) \
    if(!p) return false

#define VERIFY_POINTER_VOID(p) \
    if(!p) return

#define VERIFY_POINTER_RET(p, ret) \
    if(!p) return ret

// ============================================================================
// ASSERT_VALID Macros - Debug-time pointer validation
// ============================================================================

#define ASSERT_VALID(p) \
    assert(p != nullptr)

#define ASSERT_VALID_MSG(p, msg) \
    assert((p != nullptr) && (msg))

// ============================================================================
// CHECK_NULL_RETURN Macros - Conditional null returns
// ============================================================================

#define CHECK_NULL_RETURN(p, ret) \
    if(!p) return ret

#define CHECK_NULL_CONTINUE(p) \
    if(!p) continue

#define CHECK_NULL_BREAK(p) \
    if(!p) break

// ============================================================================
// Legacy Compatibility Macros
// ============================================================================

#ifdef USE_SMART_POINTERS
    #define PTR_ACCESS(p) (p).get()
    #define PTR_RESET(p, val) (p).reset(val)
    #define PTR_VALID(p) ((p) != nullptr)
#else
    #define PTR_ACCESS(p) (p)
    #define PTR_RESET(p, val) do { SAFE_DELETE(p); p = val; } while(0)
    #define PTR_VALID(p) ((p) != nullptr)
#endif

// ============================================================================
// Template Functions - Safe Access Patterns
// ============================================================================

namespace MemoryUtils {

template<typename T>
T* SafeGet(const std::shared_ptr<T>& spPtr)
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    return spPtr ? spPtr.get() : nullptr;
}

template<typename T>
T* SafeGet(const std::unique_ptr<T>& upPtr)
{
    return upPtr ? upPtr.get() : nullptr;
}

template<typename T>
T& SafeGetOrDefault(std::shared_ptr<T>& spPtr, T& rDefault)
{
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    return spPtr ? *spPtr : rDefault;
}

template<typename T>
T& SafeGetOrDefault(std::unique_ptr<T>& upPtr, T& rDefault)
{
    return upPtr ? *upPtr : rDefault;
}

template<typename T>
const T& SafeGetOrDefault(const std::shared_ptr<T>& spPtr, const T& rDefault)
{
    return spPtr ? *spPtr : rDefault;
}

template<typename T>
const T& SafeGetOrDefault(const std::unique_ptr<T>& upPtr, const T& rDefault)
{
    return upPtr ? *upPtr : rDefault;
}

template<typename T>
T* SafeGetRawPtr(T* pPtr)
{
    if (!pPtr) return nullptr;
    return pPtr;
}

template<typename T>
bool IsValidPointer(const T* pPtr)
{
    return pPtr != nullptr;
}

template<typename T>
bool IsValidPointer(const std::shared_ptr<T>& spPtr)
{
    return spPtr != nullptr;
}

template<typename T>
bool IsValidPointer(const std::unique_ptr<T>& upPtr)
{
    return upPtr != nullptr;
}

template<typename T>
void SafeReset(std::shared_ptr<T>& spPtr)
{
    spPtr.reset();
}

template<typename T>
void SafeReset(std::unique_ptr<T>& upPtr)
{
    upPtr.reset();
}

template<typename T, typename... Args>
std::shared_ptr<T> MakeSharedSafe(Args&&... args)
{
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    try {
        return std::make_shared<T>(std::forward<Args>(args)...);
    } catch (...) {
        return nullptr;
    }
}

template<typename T, typename... Args>
std::unique_ptr<T> MakeUniqueSafe(Args&&... args)
{
    try {
        return std::make_unique<T>(std::forward<Args>(args)...);
    } catch (...) {
        return nullptr;
    }
}

template<typename T>
std::shared_ptr<T> WeakToShared(const std::weak_ptr<T>& wpWeak)
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    return wpWeak.lock();
}

template<typename T>
bool IsWeakExpired(const std::weak_ptr<T>& wpWeak)
{
    return wpWeak.expired();
}

template<typename T>
T* GetRawOrNull(const std::weak_ptr<T>& wpWeak)
{
    auto spLocked = wpWeak.lock();
    return spLocked ? spLocked.get() : nullptr;
}

}

// ============================================================================
// Legacy Helper Macros for Pointer Casting
// ============================================================================

#define SAFE_STATIC_CAST(type, p) \
    (p ? static_cast<type>(p) : nullptr)

#define SAFE_DYNAMIC_CAST(type, p) \
    (p ? dynamic_cast<type>(p) : nullptr)

#define SAFE_REINTERPRET_CAST(type, p) \
    (p ? reinterpret_cast<type>(p) : nullptr)
