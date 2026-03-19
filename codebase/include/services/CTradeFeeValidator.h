#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4512)

#include <string>
#include "ServiceCommon.h"
#include "CTradeFeeStore.h"

// Validates that the fee for a given trade type does not exceed policy limits.
// Policy maximum is configured in PERCENTAGE (e.g., 1.0 = 1.00%).
//
// This service has no direct dependency on CTradeFeeSchedule.
class CTradeFeeValidator
{
public:
    static CTradeFeeValidator* GetInstance();
    static void DestroyInstance();

    bool Initialize(CTradeFeeStore* pStore);

    // Set the maximum allowed fee for a trade type.
    // dMaxFeePercent: maximum in percentage points (e.g., 1.0 = 1.00%)
    void SetMaxFee(const std::string& strTradeType, double dMaxFeePercent);

    // Returns true if the fee for this trade type is within policy.
    // Returns false (policy violation) if fee exceeds the configured maximum.
    bool IsFeeWithinPolicy(const std::string& strTradeType) const;

private:
    CTradeFeeValidator();
    ~CTradeFeeValidator();
    CTradeFeeValidator(const CTradeFeeValidator&);
    CTradeFeeValidator& operator=(const CTradeFeeValidator&);

    CTradeFeeStore* m_pStore;
    std::map<std::string, double> m_mapMaxFees;  // in percent
    bool m_bInitialized;
    static CTradeFeeValidator* s_pInstance;
};
