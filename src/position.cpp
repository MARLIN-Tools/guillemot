#include "position.h"

#include "eval_params.h"
#include "eval_tables.h"
#include "zobrist.h"

#include <array>
#include <cctype>
#include <sstream>
#include <string>

namespace makaira {
namespace {

constexpr std::size_t HISTORY_RESERVE = 1024;

std::array<int, SQ_NB> build_castling_masks() {
    std::array<int, SQ_NB> m{};
    m.fill(WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO);

    m[SQ_E1] &= ~(WHITE_OO | WHITE_OOO);
    m[SQ_H1] &= ~WHITE_OO;
    m[SQ_A1] &= ~WHITE_OOO;

    m[SQ_E8] &= ~(BLACK_OO | BLACK_OOO);
    m[SQ_H8] &= ~BLACK_OO;
    m[SQ_A8] &= ~BLACK_OOO;

    return m;
}

const std::array<int, SQ_NB> CASTLING_MASK = build_castling_masks();

Piece piece_from_fen(char c) {
    switch (c) {
        case 'P': return W_PAWN;
        case 'N': return W_KNIGHT;
        case 'B': return W_BISHOP;
        case 'R': return W_ROOK;
        case 'Q': return W_QUEEN;
        case 'K': return W_KING;
        case 'p': return B_PAWN;
        case 'n': return B_KNIGHT;
        case 'b': return B_BISHOP;
        case 'r': return B_ROOK;
        case 'q': return B_QUEEN;
        case 'k': return B_KING;
        default: return NO_PIECE;
    }
}

}  // namespace

Position::Position() {
    eval_tables::init_eval_tables();
    clear();
}

void Position::clear() {
    for (auto& bb_by_color : piece_bb_) {
        bb_by_color.fill(0);
    }
    occupancy_.fill(0);
    board_.fill(NO_PIECE);
    king_square_.fill(SQ_NONE);
    side_to_move_ = WHITE;
    castling_rights_ = NO_CASTLING;
    ep_square_ = SQ_NONE;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    key_ = 0;
    pawn_key_ = 0;
    mg_psqt_ = {0, 0};
    eg_psqt_ = {0, 0};
    non_pawn_material_ = {0, 0};
    phase_ = 0;
    history_.clear();
    if (history_.capacity() < HISTORY_RESERVE) {
        history_.reserve(HISTORY_RESERVE);
    }
}

bool Position::set_startpos() {
    return set_from_fen(CHESS_STARTPOS_FEN);
}

bool Position::set_from_fen(const std::string& fen) {
    clear();

    std::istringstream iss(fen);
    std::string board_part;
    std::string stm_part;
    std::string castling_part;
    std::string ep_part;

    if (!(iss >> board_part >> stm_part >> castling_part >> ep_part)) {
        return false;
    }

    int rank = 7;
    int file = 0;
    for (char c : board_part) {
        if (c == '/') {
            --rank;
            file = 0;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            file += c - '0';
            continue;
        }

        const Piece pc = piece_from_fen(c);
        if (pc == NO_PIECE || file > 7 || rank < 0) {
            return false;
        }

        const Square sq = make_square(static_cast<File>(file), static_cast<Rank>(rank));
        add_piece(pc, sq);
        ++file;
    }

    if (king_square_[WHITE] == SQ_NONE || king_square_[BLACK] == SQ_NONE) {
        return false;
    }

    if (stm_part == "w") {
        side_to_move_ = WHITE;
    } else if (stm_part == "b") {
        side_to_move_ = BLACK;
    } else {
        return false;
    }

    castling_rights_ = NO_CASTLING;
    if (castling_part != "-") {
        for (char c : castling_part) {
            switch (c) {
                case 'K': castling_rights_ |= WHITE_OO; break;
                case 'Q': castling_rights_ |= WHITE_OOO; break;
                case 'k': castling_rights_ |= BLACK_OO; break;
                case 'q': castling_rights_ |= BLACK_OOO; break;
                default: return false;
            }
        }
    }

    if (ep_part == "-") {
        ep_square_ = SQ_NONE;
    } else {
        ep_square_ = square_from_string(ep_part);
        if (!is_ok_square(ep_square_)) {
            return false;
        }
    }

    if (!(iss >> halfmove_clock_)) {
        halfmove_clock_ = 0;
    }
    if (!(iss >> fullmove_number_)) {
        fullmove_number_ = 1;
    }

    key_ = compute_full_key();
    history_.clear();
    return true;
}

void Position::add_piece(Piece pc, Square sq) {
    const Color c = color_of(pc);
    const PieceType pt = type_of(pc);
    const Bitboard b = bb_from(sq);

    board_[sq] = pc;
    piece_bb_[c][pt - 1] |= b;
    occupancy_[c] |= b;
    occupancy_[COLOR_NB] |= b;
    key_ ^= Zobrist.piece[pc - 1][sq];
    const Score ps = eval_tables::psqt(pc, sq);
    mg_psqt_[c] += ps.mg;
    eg_psqt_[c] += ps.eg;

    if (pt == PAWN) {
        pawn_key_ ^= Zobrist.piece[pc - 1][sq];
    } else if (pt != KING) {
        non_pawn_material_[c] += eval_params::PIECE_VALUE[pt].mg;
        phase_ += eval_params::PHASE_INC[pt];
    }

    if (pt == KING) {
        king_square_[c] = sq;
    }
}

void Position::remove_piece(Piece pc, Square sq) {
    const Color c = color_of(pc);
    const PieceType pt = type_of(pc);
    const Bitboard b = bb_from(sq);

    board_[sq] = NO_PIECE;
    piece_bb_[c][pt - 1] &= ~b;
    occupancy_[c] &= ~b;
    occupancy_[COLOR_NB] &= ~b;
    key_ ^= Zobrist.piece[pc - 1][sq];
    const Score ps = eval_tables::psqt(pc, sq);
    mg_psqt_[c] -= ps.mg;
    eg_psqt_[c] -= ps.eg;

    if (pt == PAWN) {
        pawn_key_ ^= Zobrist.piece[pc - 1][sq];
    } else if (pt != KING) {
        non_pawn_material_[c] -= eval_params::PIECE_VALUE[pt].mg;
        phase_ -= eval_params::PHASE_INC[pt];
    }
}

void Position::move_piece(Piece pc, Square from, Square to) {
    const Color c = color_of(pc);
    const PieceType pt = type_of(pc);
    const Bitboard from_bb = bb_from(from);
    const Bitboard to_bb = bb_from(to);

    board_[from] = NO_PIECE;
    board_[to] = pc;

    piece_bb_[c][pt - 1] ^= from_bb | to_bb;
    occupancy_[c] ^= from_bb | to_bb;
    occupancy_[COLOR_NB] ^= from_bb | to_bb;

    key_ ^= Zobrist.piece[pc - 1][from];
    key_ ^= Zobrist.piece[pc - 1][to];
    const Score ps_from = eval_tables::psqt(pc, from);
    const Score ps_to = eval_tables::psqt(pc, to);
    mg_psqt_[c] += ps_to.mg - ps_from.mg;
    eg_psqt_[c] += ps_to.eg - ps_from.eg;

    if (pt == PAWN) {
        pawn_key_ ^= Zobrist.piece[pc - 1][from];
        pawn_key_ ^= Zobrist.piece[pc - 1][to];
    }

    if (pt == KING) {
        king_square_[c] = to;
    }
}

bool Position::is_square_attacked(Square sq, Color by) const {
    const Bitboard occ = occupancy_[COLOR_NB];

    const Bitboard pawns = piece_bb_[by][PAWN - 1];
    if ((by == WHITE ? attacks::pawn[BLACK][sq] : attacks::pawn[WHITE][sq]) & pawns) {
        return true;
    }

    if (attacks::knight[sq] & piece_bb_[by][KNIGHT - 1]) {
        return true;
    }

    const Bitboard bishops_queens = piece_bb_[by][BISHOP - 1] | piece_bb_[by][QUEEN - 1];
    if (attacks::bishop_attacks(sq, occ) & bishops_queens) {
        return true;
    }

    const Bitboard rooks_queens = piece_bb_[by][ROOK - 1] | piece_bb_[by][QUEEN - 1];
    if (attacks::rook_attacks(sq, occ) & rooks_queens) {
        return true;
    }

    return (attacks::king[sq] & piece_bb_[by][KING - 1]) != 0;
}

bool Position::in_check(Color c) const {
    return is_square_attacked(king_square_[c], ~c);
}

bool Position::is_repetition() const {
    if (history_.size() < 4) {
        return false;
    }

    const int max_back = halfmove_clock_;
    int steps = 2;
    while (steps <= max_back) {
        const int idx = static_cast<int>(history_.size()) - steps;
        if (idx < 0) {
            break;
        }
        if (history_[idx].key == key_) {
            return true;
        }
        steps += 2;
    }

    return false;
}

bool Position::is_insufficient_material() const {
    const int pawns = popcount(piece_bb_[WHITE][PAWN - 1]) + popcount(piece_bb_[BLACK][PAWN - 1]);
    const int rooks = popcount(piece_bb_[WHITE][ROOK - 1]) + popcount(piece_bb_[BLACK][ROOK - 1]);
    const int queens = popcount(piece_bb_[WHITE][QUEEN - 1]) + popcount(piece_bb_[BLACK][QUEEN - 1]);

    if (pawns != 0 || rooks != 0 || queens != 0) {
        return false;
    }

    const int white_knights = popcount(piece_bb_[WHITE][KNIGHT - 1]);
    const int white_bishops = popcount(piece_bb_[WHITE][BISHOP - 1]);
    const int black_knights = popcount(piece_bb_[BLACK][KNIGHT - 1]);
    const int black_bishops = popcount(piece_bb_[BLACK][BISHOP - 1]);

    const int white_minors = white_knights + white_bishops;
    const int black_minors = black_knights + black_bishops;
    const int total_minors = white_minors + black_minors;

    if (total_minors == 0) {
        return true;
    }
    if (total_minors == 1) {
        return true;
    }

    if (white_knights == 0 && black_knights == 0 && white_bishops == 1 && black_bishops == 1) {
        return true;
    }

    return false;
}

bool Position::is_draw() const {
    return halfmove_clock_ >= 100 || is_repetition() || is_insufficient_material();
}

bool Position::make_move(Move move) {
    const Square from = move.from();
    const Square to = move.to();

    if (!is_ok_square(from) || !is_ok_square(to)) {
        return false;
    }

    const Piece moved = board_[from];
    if (moved == NO_PIECE || color_of(moved) != side_to_move_) {
        return false;
    }

    history_.emplace_back();
    StateInfo& st = history_.back();
    st.key = key_;
    st.move = move;
    st.moved_piece = moved;
    st.captured_piece = NO_PIECE;
    st.castling_rights = castling_rights_;
    st.ep_square = ep_square_;
    st.halfmove_clock = halfmove_clock_;
    st.fullmove_number = fullmove_number_;
    st.pawn_key = pawn_key_;
    st.mg_psqt = mg_psqt_;
    st.eg_psqt = eg_psqt_;
    st.non_pawn_material = non_pawn_material_;
    st.phase = phase_;
    st.is_null = false;

    if (ep_square_ != SQ_NONE) {
        key_ ^= Zobrist.en_passant[file_of(ep_square_)];
    }
    key_ ^= Zobrist.castling[castling_rights_];

    ep_square_ = SQ_NONE;

    Piece captured = NO_PIECE;
    const bool pawn_move = type_of(moved) == PAWN;

    if (move.is_castling()) {
        if (side_to_move_ == WHITE) {
            if (to == SQ_G1) {
                move_piece(W_KING, SQ_E1, SQ_G1);
                move_piece(W_ROOK, SQ_H1, SQ_F1);
            } else {
                move_piece(W_KING, SQ_E1, SQ_C1);
                move_piece(W_ROOK, SQ_A1, SQ_D1);
            }
        } else {
            if (to == SQ_G8) {
                move_piece(B_KING, SQ_E8, SQ_G8);
                move_piece(B_ROOK, SQ_H8, SQ_F8);
            } else {
                move_piece(B_KING, SQ_E8, SQ_C8);
                move_piece(B_ROOK, SQ_A8, SQ_D8);
            }
        }
    } else {
        if (move.is_en_passant()) {
            const Square cap_sq = side_to_move_ == WHITE ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
            captured = board_[cap_sq];
            if (captured == NO_PIECE) {
                unmake_move();
                return false;
            }
            remove_piece(captured, cap_sq);
        } else if (move.is_capture()) {
            captured = board_[to];
            if (captured == NO_PIECE) {
                unmake_move();
                return false;
            }
            remove_piece(captured, to);
        }

        if (move.is_promotion()) {
            remove_piece(moved, from);
            add_piece(make_piece(side_to_move_, move.promotion()), to);
        } else {
            move_piece(moved, from, to);
        }

        if (move.is_double_pawn_push()) {
            ep_square_ = side_to_move_ == WHITE ? static_cast<Square>(from + 8) : static_cast<Square>(from - 8);
        }
    }

    history_.back().captured_piece = captured;

    halfmove_clock_ = (pawn_move || captured != NO_PIECE) ? 0 : (halfmove_clock_ + 1);
    if (side_to_move_ == BLACK) {
        ++fullmove_number_;
    }

    castling_rights_ &= CASTLING_MASK[from];
    castling_rights_ &= CASTLING_MASK[to];

    key_ ^= Zobrist.castling[castling_rights_];
    if (ep_square_ != SQ_NONE) {
        key_ ^= Zobrist.en_passant[file_of(ep_square_)];
    }

    side_to_move_ = ~side_to_move_;
    key_ ^= Zobrist.side;

    const Color us = ~side_to_move_;
    if (in_check(us)) {
        unmake_move();
        return false;
    }

    return true;
}

void Position::unmake_move() {
    if (history_.empty()) {
        return;
    }

    const StateInfo& st = history_.back();

    if (st.is_null) {
        side_to_move_ = ~side_to_move_;
        castling_rights_ = st.castling_rights;
        ep_square_ = st.ep_square;
        halfmove_clock_ = st.halfmove_clock;
        fullmove_number_ = st.fullmove_number;
        key_ = st.key;
        pawn_key_ = st.pawn_key;
        mg_psqt_ = st.mg_psqt;
        eg_psqt_ = st.eg_psqt;
        non_pawn_material_ = st.non_pawn_material;
        phase_ = st.phase;
        history_.pop_back();
        return;
    }

    const Move move = st.move;
    const Square from = move.from();
    const Square to = move.to();

    const Color mover = ~side_to_move_;
    side_to_move_ = mover;
    castling_rights_ = st.castling_rights;
    ep_square_ = st.ep_square;
    halfmove_clock_ = st.halfmove_clock;
    fullmove_number_ = st.fullmove_number;

    if (move.is_castling()) {
        if (mover == WHITE) {
            if (to == SQ_G1) {
                move_piece(W_KING, SQ_G1, SQ_E1);
                move_piece(W_ROOK, SQ_F1, SQ_H1);
            } else {
                move_piece(W_KING, SQ_C1, SQ_E1);
                move_piece(W_ROOK, SQ_D1, SQ_A1);
            }
        } else {
            if (to == SQ_G8) {
                move_piece(B_KING, SQ_G8, SQ_E8);
                move_piece(B_ROOK, SQ_F8, SQ_H8);
            } else {
                move_piece(B_KING, SQ_C8, SQ_E8);
                move_piece(B_ROOK, SQ_D8, SQ_A8);
            }
        }
    } else {
        if (move.is_promotion()) {
            remove_piece(board_[to], to);
            add_piece(make_piece(mover, PAWN), from);
        } else {
            move_piece(st.moved_piece, to, from);
        }

        if (st.captured_piece != NO_PIECE) {
            if (move.is_en_passant()) {
                const Square cap_sq = mover == WHITE ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
                add_piece(st.captured_piece, cap_sq);
            } else {
                add_piece(st.captured_piece, to);
            }
        }
    }

    key_ = st.key;
    pawn_key_ = st.pawn_key;
    mg_psqt_ = st.mg_psqt;
    eg_psqt_ = st.eg_psqt;
    non_pawn_material_ = st.non_pawn_material;
    phase_ = st.phase;

    history_.pop_back();
}

void Position::make_null_move() {
    history_.emplace_back();
    StateInfo& st = history_.back();
    st.key = key_;
    st.move = Move{};
    st.moved_piece = NO_PIECE;
    st.captured_piece = NO_PIECE;
    st.castling_rights = castling_rights_;
    st.ep_square = ep_square_;
    st.halfmove_clock = halfmove_clock_;
    st.fullmove_number = fullmove_number_;
    st.pawn_key = pawn_key_;
    st.mg_psqt = mg_psqt_;
    st.eg_psqt = eg_psqt_;
    st.non_pawn_material = non_pawn_material_;
    st.phase = phase_;
    st.is_null = true;

    if (ep_square_ != SQ_NONE) {
        key_ ^= Zobrist.en_passant[file_of(ep_square_)];
        ep_square_ = SQ_NONE;
    }

    key_ ^= Zobrist.side;
    side_to_move_ = ~side_to_move_;
    ++halfmove_clock_;
    if (side_to_move_ == WHITE) {
        ++fullmove_number_;
    }
}

void Position::unmake_null_move() {
    if (history_.empty()) {
        return;
    }

    const StateInfo& st = history_.back();
    if (!st.is_null) {
        return;
    }

    side_to_move_ = ~side_to_move_;
    castling_rights_ = st.castling_rights;
    ep_square_ = st.ep_square;
    halfmove_clock_ = st.halfmove_clock;
    fullmove_number_ = st.fullmove_number;
    key_ = st.key;
    pawn_key_ = st.pawn_key;
    mg_psqt_ = st.mg_psqt;
    eg_psqt_ = st.eg_psqt;
    non_pawn_material_ = st.non_pawn_material;
    phase_ = st.phase;
    history_.pop_back();
}

void Position::update_occupancy_cache() {
    occupancy_[WHITE] = occupancy_[BLACK] = 0;
    for (int pt = PAWN; pt <= KING; ++pt) {
        occupancy_[WHITE] |= piece_bb_[WHITE][pt - 1];
        occupancy_[BLACK] |= piece_bb_[BLACK][pt - 1];
    }
    occupancy_[COLOR_NB] = occupancy_[WHITE] | occupancy_[BLACK];
}

Key Position::compute_full_key() const {
    Key key = 0;

    for (int sq = SQ_A1; sq <= SQ_H8; ++sq) {
        const Piece pc = board_[sq];
        if (pc != NO_PIECE) {
            key ^= Zobrist.piece[pc - 1][sq];
        }
    }

    key ^= Zobrist.castling[castling_rights_];
    if (ep_square_ != SQ_NONE) {
        key ^= Zobrist.en_passant[file_of(ep_square_)];
    }
    if (side_to_move_ == BLACK) {
        key ^= Zobrist.side;
    }

    return key;
}

}  // namespace makaira
