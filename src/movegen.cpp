#include "movegen.h"

#include <array>

namespace makaira {
namespace {

void push_promotion_moves(MoveList& out, Square from, Square to, std::uint8_t flags) {
    out.push(Move(from, to, flags, QUEEN));
    out.push(Move(from, to, flags, ROOK));
    out.push(Move(from, to, flags, BISHOP));
    out.push(Move(from, to, flags, KNIGHT));
}

}  // namespace

void generate_pseudo_legal(const Position& pos, MoveList& out) {
    out.clear();

    const Color us = pos.side_to_move();
    const Color them = ~us;
    const Bitboard own_occ = pos.occupancy(us);
    const Bitboard opp_occ = pos.occupancy(them);
    const Bitboard all_occ = pos.occupancy();

    Bitboard pawns = pos.pieces(us, PAWN);
    while (pawns) {
        const Square from = pop_lsb(pawns);
        const int f = static_cast<int>(file_of(from));
        const int r = static_cast<int>(rank_of(from));

        if (us == WHITE) {
            const Square one = static_cast<Square>(from + 8);
            if (one <= SQ_H8 && (bb_from(one) & all_occ) == 0) {
                if (r == RANK_7) {
                    push_promotion_moves(out, from, one, FLAG_NONE);
                } else {
                    out.push(Move(from, one, FLAG_NONE));
                    if (r == RANK_2) {
                        const Square two = static_cast<Square>(from + 16);
                        if ((bb_from(two) & all_occ) == 0) {
                            out.push(Move(from, two, FLAG_DOUBLE_PAWN));
                        }
                    }
                }
            }

            if (f > FILE_A) {
                const Square to = static_cast<Square>(from + 7);
                if (to <= SQ_H8 && (bb_from(to) & opp_occ) != 0) {
                    if (r == RANK_7) {
                        push_promotion_moves(out, from, to, FLAG_CAPTURE);
                    } else {
                        out.push(Move(from, to, FLAG_CAPTURE));
                    }
                }
                if (to == pos.ep_square()) {
                    out.push(Move(from, to, FLAG_CAPTURE | FLAG_EN_PASSANT));
                }
            }
            if (f < FILE_H) {
                const Square to = static_cast<Square>(from + 9);
                if (to <= SQ_H8 && (bb_from(to) & opp_occ) != 0) {
                    if (r == RANK_7) {
                        push_promotion_moves(out, from, to, FLAG_CAPTURE);
                    } else {
                        out.push(Move(from, to, FLAG_CAPTURE));
                    }
                }
                if (to == pos.ep_square()) {
                    out.push(Move(from, to, FLAG_CAPTURE | FLAG_EN_PASSANT));
                }
            }
        } else {
            const Square one = static_cast<Square>(from - 8);
            if (one >= SQ_A1 && (bb_from(one) & all_occ) == 0) {
                if (r == RANK_2) {
                    push_promotion_moves(out, from, one, FLAG_NONE);
                } else {
                    out.push(Move(from, one, FLAG_NONE));
                    if (r == RANK_7) {
                        const Square two = static_cast<Square>(from - 16);
                        if ((bb_from(two) & all_occ) == 0) {
                            out.push(Move(from, two, FLAG_DOUBLE_PAWN));
                        }
                    }
                }
            }

            if (f > FILE_A) {
                const Square to = static_cast<Square>(from - 9);
                if (to >= SQ_A1 && (bb_from(to) & opp_occ) != 0) {
                    if (r == RANK_2) {
                        push_promotion_moves(out, from, to, FLAG_CAPTURE);
                    } else {
                        out.push(Move(from, to, FLAG_CAPTURE));
                    }
                }
                if (to == pos.ep_square()) {
                    out.push(Move(from, to, FLAG_CAPTURE | FLAG_EN_PASSANT));
                }
            }
            if (f < FILE_H) {
                const Square to = static_cast<Square>(from - 7);
                if (to >= SQ_A1 && (bb_from(to) & opp_occ) != 0) {
                    if (r == RANK_2) {
                        push_promotion_moves(out, from, to, FLAG_CAPTURE);
                    } else {
                        out.push(Move(from, to, FLAG_CAPTURE));
                    }
                }
                if (to == pos.ep_square()) {
                    out.push(Move(from, to, FLAG_CAPTURE | FLAG_EN_PASSANT));
                }
            }
        }
    }

    Bitboard knights = pos.pieces(us, KNIGHT);
    while (knights) {
        const Square from = pop_lsb(knights);
        Bitboard targets = attacks::knight[from] & ~own_occ;
        while (targets) {
            const Square to = pop_lsb(targets);
            const std::uint8_t flags = (bb_from(to) & opp_occ) ? FLAG_CAPTURE : FLAG_NONE;
            out.push(Move(from, to, flags));
        }
    }

    Bitboard bishops = pos.pieces(us, BISHOP);
    while (bishops) {
        const Square from = pop_lsb(bishops);
        Bitboard targets = attacks::bishop_attacks(from, all_occ) & ~own_occ;
        while (targets) {
            const Square to = pop_lsb(targets);
            const std::uint8_t flags = (bb_from(to) & opp_occ) ? FLAG_CAPTURE : FLAG_NONE;
            out.push(Move(from, to, flags));
        }
    }

    Bitboard rooks = pos.pieces(us, ROOK);
    while (rooks) {
        const Square from = pop_lsb(rooks);
        Bitboard targets = attacks::rook_attacks(from, all_occ) & ~own_occ;
        while (targets) {
            const Square to = pop_lsb(targets);
            const std::uint8_t flags = (bb_from(to) & opp_occ) ? FLAG_CAPTURE : FLAG_NONE;
            out.push(Move(from, to, flags));
        }
    }

    Bitboard queens = pos.pieces(us, QUEEN);
    while (queens) {
        const Square from = pop_lsb(queens);
        Bitboard targets = (attacks::bishop_attacks(from, all_occ) | attacks::rook_attacks(from, all_occ)) & ~own_occ;
        while (targets) {
            const Square to = pop_lsb(targets);
            const std::uint8_t flags = (bb_from(to) & opp_occ) ? FLAG_CAPTURE : FLAG_NONE;
            out.push(Move(from, to, flags));
        }
    }

    const Square ksq = pos.king_square(us);
    Bitboard king_targets = attacks::king[ksq] & ~own_occ;
    while (king_targets) {
        const Square to = pop_lsb(king_targets);
        const std::uint8_t flags = (bb_from(to) & opp_occ) ? FLAG_CAPTURE : FLAG_NONE;
        out.push(Move(ksq, to, flags));
    }

    if (us == WHITE && ksq == SQ_E1) {
        if ((pos.castling_rights() & WHITE_OO)
            && pos.piece_on(SQ_H1) == W_ROOK
            && pos.piece_on(SQ_F1) == NO_PIECE
            && pos.piece_on(SQ_G1) == NO_PIECE
            && !pos.is_square_attacked(SQ_E1, them)
            && !pos.is_square_attacked(SQ_F1, them)
            && !pos.is_square_attacked(SQ_G1, them)) {
            out.push(Move(SQ_E1, SQ_G1, FLAG_CASTLING));
        }
        if ((pos.castling_rights() & WHITE_OOO)
            && pos.piece_on(SQ_A1) == W_ROOK
            && pos.piece_on(SQ_D1) == NO_PIECE
            && pos.piece_on(SQ_C1) == NO_PIECE
            && pos.piece_on(SQ_B1) == NO_PIECE
            && !pos.is_square_attacked(SQ_E1, them)
            && !pos.is_square_attacked(SQ_D1, them)
            && !pos.is_square_attacked(SQ_C1, them)) {
            out.push(Move(SQ_E1, SQ_C1, FLAG_CASTLING));
        }
    }

    if (us == BLACK && ksq == SQ_E8) {
        if ((pos.castling_rights() & BLACK_OO)
            && pos.piece_on(SQ_H8) == B_ROOK
            && pos.piece_on(SQ_F8) == NO_PIECE
            && pos.piece_on(SQ_G8) == NO_PIECE
            && !pos.is_square_attacked(SQ_E8, them)
            && !pos.is_square_attacked(SQ_F8, them)
            && !pos.is_square_attacked(SQ_G8, them)) {
            out.push(Move(SQ_E8, SQ_G8, FLAG_CASTLING));
        }
        if ((pos.castling_rights() & BLACK_OOO)
            && pos.piece_on(SQ_A8) == B_ROOK
            && pos.piece_on(SQ_D8) == NO_PIECE
            && pos.piece_on(SQ_C8) == NO_PIECE
            && pos.piece_on(SQ_B8) == NO_PIECE
            && !pos.is_square_attacked(SQ_E8, them)
            && !pos.is_square_attacked(SQ_D8, them)
            && !pos.is_square_attacked(SQ_C8, them)) {
            out.push(Move(SQ_E8, SQ_C8, FLAG_CASTLING));
        }
    }
}

void generate_legal(Position& pos, MoveList& out) {
    MoveList pseudo;
    generate_pseudo_legal(pos, pseudo);

    out.clear();
    for (int i = 0; i < pseudo.count; ++i) {
        const Move m = pseudo[i];
        if (pos.make_move(m)) {
            out.push(m);
            pos.unmake_move();
        }
    }
}

Move parse_uci_move(Position& pos, const std::string& uci) {
    if (uci.size() < 4) {
        return Move{};
    }

    const Square from = square_from_string(uci.substr(0, 2));
    const Square to = square_from_string(uci.substr(2, 2));
    if (!is_ok_square(from) || !is_ok_square(to)) {
        return Move{};
    }

    PieceType promo = NO_PIECE_TYPE;
    if (uci.size() >= 5) {
        switch (uci[4]) {
            case 'q': promo = QUEEN; break;
            case 'r': promo = ROOK; break;
            case 'b': promo = BISHOP; break;
            case 'n': promo = KNIGHT; break;
            default: return Move{};
        }
    }

    MoveList legal;
    generate_legal(pos, legal);
    for (int i = 0; i < legal.count; ++i) {
        const Move m = legal[i];
        if (m.from() == from && m.to() == to && m.promotion() == promo) {
            return m;
        }
    }

    return Move{};
}

std::string move_to_uci(Move move) {
    if (move.is_none()) {
        return "0000";
    }

    std::string s;
    s.reserve(5);
    s += square_to_string(move.from());
    s += square_to_string(move.to());

    if (move.is_promotion()) {
        char c = 'q';
        switch (move.promotion()) {
            case KNIGHT: c = 'n'; break;
            case BISHOP: c = 'b'; break;
            case ROOK: c = 'r'; break;
            case QUEEN: c = 'q'; break;
            default: break;
        }
        s.push_back(c);
    }

    return s;
}

}  // namespace makaira
