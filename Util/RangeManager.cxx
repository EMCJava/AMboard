//
// Created by LYS on 2/18/2026.
//

#include "RangeManager.hxx"

void CRangeManager::SetSlot(int Index)
{
    SetRange(Index, 1);
}

void CRangeManager::SetRange(int Offset, int Count)
{
    auto Right = Offset + Count - 1;

    // 1. Check if we can merge with the RIGHT neighbor (starts at right + 1)
    auto right_it = m_Intervals.find(Right + 1);
    if (right_it != m_Intervals.end()) {
        Right = right_it->second; // Extend our new right to the neighbor's end
        m_Intervals.erase(right_it); // Remove the neighbor (we will re-insert the merged one)
    }

    // 2. Check if we can merge with the LEFT neighbor (ends at left - 1)
    // We look for the interval that starts before or at left.
    auto left_it = m_Intervals.upper_bound(Offset);

    if (left_it != m_Intervals.begin()) {
        left_it--; // Move back to the potential left neighbor

        // If the left neighbor ends exactly at left - 1, we merge
        if (left_it->second == Offset - 1) {
            left_it->second = Right; // Update the existing left node's end
            return; // Done (we extended the left node to cover the new range and potentially the right neighbor)
        }
    }

    // 3. If no left merge happened, insert the new range
    m_Intervals[Offset] = Right;
}

void CRangeManager::RemoveSlot(int Index)
{
    RemoveRange(Index, 1);
}

void CRangeManager::RemoveRange(int Offset, int Count)
{
    const auto Right = Offset + Count - 1;

    // Find the ONE interval that contains this range.
    // upper_bound(left) gives the first interval starting > left.
    // The interval containing 'left' must be the one immediately before that.
    auto it = m_Intervals.upper_bound(Offset);

    // Per constraints (valid API call), 'it' cannot be begin(),
    // because the range [left, right] must exist within an active slot.
    it--;

    int currentStart = it->first;
    int currentEnd = it->second;

    // We are modifying this node.
    // If we are cutting off the left side (start == left), we must erase the node
    // because we cannot change the Key of a map element in-place.
    // If we are keeping the start (start < left), we can just update the value.

    // 1. Handle the Right side remainder
    // If the removal stops before the end, we need to create a new interval for the remainder.
    if (currentEnd > Right) {
        m_Intervals[Right + 1] = currentEnd;
    }

    // 2. Handle the Left side
    if (currentStart < Offset) {
        // We keep the left part, just shorten the end
        it->second = Offset - 1;
    } else {
        // currentStart == left (Exact match on start)
        // We must remove the original node
        m_Intervals.erase(it);
    }
}

int CRangeManager::FirstFit(const int Count) const
{
    if (Count <= 0)
        return 0; // Safety check, though usually n > 0

    int CandidateStart = 0; // Valid ranges start at >= 0

    for (const auto& [Left, Right] : m_Intervals) {

        // Calculate the size of the empty gap before this interval
        // The gap is [candidateStart, intervalStart - 1]
        // Size = intervalStart - candidateStart
        if (Left - CandidateStart >= Count) {
            return CandidateStart;
        }

        // If the gap wasn't big enough, the next possible start
        // is immediately after the current interval.
        CandidateStart = Right + 1;
    }

    // If we checked all gaps and found nothing, append to the end
    return CandidateStart;
}

int CRangeManager::GetFirstFreeIndex() const noexcept
{
    if (m_Intervals.empty()) [[unlikely]] {
        return 0;
    }

    return std::prev(m_Intervals.end())->second + 1;
}