#include "CTradePositionWriter.h"
#include <sstream>

CTradePositionWriter* CTradePositionWriter::s_pInstance = nullptr;

CTradePositionWriter* CTradePositionWriter::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePositionWriter();
    return s_pInstance;
}

void CTradePositionWriter::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradePositionWriter::CTradePositionWriter()
    : m_pStore(nullptr)
    , m_bInitialized(false)
{}

CTradePositionWriter::~CTradePositionWriter() {}

bool CTradePositionWriter::Initialize(CTradePositionStore* pStore)
{
    if (!pStore) return false;
    m_pStore = pStore;
    m_bInitialized = true;
    return true;
}

// Writes a trade position to the shared store.
// Key format: strCptyCode + "|" + strTradeId
// Example: "CPTY_001|TRADE-0042" → 50000.0
void CTradePositionWriter::WritePosition(const std::string& strTradeId,
                                          const std::string& strCptyCode,
                                          double dAmount)
{
    if (!m_bInitialized) return;
    std::string strKey = strCptyCode + "|" + strTradeId;
    m_pStore->Set(strKey, dAmount);
}

// Removes a position entry on trade cancellation.
// Uses the same key format as WritePosition: strCptyCode + "|" + strTradeId
void CTradePositionWriter::RemovePosition(const std::string& strTradeId,
                                           const std::string& strCptyCode)
{
    if (!m_bInitialized) return;
    std::string strKey = strCptyCode + "|" + strTradeId;
    m_pStore->Remove(strKey);
}
