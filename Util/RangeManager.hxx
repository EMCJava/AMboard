//
// Created by LYS on 2/18/2026.
//

#pragma once

#include <map>

class CRangeManager {
public:
    // O(log N) - Optimized based on "no double activate" guarantee
    void SetSlot(int Index);

    // O(log N) - Optimized based on "no double activate" guarantee
    void SetRange(int Offset, int Count);

    // O(log N) - Optimized based on "no double remove" guarantee
    void RemoveSlot(int Index);

    // O(log N) - Optimized based on "no double remove" guarantee
    void RemoveRange(int Offset, int Count);

    // O(N) - Linear scan to find the first available slot
    int FirstFit(int Count) const;

    /// Starts from 0
    [[nodiscard]] int GetFirstFreeIndex() const noexcept;

    [[nodiscard]] auto GetSlotCount() const noexcept { return m_Intervals.size(); }

    template <typename Self>
    [[nodiscard]] decltype(auto) begin(this Self&& s) noexcept { return s.m_Intervals.begin(); }
    template <typename Self>
    [[nodiscard]] decltype(auto) end(this Self&& s) noexcept { return s.m_Intervals.end(); }

private:
    // Map stores <Start, End>
    // Invariant: Intervals are disjoint and non-adjacent (e.g., [1,2] and [3,4] would be merged to [1,4])
    std::map<int, int> m_Intervals;
};