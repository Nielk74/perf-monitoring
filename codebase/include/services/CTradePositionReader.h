#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include <vector>
#include "ServiceCommon.h"
#include "CTradePositionStore.h"

// Reads trade positions from the shared CTradePositionStore.
// Positions are written by CTradePositionWriter — this service has no
// direct dependency on CTradePositionWriter.
class CTradePositionReader
{
public:
    static CTradePositionReader* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradePositionStore* pStore);

    // Read the recorded position for a specific trade.
    // Returns true and populates refAmount if a position is found.
    // Returns false if no position exists for this trade/counterparty combination.
    bool ReadPosition(const std::string& strTradeId,
                      const std::string& strCptyCode,
                      double& refAmount) const;

    // Returns the total position across all trades for a counterparty.
    // Scans the store for all keys belonging to the counterparty.
    double GetTotalPosition(const std::string& strCptyCode) const;

private:
    CTradePositionReader();
    ~CTradePositionReader();
    CTradePositionReader(const CTradePositionReader&);
    CTradePositionReader& operator=(const CTradePositionReader&);

    CTradePositionStore* m_pStore;
    bool m_bInitialized;
    static CTradePositionReader* s_pInstance;
};
