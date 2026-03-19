#pragma once

// ============================================================================
// CMemoryManager.h - Singleton Memory Manager for Smart Pointer Management
// Created: 2010-01-15 by T. Modernizer
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// Singleton that wraps smart pointer creation and tracks allocations.
// Provides centralized memory management for the trading application.
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <memory>
#include <map>
#include <mutex>
#include <cstdint>
#include <string>
#include <functional>

// Forward declarations
class CMemoryManager;

// Pointer tracking info structure
struct PointerTrackingInfo {
    void*       m_pRawAddress;
    size_t      m_nSizeBytes;
    uint64_t    m_ulAllocationTime;
    std::string m_szTypeName;
    std::string m_szSourceFile;
    int         m_nSourceLine;
    bool        m_bIsArray;
    bool        m_bIsActive;
    
    PointerTrackingInfo()
        : m_pRawAddress(nullptr)
        , m_nSizeBytes(0)
        , m_ulAllocationTime(0)
        , m_nSourceLine(0)
        , m_bIsArray(false)
        , m_bIsActive(false)
    {}
};

// Memory statistics structure
struct MemoryStats {
    size_t   m_nTotalAllocations;
    size_t   m_nTotalDeallocations;
    size_t   m_nActivePointers;
    int64_t  m_lTotalAllocatedBytes;
    int64_t  m_lPeakAllocatedBytes;
    size_t   m_nSharedPointers;
    size_t   m_nUniquePointers;
    uint64_t m_ulLastAllocationTime;
    
    MemoryStats()
        : m_nTotalAllocations(0)
        , m_nTotalDeallocations(0)
        , m_nActivePointers(0)
        , m_lTotalAllocatedBytes(0)
        , m_lPeakAllocatedBytes(0)
        , m_nSharedPointers(0)
        , m_nUniquePointers(0)
        , m_ulLastAllocationTime(0)
    {}
};

// Memory event callback types
using MemoryAllocCallback = std::function<void(const PointerTrackingInfo&)>;
using MemoryDeallocCallback = std::function<void(const PointerTrackingInfo&)>;

// ============================================================================
// CMemoryManager - Singleton for centralized memory management
// DEPRECATED: raw pointer access - use GetSharedPtr() instead
// ============================================================================
class CMemoryManager {
public:
    static CMemoryManager& GetInstance();
    
    // Non-copyable, non-movable
    CMemoryManager(const CMemoryManager&) = delete;
    CMemoryManager& operator=(const CMemoryManager&) = delete;
    CMemoryManager(CMemoryManager&&) = delete;
    CMemoryManager& operator=(CMemoryManager&&) = delete;

    // Smart pointer creation
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateSharedPtr(Args&&... args);
    
    template<typename T, typename... Args>
    std::unique_ptr<T> CreateUniquePtr(Args&&... args);

    // Safe release for legacy pointers
    template<typename T>
    void SafeRelease(T*& pPtr);
    
    template<typename T>
    void SafeReleaseArray(T*& pPtr);

    // Tracking control
    void EnableTracking(bool bEnable) { m_bEnableTracking = bEnable; }
    bool IsTrackingEnabled() const { return m_bEnableTracking; }
    
    // Statistics
    MemoryStats GetStats() const;
    void ResetStats();
    
    size_t GetActivePointerCount() const { return m_nActivePointerCount; }
    int64_t GetTotalAllocatedBytes() const { return m_lTotalAllocatedBytes; }
    
    // Debug tracking
    void DumpAllocations() const;
    void DumpAllocationReport(const std::string& szFilePath) const;
    
    // Event callbacks
    void SetAllocCallback(MemoryAllocCallback fnCallback) { m_fnAllocCallback = fnCallback; }
    void SetDeallocCallback(MemoryDeallocCallback fnCallback) { m_fnDeallocCallback = fnCallback; }
    
