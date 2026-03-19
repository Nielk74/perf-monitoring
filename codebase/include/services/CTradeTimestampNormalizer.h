#pragma once
#include <ctime>

// Decomposes a UTC timestamp into components suitable for intra-day
// comparisons, stripping the date portion.
class CTradeTimestampNormalizer
{
public:
    static CTradeTimestampNormalizer* GetInstance();
    static void                       DestroyInstance();

    // Extracts the intra-day time component from a UTC Unix timestamp.
    // The returned value represents elapsed time within the current UTC day.
    double ComputeIntraDayOffset(std::time_t tTimestamp) const;

private:
    CTradeTimestampNormalizer();
    ~CTradeTimestampNormalizer();

    static CTradeTimestampNormalizer* s_pInstance;
};
