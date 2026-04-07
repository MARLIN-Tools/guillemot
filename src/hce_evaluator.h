#pragma once

#include "evaluator.h"
#include "pawn_hash.h"

namespace makaira {

class HCEEvaluator final : public IEvaluator {
   public:
    HCEEvaluator();

    int static_eval(const Position& pos) const override;
    int static_eval_trace(const Position& pos, EvalBreakdown* out) const override;

    EvalStats stats() const override;
    void clear_stats() override;
    void set_profile_mode(bool enabled) const override;

    int static_eval_recompute(const Position& pos) const;

   private:
    struct AttackInfo {
        std::array<Bitboard, COLOR_NB> pawn_attacks{{0, 0}};
        std::array<Bitboard, COLOR_NB> all_attacks{{0, 0}};
        std::array<int, COLOR_NB> king_attack_units{{0, 0}};
        std::array<int, COLOR_NB> king_attackers{{0, 0}};
        Score mobility{};
    };

    int evaluate(const Position& pos, bool use_incremental, EvalBreakdown* out) const;

    Score evaluate_material_psqt(const Position& pos, bool use_incremental) const;
    PawnHashEntry compute_pawn_entry(const Position& pos, Key pawn_key_with_kings) const;
    Score evaluate_piece_features(const Position& pos, const AttackInfo& ai) const;
    Score evaluate_threats(const Position& pos, const AttackInfo& ai) const;
    Score evaluate_space(const Position& pos, const AttackInfo& ai) const;
    Score evaluate_endgame_terms(const Position& pos) const;
    int evaluate_endgame_scale(const Position& pos, int blended_white_pov) const;

    AttackInfo build_attack_info(const Position& pos) const;

    mutable EvalStats stats_{};
    mutable PawnHashTable pawn_hash_;
    mutable bool profile_mode_ = false;
};

}  // namespace makaira
