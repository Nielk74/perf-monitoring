#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradeFeeStore.h"

// Manages the fee schedule for each trade type.
// Fees are expressed in BASIS POINTS (bps): 100 bps = 1%.
// Example: vanilla equity trades → 50 bps (= 0.5%)
//
// This service has no direct dependency on CTradeFeeValidator.
class CTradeFeeSchedule
{
public:
    static CTradeFeeSchedule* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeFeeStore* pStore);

    // Register the fee for a trade type.
    // dFeeBasisPoints: fee in basis points (e.g., 50 = 0.50%, 100 = 1.00%)
    void SetFee(const std::string& strTradeType, double dFeeBasisPoints);

    // Retrieve the fee for a trade type.
    // Returns fee in basis points, or 0.0 if not configured.
    double GetFee(const std::string& strTradeType) const;

private:
    CTradeFeeSchedule();
    ~CTradeFeeSchedule();
    CTradeFeeSchedule(const CTradeFeeSchedule&);
    CTradeFeeSchedule& operator=(const CTradeFeeSchedule&);

    CTradeFeeStore* m_pStore;
    bool m_bInitialized;
    static CTradeFeeSchedule* s_pInstance;
};
