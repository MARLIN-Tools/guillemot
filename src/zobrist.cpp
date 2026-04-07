#include "zobrist.h"

namespace makaira {

ZobristKeys Zobrist;

namespace {

std::uint64_t splitmix64(std::uint64_t& x) {
    x += 0x9e3779b97f4a7c15ULL;
    std::uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

}  // namespace

void init_zobrist() {
    std::uint64_t seed = 0x93f0d4f6ac8e21b7ULL;

    for (int pc = 0; pc < 12; ++pc) {
        for (int sq = 0; sq < SQ_NB; ++sq) {
            Zobrist.piece[pc][sq] = splitmix64(seed);
        }
    }

    for (int i = 0; i < CASTLING_NB; ++i) {
        Zobrist.castling[i] = splitmix64(seed);
    }

    for (int f = 0; f < FILE_NB; ++f) {
        Zobrist.en_passant[f] = splitmix64(seed);
    }

    for (int c = 0; c < COLOR_NB; ++c) {
        for (int f = 0; f < FILE_NB; ++f) {
            Zobrist.pawn_file_king[c][f] = splitmix64(seed);
        }
    }

    Zobrist.side = splitmix64(seed);
}

}  // namespace makaira
