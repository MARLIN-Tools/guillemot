#include "eval_tables.h"

#include "eval_params.h"
#include "eval_params_tuned.h"
#include "eval_psqt_tuned.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace makaira::eval_tables {

std::array<std::array<Score, SQ_NB>, PIECE_NB> PACKED_PSQT{};
std::array<Bitboard, FILE_NB> FILE_MASK{};
std::array<Bitboard, RANK_NB> RANK_MASK{};
std::array<Bitboard, FILE_NB> ADJACENT_FILE_MASK{};
std::array<std::array<Bitboard, SQ_NB>, COLOR_NB> FORWARD_MASK{};
std::array<std::array<Bitboard, SQ_NB>, COLOR_NB> PASSED_MASK{};

namespace {

std::once_flag INIT_FLAG;

int centralization_bonus(Square sq) {
    const int f = static_cast<int>(file_of(sq));
    const int r = static_cast<int>(rank_of(sq));
    const int df = std::abs(2 * f - 7);
    const int dr = std::abs(2 * r - 7);
    return 14 - (df + dr);
}

Score psqt_delta(PieceType pt, Square sq) {
    const int r = static_cast<int>(rank_of(sq));
    const int f = static_cast<int>(file_of(sq));
    const int central = centralization_bonus(sq);

    switch (pt) {
        case PAWN:
            return make_score(r * 6 - std::abs(f - 3) * 2, r * 12 - std::abs(f - 3) * 2);
        case KNIGHT:
            return make_score(central * 2 - (r == RANK_1 ? 8 : 0), central - (r == RANK_1 ? 4 : 0));
        case BISHOP:
            return make_score(central + r * 2, central + r);
        case ROOK:
            return make_score(r * 2 + (f == FILE_D || f == FILE_E ? 6 : 0), r * 3);
        case QUEEN:
            return make_score(central, central / 2 + r);
        case KING:
            return make_score(-central * 2 - r * 8, central * 2 + r * 10);
        default:
            return make_score(0, 0);
    }
}

void init_masks() {
    for (int f = FILE_A; f <= FILE_H; ++f) {
        Bitboard fm = 0;
        for (int r = RANK_1; r <= RANK_8; ++r) {
            fm |= bb_from(make_square(static_cast<File>(f), static_cast<Rank>(r)));
        }
        FILE_MASK[f] = fm;
    }

    for (int r = RANK_1; r <= RANK_8; ++r) {
        Bitboard rm = 0;
        for (int f = FILE_A; f <= FILE_H; ++f) {
            rm |= bb_from(make_square(static_cast<File>(f), static_cast<Rank>(r)));
        }
        RANK_MASK[r] = rm;
    }

    for (int f = FILE_A; f <= FILE_H; ++f) {
        Bitboard adj = 0;
        if (f > FILE_A) {
            adj |= FILE_MASK[f - 1];
        }
        if (f < FILE_H) {
            adj |= FILE_MASK[f + 1];
        }
        ADJACENT_FILE_MASK[f] = adj;
    }

    for (int sq = SQ_A1; sq <= SQ_H8; ++sq) {
        const Square s = static_cast<Square>(sq);
        const int f = static_cast<int>(file_of(s));
        const int r = static_cast<int>(rank_of(s));

        Bitboard white_forward = 0;
        Bitboard black_forward = 0;

        for (int rr = r + 1; rr <= RANK_8; ++rr) {
            white_forward |= bb_from(make_square(static_cast<File>(f), static_cast<Rank>(rr)));
        }
        for (int rr = r - 1; rr >= RANK_1; --rr) {
            black_forward |= bb_from(make_square(static_cast<File>(f), static_cast<Rank>(rr)));
        }

        FORWARD_MASK[WHITE][sq] = white_forward;
        FORWARD_MASK[BLACK][sq] = black_forward;

        Bitboard white_passed = white_forward & (FILE_MASK[f] | ADJACENT_FILE_MASK[f]);
        Bitboard black_passed = black_forward & (FILE_MASK[f] | ADJACENT_FILE_MASK[f]);
        PASSED_MASK[WHITE][sq] = white_passed;
        PASSED_MASK[BLACK][sq] = black_passed;
    }
}

void init_psqt() {
    for (int sq = SQ_A1; sq <= SQ_H8; ++sq) {
        const Square white_sq = static_cast<Square>(sq);
        const Square black_sq = mirror_square(white_sq);

        PACKED_PSQT[NO_PIECE][sq] = make_score(0, 0);

        for (int pt = PAWN; pt <= KING; ++pt) {
            const PieceType ptype = static_cast<PieceType>(pt);
            const Score base = eval_params::PIECE_VALUE[pt];
            const Score w_delta = psqt_delta(ptype, white_sq);
            const Score b_delta = psqt_delta(ptype, black_sq);
            const int w_bucket = psqt_bucket(white_sq, WHITE);
            const int b_bucket = psqt_bucket(static_cast<Square>(sq), BLACK);
            const Score w_tuned = make_score(
              (eval_psqt_tuned::PSQT_BUCKET_MG[pt][w_bucket] * eval_params_tuned::PSQT_BUCKET_MG_SCALE) / 100,
              (eval_psqt_tuned::PSQT_BUCKET_EG[pt][w_bucket] * eval_params_tuned::PSQT_BUCKET_EG_SCALE) / 100);
            const Score b_tuned = make_score(
              (eval_psqt_tuned::PSQT_BUCKET_MG[pt][b_bucket] * eval_params_tuned::PSQT_BUCKET_MG_SCALE) / 100,
              (eval_psqt_tuned::PSQT_BUCKET_EG[pt][b_bucket] * eval_params_tuned::PSQT_BUCKET_EG_SCALE) / 100);

            PACKED_PSQT[make_piece(WHITE, ptype)][sq] =
              make_score(base.mg + w_delta.mg + w_tuned.mg, base.eg + w_delta.eg + w_tuned.eg);
            PACKED_PSQT[make_piece(BLACK, ptype)][sq] =
              make_score(base.mg + b_delta.mg + b_tuned.mg, base.eg + b_delta.eg + b_tuned.eg);
        }
    }
}

}  // namespace

void init_eval_tables() {
    std::call_once(INIT_FLAG, []() {
        init_masks();
        init_psqt();
    });
}

}  // namespace makaira::eval_tables
