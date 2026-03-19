#include "util/CMemoryManager.h"

// ============================================================================
// CMemoryManager.cpp - Singleton Memory Manager Implementation
// Created: 2010-01-15 by T. Modernizer
// Last Modified: 2010-02-20 by T. Modernizer
// ============================================================================
// Implementation of the centralized memory management system.
//
// NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
// FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
// ============================================================================

#include <cstdio>
#include <chrono>
#include <fstream>
#include <iostream>
#include <cstring>

// ============================================================================
// Static instance accessor
// ============================================================================

CMemoryManager& CMemoryManager::GetInstance()
{
    // Thread-safe singleton using Meyers' singleton pattern
    // Added in v3.0 for thread safety
    static CMemoryManager s_instance;
    return s_instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

CMemoryManager::CMemoryManager()
    : m_lTotalAllocatedBytes(0)
    , m_nActivePointerCount(0)
    , m_nTotalAllocations(0)
    , m_nTotalDeallocations(0)
    , m_lPeakAllocatedBytes(0)
    , m_bEnableTracking(true)
    , m_bEnableDebugOutput(false)
    , m_fnAllocCallback(nullptr)
    , m_fnDeallocCallback(nullptr)
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    // Initialize tracking map
    m_mapTrackedPointers.clear();
}

CMemoryManager::~CMemoryManager()
{
    // Clean up any remaining tracked allocations
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    if (m_bEnableDebugOutput && !m_mapTrackedPointers.empty()) {
        // Report memory leaks
        std::cerr << "[CMemoryManager] Warning: " << m_mapTrackedPointers.size() 
                  << " allocations still active at shutdown" << std::endl;
        
        for (const auto& pair : m_mapTrackedPointers) {
            const PointerTrackingInfo& info = pair.second;
            std::cerr << "  Leak: " << info.m_szTypeName 
                      << " at " << info.m_pRawAddress
                      << " (" << info.m_nSizeBytes << " bytes)"
                      << " from " << info.m_szSourceFile 
                      << ":" << info.m_nSourceLine << std::endl;
        }
    }
    
    m_mapTrackedPointers.clear();
}

// ============================================================================
// Allocation Recording
// ============================================================================

void CMemoryManager::RecordAllocation(const PointerTrackingInfo& info)
{
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    if (!m_bEnableTracking) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    // Store tracking info
    m_mapTrackedPointers[info.m_pRawAddress] = info;
    
    // Update statistics
    m_nTotalAllocations++;
    m_nActivePointerCount++;
    m_lTotalAllocatedBytes += static_cast<int64_t>(info.m_nSizeBytes);
    
    if (m_lTotalAllocatedBytes > m_lPeakAllocatedBytes) {
        m_lPeakAllocatedBytes = m_lTotalAllocatedBytes;
    }
    
    // Fire callback if registered
    if (m_fnAllocCallback) {
        m_fnAllocCallback(info);
    }
    
    if (m_bEnableDebugOutput) {
        std::cout << "[CMemoryManager] Allocated: " << info.m_szTypeName
                  << " at " << info.m_pRawAddress
                  << " (" << info.m_nSizeBytes << " bytes)" << std::endl;
    }
}

void CMemoryManager::RecordDeallocation(void* pPtr)
{
    if (!m_bEnableTracking || !pPtr) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    auto it = m_mapTrackedPointers.find(pPtr);
    if (it != m_mapTrackedPointers.end()) {
        PointerTrackingInfo info = it->second;
        
        // Update statistics
        m_nTotalDeallocations++;
        if (m_nActivePointerCount > 0) {
            m_nActivePointerCount--;
        }
        m_lTotalAllocatedBytes -= static_cast<int64_t>(info.m_nSizeBytes);
        if (m_lTotalAllocatedBytes < 0) {
            m_lTotalAllocatedBytes = 0;
        }
        
        // Fire callback if registered
        if (m_fnDeallocCallback) {
            m_fnDeallocCallback(info);
        }
        
        if (m_bEnableDebugOutput) {
            std::cout << "[CMemoryManager] Deallocated: " << info.m_szTypeName
                      << " at " << info.m_pRawAddress << std::endl;
        }
        
        m_mapTrackedPointers.erase(it);
    }
}

// ============================================================================
// Statistics
// ============================================================================

MemoryStats CMemoryManager::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    MemoryStats stats;
    stats.m_nTotalAllocations = m_nTotalAllocations;
    stats.m_nTotalDeallocations = m_nTotalDeallocations;
    stats.m_nActivePointers = m_nActivePointerCount;
    stats.m_lTotalAllocatedBytes = m_lTotalAllocatedBytes;
    stats.m_lPeakAllocatedBytes = m_lPeakAllocatedBytes;
    stats.m_nSharedPointers = 0;  // Would need separate tracking
    stats.m_nUniquePointers = 0;  // Would need separate tracking
    
    if (!m_mapTrackedPointers.empty()) {
        auto last = m_mapTrackedPointers.rbegin();
        stats.m_ulLastAllocationTime = last->second.m_ulAllocationTime;
    }
    
    return stats;
}

