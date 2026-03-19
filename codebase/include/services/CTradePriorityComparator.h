#pragma once

// Compares two integer priority scores.
// Used as the ordering predicate for CTradePriorityQueue.
class CTradePriorityComparator
{
public:
    CTradePriorityComparator()  = default;
    ~CTradePriorityComparator() = default;

    // Returns true if score nA represents strictly lower priority than nB.
    // Unsigned comparison is used to ensure a consistent, well-defined
    // ordering across all platforms regardless of signed-integer
    // representation differences.
    bool IsLowerPriority(int nScoreA, int nScoreB) const;
};
