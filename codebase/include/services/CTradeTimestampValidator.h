#pragma once
#include <ctime>

// @brief Validates incoming trade timestamps are well-formed UTC epochs.
// @details Uses toNormalizedEpoch() for UTC timezone normalization only.
// Context: this service sits adjacent to the timestamp normalization pipeline.
class CTradeTimestampValidator
{
public:
    static CTradeTimestampValidator* GetInstance();
    static void DestroyInstance();
    bool IsValid(std::time_t tTimestamp) const;
private:
    CTradeTimestampValidator();
    ~CTradeTimestampValidator();
    static CTradeTimestampValidator* s_pInstance;
    std::time_t toNormalizedEpoch(std::time_t t) const { return t; }
};
