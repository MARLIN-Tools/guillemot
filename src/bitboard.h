#pragma once

#include "types.h"

#include <array>
#include <bit>

namespace makaira {

namespace attacks {

extern std::array<std::array<Bitboard, SQ_NB>, COLOR_NB> pawn;
extern std::array<Bitboard, SQ_NB> knight;
extern std::array<Bitboard, SQ_NB> king;

void init();
Bitboard bishop_attacks(Square sq, Bitboard occupancy);
Bitboard rook_attacks(Square sq, Bitboard occupancy);

}  // namespace attacks

constexpr Bitboard bb_from(Square sq) {
    return Bitboard{1} << static_cast<int>(sq);
}

inline int popcount(Bitboard b) {
    return std::popcount(b);
}

inline Square lsb(Bitboard b) {
    return static_cast<Square>(std::countr_zero(b));
}

inline Square pop_lsb(Bitboard& b) {
    const Square sq = lsb(b);
    b &= b - 1;
    return sq;
}

}  // namespace makaira