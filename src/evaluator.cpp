#include "evaluator.h"

#include <array>

namespace makaira {

int IEvaluator::static_eval_trace(const Position& pos, EvalBreakdown* out) const {
    const int score = static_eval(pos);
    if (out) {
        *out = EvalBreakdown{};
        out->total_white_pov = pos.side_to_move() == WHITE ? score : -score;
    }
    return score;
}

EvalStats IEvaluator::stats() const {
    return EvalStats{};
}

void IEvaluator::clear_stats() {}

int MaterialEvaluator::static_eval(const Position& pos) const {
    static constexpr std::array<int, PIECE_TYPE_NB> value = {
      0,
      100,
      320,
      330,
      500,
      900,
      0,
    };

    int white = 0;
    int black = 0;

    for (int pt = PAWN; pt <= KING; ++pt) {
        white += popcount(pos.pieces(WHITE, static_cast<PieceType>(pt))) * value[pt];
        black += popcount(pos.pieces(BLACK, static_cast<PieceType>(pt))) * value[pt];
    }

    const int score = white - black;
    return pos.side_to_move() == WHITE ? score : -score;
}

}  // namespace makaira