    // Manual tracking for external allocations
    void TrackAllocation(void* pPtr, size_t nSize, const char* szTypeName, 
                         const char* szFile, int nLine, bool bIsArray = false);
    void UntrackAllocation(void* pPtr);
    
    // Query tracking info
    bool IsTracked(void* pPtr) const;
    const PointerTrackingInfo* GetTrackingInfo(void* pPtr) const;
    
    // Legacy compatibility
    void* AllocateRaw(size_t nSize, const char* szTypeName = nullptr);
    void DeallocateRaw(void* pPtr);

private:
    CMemoryManager();
    ~CMemoryManager();
    
    void RecordAllocation(const PointerTrackingInfo& info);
    void RecordDeallocation(void* pPtr);
    
    // Tracking data
    std::map<void*, PointerTrackingInfo> m_mapTrackedPointers;
    mutable std::mutex m_mutexTracking;
    
    // Statistics
    int64_t  m_lTotalAllocatedBytes;
    size_t   m_nActivePointerCount;
    size_t   m_nTotalAllocations;
    size_t   m_nTotalDeallocations;
    int64_t  m_lPeakAllocatedBytes;
    
    // Tracking flags
    bool m_bEnableTracking;
    bool m_bEnableDebugOutput;
    
    // Callbacks
    MemoryAllocCallback m_fnAllocCallback;
    MemoryDeallocCallback m_fnDeallocCallback;
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T, typename... Args>
std::shared_ptr<T> CMemoryManager::CreateSharedPtr(Args&&... args)
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    std::shared_ptr<T> spResult;
    
    try {
        spResult = std::make_shared<T>(std::forward<Args>(args)...);
        
        if (m_bEnableTracking && spResult) {
            PointerTrackingInfo info;
            info.m_pRawAddress = spResult.get();
            info.m_nSizeBytes = sizeof(T);
            info.m_szTypeName = typeid(T).name();
            info.m_bIsArray = false;
            info.m_bIsActive = true;
            RecordAllocation(info);
        }
    } catch (...) {
        // Allocation failed
        return nullptr;
    }
    
    return spResult;
}

template<typename T, typename... Args>
std::unique_ptr<T> CMemoryManager::CreateUniquePtr(Args&&... args)
{
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    std::unique_ptr<T> upResult;
    
    try {
        upResult = std::make_unique<T>(std::forward<Args>(args)...);
        
        if (m_bEnableTracking && upResult) {
            PointerTrackingInfo info;
            info.m_pRawAddress = upResult.get();
            info.m_nSizeBytes = sizeof(T);
            info.m_szTypeName = typeid(T).name();
            info.m_bIsArray = false;
            info.m_bIsActive = true;
            RecordAllocation(info);
        }
    } catch (...) {
        return nullptr;
    }
    
    return upResult;
}

template<typename T>
void CMemoryManager::SafeRelease(T*& pPtr)
{
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    if (pPtr) {
        if (m_bEnableTracking) {
            RecordDeallocation(pPtr);
        }
        delete pPtr;
        pPtr = nullptr;
    }
}

template<typename T>
void CMemoryManager::SafeReleaseArray(T*& pPtr)
{
    if (pPtr) {
        if (m_bEnableTracking) {
            RecordDeallocation(pPtr);
        }
        delete[] pPtr;
        pPtr = nullptr;
    }
}

// ============================================================================
// Global accessor macro
// ============================================================================

#define g_MemoryManager CMemoryManager::GetInstance()

// Tracking macros for debug builds
#ifdef _DEBUG
    #define TRACK_ALLOC(ptr, type, size) \
        g_MemoryManager.TrackAllocation(ptr, size, #type, __FILE__, __LINE__)
    #define TRACK_DEALLOC(ptr) \
        g_MemoryManager.UntrackAllocation(ptr)
#else
    #define TRACK_ALLOC(ptr, type, size) ((void)0)
    #define TRACK_DEALLOC(ptr) ((void)0)
#endif
