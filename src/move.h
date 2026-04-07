#pragma once

#include "types.h"

#include <array>
#include <cstdint>

namespace makaira {

enum MoveFlag : std::uint8_t {
    FLAG_NONE = 0,
    FLAG_CAPTURE = 1 << 0,
    FLAG_DOUBLE_PAWN = 1 << 1,
    FLAG_EN_PASSANT = 1 << 2,
    FLAG_CASTLING = 1 << 3
};

class Move {
   public:
    constexpr Move() = default;

    constexpr Move(Square from, Square to, std::uint8_t flags = FLAG_NONE, PieceType promotion = NO_PIECE_TYPE) :
        data_(static_cast<std::uint32_t>(from)
              | (static_cast<std::uint32_t>(to) << 6)
              | (static_cast<std::uint32_t>(promotion) << 12)
              | (static_cast<std::uint32_t>(flags) << 16)) {}

    constexpr Square from() const { return static_cast<Square>(data_ & 0x3F); }
    constexpr Square to() const { return static_cast<Square>((data_ >> 6) & 0x3F); }
    constexpr PieceType promotion() const { return static_cast<PieceType>((data_ >> 12) & 0x0F); }
    constexpr std::uint8_t flags() const { return static_cast<std::uint8_t>((data_ >> 16) & 0xFF); }

    constexpr bool is_none() const { return data_ == 0; }
    constexpr bool is_capture() const { return (flags() & FLAG_CAPTURE) != 0; }
    constexpr bool is_double_pawn_push() const { return (flags() & FLAG_DOUBLE_PAWN) != 0; }
    constexpr bool is_en_passant() const { return (flags() & FLAG_EN_PASSANT) != 0; }
    constexpr bool is_castling() const { return (flags() & FLAG_CASTLING) != 0; }
    constexpr bool is_promotion() const { return promotion() != NO_PIECE_TYPE; }

    constexpr std::uint32_t raw() const { return data_; }

    friend constexpr bool operator==(Move a, Move b) { return a.data_ == b.data_; }
    friend constexpr bool operator!=(Move a, Move b) { return a.data_ != b.data_; }

   private:
    std::uint32_t data_ = 0;
};

struct MoveList {
    std::array<Move, 256> moves{};
    int count = 0;

    void clear() { count = 0; }

    void push(Move m) {
        moves[count++] = m;
    }

    Move operator[](int idx) const { return moves[idx]; }
};

}  // namespace makaira