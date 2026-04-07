#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace makaira {

using Bitboard = std::uint64_t;
using Key = std::uint64_t;

enum Color : int {
    WHITE = 0,
    BLACK = 1,
    COLOR_NB = 2
};

constexpr Color operator~(Color c) {
    return static_cast<Color>(c ^ 1);
}

enum PieceType : int {
    NO_PIECE_TYPE = 0,
    PAWN = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK = 4,
    QUEEN = 5,
    KING = 6,
    PIECE_TYPE_NB = 7
};

enum Piece : int {
    NO_PIECE = 0,
    W_PAWN = 1,
    W_KNIGHT = 2,
    W_BISHOP = 3,
    W_ROOK = 4,
    W_QUEEN = 5,
    W_KING = 6,
    B_PAWN = 7,
    B_KNIGHT = 8,
    B_BISHOP = 9,
    B_ROOK = 10,
    B_QUEEN = 11,
    B_KING = 12,
    PIECE_NB = 13
};

enum File : int {
    FILE_A = 0,
    FILE_B = 1,
    FILE_C = 2,
    FILE_D = 3,
    FILE_E = 4,
    FILE_F = 5,
    FILE_G = 6,
    FILE_H = 7,
    FILE_NB = 8
};

enum Rank : int {
    RANK_1 = 0,
    RANK_2 = 1,
    RANK_3 = 2,
    RANK_4 = 3,
    RANK_5 = 4,
    RANK_6 = 5,
    RANK_7 = 6,
    RANK_8 = 7,
    RANK_NB = 8
};

enum Square : int {
    SQ_A1 = 0,
    SQ_B1 = 1,
    SQ_C1 = 2,
    SQ_D1 = 3,
    SQ_E1 = 4,
    SQ_F1 = 5,
    SQ_G1 = 6,
    SQ_H1 = 7,
    SQ_A8 = 56,
    SQ_B8 = 57,
    SQ_C8 = 58,
    SQ_D8 = 59,
    SQ_E8 = 60,
    SQ_F8 = 61,
    SQ_G8 = 62,
    SQ_H8 = 63,
    SQ_NONE = 64,
    SQ_NB = 64
};

enum CastlingRight : int {
    NO_CASTLING = 0,
    WHITE_OO = 1,
    WHITE_OOO = 2,
    BLACK_OO = 4,
    BLACK_OOO = 8,
    CASTLING_NB = 16
};

constexpr PieceType type_of(Piece pc) {
    return pc == NO_PIECE ? NO_PIECE_TYPE : static_cast<PieceType>(((static_cast<int>(pc) - 1) % 6) + 1);
}

constexpr Color color_of(Piece pc) {
    return static_cast<int>(pc) <= static_cast<int>(W_KING) ? WHITE : BLACK;
}

constexpr Piece make_piece(Color c, PieceType pt) {
    if (pt == NO_PIECE_TYPE) {
        return NO_PIECE;
    }
    return static_cast<Piece>((c == WHITE ? 0 : 6) + static_cast<int>(pt));
}

constexpr File file_of(Square sq) {
    return static_cast<File>(static_cast<int>(sq) & 7);
}

constexpr Rank rank_of(Square sq) {
    return static_cast<Rank>(static_cast<int>(sq) >> 3);
}

constexpr Square make_square(File f, Rank r) {
    return static_cast<Square>(static_cast<int>(r) * 8 + static_cast<int>(f));
}

constexpr bool is_ok_square(Square sq) {
    return sq >= SQ_A1 && sq <= SQ_H8;
}

inline std::string square_to_string(Square sq) {
    if (!is_ok_square(sq)) {
        return "--";
    }
    std::string s = "a1";
    s[0] = static_cast<char>('a' + static_cast<int>(file_of(sq)));
    s[1] = static_cast<char>('1' + static_cast<int>(rank_of(sq)));
    return s;
}

inline Square square_from_string(const std::string& s) {
    if (s.size() != 2 || s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') {
        return SQ_NONE;
    }
    return make_square(static_cast<File>(s[0] - 'a'), static_cast<Rank>(s[1] - '1'));
}

}  // namespace makaira