void CMemoryManager::ResetStats()
{
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    m_nTotalAllocations = 0;
    m_nTotalDeallocations = 0;
    m_nActivePointerCount = m_mapTrackedPointers.size();
    m_lTotalAllocatedBytes = 0;
    m_lPeakAllocatedBytes = 0;
    
    // Recalculate current allocated bytes
    for (const auto& pair : m_mapTrackedPointers) {
        m_lTotalAllocatedBytes += static_cast<int64_t>(pair.second.m_nSizeBytes);
    }
}

// ============================================================================
// Debug Tracking
// ============================================================================

void CMemoryManager::DumpAllocations() const
{
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    std::cout << "=== CMemoryManager Allocation Dump ===" << std::endl;
    std::cout << "Active pointers: " << m_nActivePointerCount << std::endl;
    std::cout << "Total allocated: " << m_lTotalAllocatedBytes << " bytes" << std::endl;
    std::cout << "Peak allocated:  " << m_lPeakAllocatedBytes << " bytes" << std::endl;
    std::cout << std::endl;
    
    if (m_mapTrackedPointers.empty()) {
        std::cout << "No active allocations." << std::endl;
        return;
    }
    
    for (const auto& pair : m_mapTrackedPointers) {
        const PointerTrackingInfo& info = pair.second;
        std::cout << "  [" << info.m_szTypeName << "] "
                  << info.m_pRawAddress
                  << " (" << info.m_nSizeBytes << " bytes)"
                  << " - " << info.m_szSourceFile
                  << ":" << info.m_nSourceLine << std::endl;
    }
}

void CMemoryManager::DumpAllocationReport(const std::string& szFilePath) const
{
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    std::ofstream file(szFilePath);
    if (!file.is_open()) {
        std::cerr << "[CMemoryManager] Failed to open report file: " << szFilePath << std::endl;
        return;
    }
    
    file << "CMemoryManager Allocation Report" << std::endl;
    file << "================================" << std::endl;
    file << std::endl;
    file << "Statistics:" << std::endl;
    file << "  Active pointers: " << m_nActivePointerCount << std::endl;
    file << "  Total allocations: " << m_nTotalAllocations << std::endl;
    file << "  Total deallocations: " << m_nTotalDeallocations << std::endl;
    file << "  Currently allocated: " << m_lTotalAllocatedBytes << " bytes" << std::endl;
    file << "  Peak allocated: " << m_lPeakAllocatedBytes << " bytes" << std::endl;
    file << std::endl;
    
    if (!m_mapTrackedPointers.empty()) {
        file << "Active Allocations:" << std::endl;
        for (const auto& pair : m_mapTrackedPointers) {
            const PointerTrackingInfo& info = pair.second;
            file << "  Type: " << info.m_szTypeName << std::endl;
            file << "    Address: " << info.m_pRawAddress << std::endl;
            file << "    Size: " << info.m_nSizeBytes << " bytes" << std::endl;
            file << "    Source: " << info.m_szSourceFile 
                 << ":" << info.m_nSourceLine << std::endl;
            file << std::endl;
        }
    }
    
    file.close();
}

// ============================================================================
// Manual Tracking
// ============================================================================

void CMemoryManager::TrackAllocation(void* pPtr, size_t nSize, const char* szTypeName,
                                     const char* szFile, int nLine, bool bIsArray)
{
    // NOTE: transitioning to smart pointers - legacy raw pointers kept for ABI compatibility
    if (!pPtr || !m_bEnableTracking) {
        return;
    }
    
    PointerTrackingInfo info;
    info.m_pRawAddress = pPtr;
    info.m_nSizeBytes = nSize;
    info.m_szTypeName = szTypeName ? szTypeName : "unknown";
    info.m_szSourceFile = szFile ? szFile : "";
    info.m_nSourceLine = nLine;
    info.m_bIsArray = bIsArray;
    info.m_bIsActive = true;
    info.m_ulAllocationTime = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    
    RecordAllocation(info);
}

void CMemoryManager::UntrackAllocation(void* pPtr)
{
    RecordDeallocation(pPtr);
}

bool CMemoryManager::IsTracked(void* pPtr) const
{
    if (!pPtr) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    return m_mapTrackedPointers.find(pPtr) != m_mapTrackedPointers.end();
}

const PointerTrackingInfo* CMemoryManager::GetTrackingInfo(void* pPtr) const
{
    if (!pPtr) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mutexTracking);
    
    auto it = m_mapTrackedPointers.find(pPtr);
    if (it != m_mapTrackedPointers.end()) {
        return &(it->second);
    }
    
    return nullptr;
}

// ============================================================================
// Legacy Compatibility
// ============================================================================

void* CMemoryManager::AllocateRaw(size_t nSize, const char* szTypeName)
{
    // DEPRECATED: raw pointer access - use GetSharedPtr() instead
    void* pPtr = ::operator new(nSize, std::nothrow);
    
    if (pPtr && m_bEnableTracking) {
        TrackAllocation(pPtr, nSize, szTypeName ? szTypeName : "raw", 
                       "unknown", 0, false);
    }
    
    return pPtr;
}

void CMemoryManager::DeallocateRaw(void* pPtr)
{
    // FIXME: some legacy code still uses raw pointers, DO NOT remove until v3 migration complete
    if (pPtr) {
        if (m_bEnableTracking) {
            RecordDeallocation(pPtr);
        }
        ::operator delete(pPtr);
    }
}
