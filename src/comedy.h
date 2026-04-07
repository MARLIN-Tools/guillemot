#pragma once

#include "types.h"
#include "move.h"
#include "position.h"

#include <algorithm>
#include <string>
#include <vector>
#include <random>
#include <array>

namespace guillemot {

using makaira::Position;
using makaira::Square;
using makaira::Color;
using makaira::PieceType;
using makaira::Bitboard;
class ComedyEngine {
public:
    ComedyEngine();
    void set_level(int level) { level_ = std::clamp(level, 1, 7); }
    int level() const { return level_; }
    void set_boss_mode(bool enabled) { boss_mode_ = enabled; }
    bool boss_mode() const { return boss_mode_; }
    int chaos_tier(int eval_cp) const;
    int honest_depth() const;
    int best_move_chance() const;
    std::string generate_boss_move(const Position& pos, int tier);
    std::string comedy_comment(int tier) const;

private:
    enum BossMoveType {
        BOSS_TELEPORT,
        BOSS_DROP,
        BOSS_OFF_RANK_PROMO,
        BOSS_IN_PLACE_PROMO,
        BOSS_STEAL_PIECE,
        BOSS_KIDNAP_KING,
        BOSS_TYPE_COUNT
    };
    std::string teleport_piece(const Position& pos);
    std::string drop_piece(const Position& pos);
    std::string off_rank_promo(const Position& pos, int tier);
    std::string in_place_promo(const Position& pos, int tier);
    std::string steal_piece(const Position& pos);
    std::string kidnap_king(const Position& pos);
    std::vector<Square> find_pieces(const Position& pos, Color c, PieceType pt) const;
    std::vector<Square> find_all_pieces(const Position& pos, Color c) const;
    std::vector<Square> find_empty_squares(const Position& pos) const;
    static std::string sq_to_str(Square sq);
    static char pt_to_char(PieceType pt);
    char promo_piece_for_tier(int tier);
    int rand_int(int lo, int hi);
    bool coin_flip(int pct = 50);

    int level_ = 4;
    bool boss_mode_ = false;
    std::mt19937 rng_;
};

}
