#pragma once

#include "move.h"
#include "position.h"

#include <cstdint>

namespace makaira {

struct Score {
    int mg = 0;
    int eg = 0;
};

constexpr Score make_score(int mg, int eg) {
    return Score{mg, eg};
}

constexpr Score operator+(Score a, Score b) {
    return Score{a.mg + b.mg, a.eg + b.eg};
}

constexpr Score operator-(Score a, Score b) {
    return Score{a.mg - b.mg, a.eg - b.eg};
}

constexpr Score& operator+=(Score& a, Score b) {
    a.mg += b.mg;
    a.eg += b.eg;
    return a;
}

constexpr Score operator*(Score a, int k) {
    return Score{a.mg * k, a.eg * k};
}

struct EvalBreakdown {
    Score material_psqt{};
    Score pawns{};
    Score mobility{};
    Score king_safety{};
    Score piece_features{};
    Score threats{};
    Score space{};
    int endgame_scale = 128;
    int tempo = 0;
    int phase = 0;
    int total_white_pov = 0;
};

struct EvalStats {
    std::uint64_t eval_calls = 0;
    std::uint64_t pawn_hash_hits = 0;
    std::uint64_t pawn_hash_misses = 0;
    std::uint64_t eval_ns_total = 0;
    std::uint64_t eval_ns_pawn = 0;
    std::uint64_t eval_ns_attack_maps = 0;
    std::uint64_t eval_ns_mobility = 0;
    std::uint64_t eval_ns_king = 0;
    std::uint64_t eval_ns_threats = 0;
    std::uint64_t eval_ns_space = 0;
};

class IEvaluator {
   public:
    virtual ~IEvaluator() = default;

    virtual int static_eval(const Position& pos) const = 0;
    virtual int static_eval_trace(const Position& pos, EvalBreakdown* out) const;

    // Evaluators that maintain incremental state (e.g. NNUE accumulators)
    // can request make/unmake callbacks in the hot search loop.
    virtual bool requires_move_hooks() const { return false; }

    virtual EvalStats stats() const;
    virtual void clear_stats();
    virtual void set_profile_mode(bool) const {}

    virtual void on_make_move(const Position&, Move) const {}
    virtual void on_unmake_move(const Position&, Move) const {}
};

class MaterialEvaluator final : public IEvaluator {
   public:
    int static_eval(const Position& pos) const override;
};

class HCEEvaluator;

}  // namespace makaira
