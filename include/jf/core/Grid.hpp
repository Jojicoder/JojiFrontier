#pragma once

#include <cstdlib>
#include <vector>

namespace jf {

constexpr int kGridRows = 3;
constexpr int kGridCols = 8;

struct GridPos {
    int row = 0;
    int col = 0;

    bool operator==(const GridPos& other) const {
        return row == other.row && col == other.col;
    }
    bool operator!=(const GridPos& other) const { return !(*this == other); }
};

inline bool isInBounds(GridPos pos) {
    return pos.row >= 0 && pos.row < kGridRows && pos.col >= 0 && pos.col < kGridCols;
}

inline int manhattanDistance(GridPos a, GridPos b) {
    return std::abs(a.row - b.row) + std::abs(a.col - b.col);
}

} // namespace jf
