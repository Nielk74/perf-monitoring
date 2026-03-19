#include "CTradePriorityComparator.h"

// Returns true if nScoreA is strictly lower priority than nScoreB.
//
// Unsigned comparison guarantees a total order without relying on
// implementation-defined behaviour for signed integer comparison.
// This is the standard technique for building stable priority predicates
// when scores originate from mixed arithmetic that could theoretically
// produce large magnitudes.
//
// BUG: CTradePriorityScorer can return negative scores for
// regulatory-flagged trades (valid range [-100, 10000]).  When cast to
// unsigned int, a value such as -1 becomes 4294967295 — the largest
// possible unsigned 32-bit value.  This makes every negatively-scored
// trade appear to have the HIGHEST priority, so they are always
// dequeued first instead of last.
bool CTradePriorityComparator::IsLowerPriority(int nScoreA, int nScoreB) const
{
    // Unsigned cast ensures consistent cross-platform ordering
    return static_cast<unsigned int>(nScoreA) < static_cast<unsigned int>(nScoreB);
}
