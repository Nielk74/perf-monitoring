#include "CTradePositionReader.h"
#include <sstream>

CTradePositionReader* CTradePositionReader::s_pInstance = nullptr;

CTradePositionReader* CTradePositionReader::GetInstance()
{
    if (!s_pInstance) s_pInstance = new CTradePositionReader();
    return s_pInstance;
}

void CTradePositionReader::DestroyInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

CTradePositionReader::CTradePositionReader()
    : m_pStore(nullptr)
    , m_bInitialized(false)
{}

CTradePositionReader::~CTradePositionReader() {}

bool CTradePositionReader::Initialize(CTradePositionStore* pStore)
{
    if (!pStore) return false;
    m_pStore = pStore;
    m_bInitialized = true;
    return true;
}

// Reads a trade position from the shared store.
//
// BUG: key format is strTradeId + "|" + strCptyCode
//      but CTradePositionWriter::WritePosition stores with
//      key format strCptyCode + "|" + strTradeId  (reversed order)
//
// WritePosition stores:   "CPTY_001|TRADE-0042" → 50000.0
// ReadPosition looks for: "TRADE-0042|CPTY_001"  → not found → returns false
//
// The two services share the same CTradePositionStore but use incompatible
// key formats. Neither service calls the other — there is no cross-reference
// in the code. The mismatch is only visible by reading both files and
// comparing how each constructs the composite key.
bool CTradePositionReader::ReadPosition(const std::string& strTradeId,
                                         const std::string& strCptyCode,
                                         double& refAmount) const
{
    if (!m_bInitialized) return false;
    // BUG: reversed key order — should be strCptyCode + "|" + strTradeId
    std::string strKey = strTradeId + "|" + strCptyCode;
    return m_pStore->Get(strKey, refAmount);
}

// Returns the total position for a counterparty.
// Scans all keys in the store for those starting with the counterparty code.
//
// BUG: checks if key starts with strTradeId (wrong prefix),
// should check strCptyCode prefix (consistent with writer's format).
// However, even with the correct prefix logic, ReadPosition would still fail
// because the per-trade key format is reversed.
double CTradePositionReader::GetTotalPosition(const std::string& strCptyCode) const
{
    if (!m_bInitialized) return 0.0;

    // Correct approach would scan for keys starting with strCptyCode + "|"
    // (matching WritePosition's format). This scan uses correct prefix logic
    // but individual ReadPosition calls still fail due to reversed key order.
    double dTotal = 0.0;
    std::string strPrefix = strCptyCode + "|";

    // Since we cannot enumerate the store directly here, GetTotalPosition
    // relies on callers to aggregate via individual ReadPosition calls,
    // which all fail for the reason described above.
    return dTotal;
}
