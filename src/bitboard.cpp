#include "bitboard.h"

namespace makaira {
namespace {

constexpr bool on_board(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

Bitboard slider_attacks(Square sq, Bitboard occupancy, const int* dfile, const int* drank, int n) {
    Bitboard attacks = 0;
    const int f0 = static_cast<int>(file_of(sq));
    const int r0 = static_cast<int>(rank_of(sq));

    for (int i = 0; i < n; ++i) {
        int f = f0 + dfile[i];
        int r = r0 + drank[i];
        while (on_board(f, r)) {
            const Square to = make_square(static_cast<File>(f), static_cast<Rank>(r));
            attacks |= bb_from(to);
            if (occupancy & bb_from(to)) {
                break;
            }
            f += dfile[i];
            r += drank[i];
        }
    }

    return attacks;
}

}  // namespace

namespace attacks {

std::array<std::array<Bitboard, SQ_NB>, COLOR_NB> pawn{};
std::array<Bitboard, SQ_NB> knight{};
std::array<Bitboard, SQ_NB> king{};

void init() {
    for (int sq = SQ_A1; sq <= SQ_H8; ++sq) {
        const Square s = static_cast<Square>(sq);
        const int file = static_cast<int>(file_of(s));
        const int rank = static_cast<int>(rank_of(s));

        Bitboard white_pawn = 0;
        Bitboard black_pawn = 0;
        if (on_board(file - 1, rank + 1)) {
            white_pawn |= bb_from(make_square(static_cast<File>(file - 1), static_cast<Rank>(rank + 1)));
        }
        if (on_board(file + 1, rank + 1)) {
            white_pawn |= bb_from(make_square(static_cast<File>(file + 1), static_cast<Rank>(rank + 1)));
        }
        if (on_board(file - 1, rank - 1)) {
            black_pawn |= bb_from(make_square(static_cast<File>(file - 1), static_cast<Rank>(rank - 1)));
        }
        if (on_board(file + 1, rank - 1)) {
            black_pawn |= bb_from(make_square(static_cast<File>(file + 1), static_cast<Rank>(rank - 1)));
        }
        pawn[WHITE][sq] = white_pawn;
        pawn[BLACK][sq] = black_pawn;

        Bitboard knight_attacks = 0;
        static constexpr int knight_df[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
        static constexpr int knight_dr[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
        for (int i = 0; i < 8; ++i) {
            const int nf = file + knight_df[i];
            const int nr = rank + knight_dr[i];
            if (on_board(nf, nr)) {
                knight_attacks |= bb_from(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
            }
        }
        knight[sq] = knight_attacks;

        Bitboard king_attacks = 0;
        for (int df = -1; df <= 1; ++df) {
            for (int dr = -1; dr <= 1; ++dr) {
                if (df == 0 && dr == 0) {
                    continue;
                }
                const int nf = file + df;
                const int nr = rank + dr;
                if (on_board(nf, nr)) {
                    king_attacks |= bb_from(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
                }
            }
        }
        king[sq] = king_attacks;
    }
}

Bitboard bishop_attacks(Square sq, Bitboard occupancy) {
    static constexpr int df[4] = {1, 1, -1, -1};
    static constexpr int dr[4] = {1, -1, 1, -1};
    return slider_attacks(sq, occupancy, df, dr, 4);
}

Bitboard rook_attacks(Square sq, Bitboard occupancy) {
    static constexpr int df[4] = {1, -1, 0, 0};
    static constexpr int dr[4] = {0, 0, 1, -1};
    return slider_attacks(sq, occupancy, df, dr, 4);
}

}  // namespace attacks
}  // namespace makaira