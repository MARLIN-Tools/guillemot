#pragma once

#include "bitboard.h"
#include "move.h"
#include "types.h"

#include <array>
#include <string>
#include <vector>

namespace makaira {

constexpr const char* CHESS_STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct StateInfo {
    Key key = 0;
    Key pawn_key = 0;
    Move move{};
    Piece moved_piece = NO_PIECE;
    Piece captured_piece = NO_PIECE;
    int castling_rights = NO_CASTLING;
    Square ep_square = SQ_NONE;
    int halfmove_clock = 0;
    int fullmove_number = 1;
    std::array<int, COLOR_NB> mg_psqt{{0, 0}};
    std::array<int, COLOR_NB> eg_psqt{{0, 0}};
    std::array<int, COLOR_NB> non_pawn_material{{0, 0}};
    int phase = 0;
    bool is_null = false;
};

class Position {
   public:
    Position();

    void clear();
    bool set_from_fen(const std::string& fen);
    bool set_startpos();

    Color side_to_move() const { return side_to_move_; }
    int castling_rights() const { return castling_rights_; }
    Square ep_square() const { return ep_square_; }
    int halfmove_clock() const { return halfmove_clock_; }
    int fullmove_number() const { return fullmove_number_; }
    Key key() const { return key_; }
    Key pawn_key() const { return pawn_key_; }

    Piece piece_on(Square sq) const { return board_[sq]; }
    Square king_square(Color c) const { return king_square_[c]; }

    Bitboard pieces(Color c, PieceType pt) const { return piece_bb_[c][pt - 1]; }
    Bitboard occupancy(Color c) const { return occupancy_[c]; }
    Bitboard occupancy() const { return occupancy_[COLOR_NB]; }
    int mg_psqt(Color c) const { return mg_psqt_[c]; }
    int eg_psqt(Color c) const { return eg_psqt_[c]; }
    int non_pawn_material(Color c) const { return non_pawn_material_[c]; }
    int phase() const { return phase_; }

    bool make_move(Move move);
    void unmake_move();
    void make_null_move();
    void unmake_null_move();

    bool is_square_attacked(Square sq, Color by) const;
    bool in_check(Color c) const;
    bool is_repetition() const;
    bool is_insufficient_material() const;
    bool is_draw() const;

    const std::vector<StateInfo>& history() const { return history_; }

   private:
    void add_piece(Piece pc, Square sq);
    void remove_piece(Piece pc, Square sq);
    void move_piece(Piece pc, Square from, Square to);

    void update_occupancy_cache();
    Key compute_full_key() const;

    std::array<std::array<Bitboard, 6>, COLOR_NB> piece_bb_{};
    std::array<Bitboard, COLOR_NB + 1> occupancy_{};
    std::array<Piece, SQ_NB> board_{};
    std::array<Square, COLOR_NB> king_square_{SQ_NONE, SQ_NONE};

    Color side_to_move_ = WHITE;
    int castling_rights_ = NO_CASTLING;
    Square ep_square_ = SQ_NONE;
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
    Key key_ = 0;
    Key pawn_key_ = 0;
    std::array<int, COLOR_NB> mg_psqt_{{0, 0}};
    std::array<int, COLOR_NB> eg_psqt_{{0, 0}};
    std::array<int, COLOR_NB> non_pawn_material_{{0, 0}};
    int phase_ = 0;

    std::vector<StateInfo> history_{};
};

}  // namespace makaira
