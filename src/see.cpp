#include "see.h"

#include "bitboard.h"
#include "eval_params.h"

#include <algorithm>
#include <array>

namespace makaira {
namespace {

constexpr std::array<int, PIECE_TYPE_NB> SEE_VALUE = {
  0,
  100,
  320,
  330,
  500,
  900,
  10000,
};

inline int piece_value(PieceType pt) {
    return SEE_VALUE[static_cast<int>(pt)];
}

Bitboard attackers_to(const Position& pos, Square sq, Bitboard occ) {
    Bitboard at = 0;
    at |= attacks::pawn[BLACK][sq] & pos.pieces(WHITE, PAWN);
    at |= attacks::pawn[WHITE][sq] & pos.pieces(BLACK, PAWN);
    at |= attacks::knight[sq] & (pos.pieces(WHITE, KNIGHT) | pos.pieces(BLACK, KNIGHT));
    at |= attacks::bishop_attacks(sq, occ)
          & (pos.pieces(WHITE, BISHOP) | pos.pieces(BLACK, BISHOP) | pos.pieces(WHITE, QUEEN) | pos.pieces(BLACK, QUEEN));
    at |= attacks::rook_attacks(sq, occ)
          & (pos.pieces(WHITE, ROOK) | pos.pieces(BLACK, ROOK) | pos.pieces(WHITE, QUEEN) | pos.pieces(BLACK, QUEEN));
    at |= attacks::king[sq] & (pos.pieces(WHITE, KING) | pos.pieces(BLACK, KING));
    return at;
}

}  // namespace

int see_captured_value(const Position& pos, Move move) {
    Piece captured = NO_PIECE;
    if (move.is_en_passant()) {
        captured = make_piece(~pos.side_to_move(), PAWN);
    } else {
        captured = pos.piece_on(move.to());
    }
    return captured == NO_PIECE ? 0 : piece_value(type_of(captured));
}

int static_exchange_eval(const Position& pos, Move move) {
    if (move.is_none()) {
        return 0;
    }

    const Square from = move.from();
    const Square to = move.to();
    const Piece moved = pos.piece_on(from);
    if (moved == NO_PIECE) {
        return 0;
    }

    int gain[32] = {0};
    gain[0] = see_captured_value(pos, move);

    Bitboard occ = pos.occupancy();
    Bitboard occ_side[COLOR_NB] = {pos.occupancy(WHITE), pos.occupancy(BLACK)};
    const Color us = pos.side_to_move();
    const Color them = ~us;

    const Bitboard from_bb = bb_from(from);
    occ ^= from_bb;
    occ_side[us] ^= from_bb;
    if (move.is_en_passant()) {
        const Square cap_sq = us == WHITE ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
        const Bitboard cap_bb = bb_from(cap_sq);
        occ ^= cap_bb;
        occ_side[them] ^= cap_bb;
    } else {
        const Piece cap = pos.piece_on(to);
        if (cap != NO_PIECE) {
            occ_side[them] ^= bb_from(to);
        }
    }
    occ |= bb_from(to);
    occ_side[us] |= bb_from(to);

    PieceType last = type_of(moved);
    Color side = them;
    int d = 0;

    while (true) {
        Bitboard at = attackers_to(pos, to, occ) & occ_side[side];
        if (!at) {
            break;
        }

        Square from_sq = SQ_NONE;
        PieceType pt = NO_PIECE_TYPE;
        for (PieceType cand : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            Bitboard bb = at & pos.pieces(side, cand);
            if (bb) {
                from_sq = lsb(bb);
                pt = cand;
                break;
            }
        }
        if (!is_ok_square(from_sq) || pt == NO_PIECE_TYPE) {
            break;
        }

        ++d;
        gain[d] = piece_value(last) - gain[d - 1];
        if (std::max(-gain[d - 1], gain[d]) < 0) {
            break;
        }

        const Bitboard bb = bb_from(from_sq);
        occ ^= bb;
        occ_side[side] ^= bb;
        last = pt;
        side = ~side;
    }

    while (--d > 0) {
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
    }
    return gain[0];
}

}  // namespace makaira

