#pragma once

#include "types.h"

#include <array>

namespace makaira {

struct ZobristKeys {
    std::array<std::array<Key, SQ_NB>, 12> piece{};
    std::array<Key, CASTLING_NB> castling{};
    std::array<Key, FILE_NB> en_passant{};
    std::array<std::array<Key, FILE_NB>, COLOR_NB> pawn_file_king{};
    Key side = 0;
};

extern ZobristKeys Zobrist;

void init_zobrist();

}  // namespace makaira
