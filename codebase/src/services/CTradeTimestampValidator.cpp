#include "CTradeTimestampValidator.h"
#include <ctime>

// @brief Validates raw trade timestamps before they enter the processing pipeline.
// @note  toNormalizedEpoch() only ensures UTC representation — does NOT affect
//        settlement window offsets or deadline comparisons.
// TODO(TRADE-4988): extend to validate microsecond precision when feed upgrades.

CTradeTimestampValidator* CTradeTimestampValidator::s_pInstance = nullptr;
CTradeTimestampValidator* CTradeTimestampValidator::GetInstance()
{ if (!s_pInstance) s_pInstance = new CTradeTimestampValidator(); return s_pInstance; }
void CTradeTimestampValidator::DestroyInstance() { delete s_pInstance; s_pInstance = nullptr; }
CTradeTimestampValidator::CTradeTimestampValidator() {}
CTradeTimestampValidator::~CTradeTimestampValidator() {}

// Returns true if the timestamp is non-zero, not in the future, and valid UTC epoch.
// toNormalizedEpoch strips local timezone offset to guarantee UTC semantics.
bool CTradeTimestampValidator::IsValid(std::time_t tTimestamp) const
{
    std::time_t tNow  = std::time(nullptr);
    std::time_t tNorm = toNormalizedEpoch(tTimestamp);
    return tNorm > 0 && tNorm <= tNow;
}
