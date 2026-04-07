#include "movepicker.h"

#include "movegen.h"
#include "see.h"

#include <algorithm>
#include <array>
#include <utility>

namespace makaira {
namespace {

constexpr std::array<int, PIECE_TYPE_NB> PIECE_ORDER_VALUE = {
  0,
  100,
  320,
  330,
  500,
  900,
  10000,
};

}  // namespace

MovePicker::MovePicker(const Position& pos, Move tt_move, bool qsearch_only, const QuietOrderContext* quiet_ctx) :
    tt_move_(tt_move),
    qsearch_only_(qsearch_only),
    quiet_ctx_(quiet_ctx) {
    MoveList moves;
    generate_pseudo_legal(pos, moves);
    generated_count_ = moves.count;

    for (int i = 0; i < moves.count; ++i) {
        const Move move = moves[i];

        if (!tt_move_.is_none() && move == tt_move_) {
            continue;
        }

        const bool is_capture_or_promo = move.is_capture() || move.is_promotion();
        if (qsearch_only_ && !is_capture_or_promo) {
            continue;
        }

        if (is_capture_or_promo) {
            const int score = capture_score(pos, move, quiet_ctx_);
            if (score >= 0 || move.is_promotion()) {
                good_captures_[good_count_++] = ScoredMove{move, score};
            } else {
                bad_captures_[bad_count_++] = ScoredMove{move, score};
            }
            continue;
        }

        quiets_[quiet_count_++] = ScoredMove{move, quiet_score(pos, move, quiet_ctx_)};
    }
}

Move MovePicker::next(MovePickPhase* phase) {
    if (!tt_done_) {
        tt_done_ = true;
        if (!tt_move_.is_none()) {
            if (phase) {
                *phase = MovePickPhase::TT;
            }
            return tt_move_;
        }
    }

    if (Move move = pick_next_from_bucket(good_captures_, good_count_, good_idx_, MovePickPhase::GOOD_CAPTURE, phase); !move.is_none()) {
        return move;
    }

    if (!qsearch_only_) {
        if (Move move = pick_next_from_bucket(quiets_, quiet_count_, quiet_idx_, MovePickPhase::QUIET, phase); !move.is_none()) {
            return move;
        }
    }

    if (Move move = pick_next_from_bucket(bad_captures_, bad_count_, bad_idx_, MovePickPhase::BAD_CAPTURE, phase); !move.is_none()) {
        return move;
    }

    if (phase) {
        *phase = MovePickPhase::END;
    }
    return Move{};
}

bool MovePicker::better(const ScoredMove& lhs, const ScoredMove& rhs) {
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }

    return lhs.move.raw() < rhs.move.raw();
}

Move MovePicker::pick_next_from_bucket(std::array<ScoredMove, 256>& bucket, int count, int& index, MovePickPhase phase, MovePickPhase* out) {
    if (index >= count) {
        return Move{};
    }

    int best = index;
    for (int i = index + 1; i < count; ++i) {
        if (better(bucket[i], bucket[best])) {
            best = i;
        }
    }

    if (best != index) {
        std::swap(bucket[index], bucket[best]);
    }

    if (out) {
        *out = phase;
    }
    return bucket[index++].move;
}

int MovePicker::capture_score(const Position& pos, Move move, const QuietOrderContext* quiet_ctx) {
    Piece captured = NO_PIECE;
    if (move.is_en_passant()) {
        captured = make_piece(~pos.side_to_move(), PAWN);
    } else {
        captured = pos.piece_on(move.to());
    }

    const Piece attacker = pos.piece_on(move.from());
    const int captured_value = captured == NO_PIECE ? 0 : PIECE_ORDER_VALUE[type_of(captured)];
    const int attacker_value = attacker == NO_PIECE ? 0 : PIECE_ORDER_VALUE[type_of(attacker)];

    int hist = 0;
    if (quiet_ctx && quiet_ctx->use_capture_history && quiet_ctx->capture_history && attacker != NO_PIECE && captured != NO_PIECE) {
        const int side = static_cast<int>(pos.side_to_move());
        const int moved_pt = static_cast<int>(type_of(attacker));
        const int to = static_cast<int>(move.to());
        const int cap_pt = static_cast<int>(type_of(captured));
        const int idx = ((side * PIECE_TYPE_NB + moved_pt) * SQ_NB + to) * PIECE_TYPE_NB + cap_pt;
        hist = quiet_ctx->capture_history[idx];
    }

    const int see_term = (quiet_ctx && quiet_ctx->use_see) ? (static_exchange_eval(pos, move) * 1024) : 0;
    return see_term + hist + captured_value * 16 - attacker_value;
}

int MovePicker::quiet_score(const Position& pos, Move move, const QuietOrderContext* quiet_ctx) {
    if (!quiet_ctx) {
        return 0;
    }

    int score = 0;
    if (move == quiet_ctx->killer1) {
        score += 1000000;
    } else if (move == quiet_ctx->killer2) {
        score += 900000;
    } else if (move == quiet_ctx->counter) {
        score += 800000;
    }

    if (quiet_ctx->use_history && quiet_ctx->history) {
        const int idx = (static_cast<int>(quiet_ctx->side) * SQ_NB + static_cast<int>(move.from())) * SQ_NB
                        + static_cast<int>(move.to());
        score += quiet_ctx->history[idx];
    }

    if (quiet_ctx->use_cont_history && quiet_ctx->cont_history) {
        const Piece moved = pos.piece_on(move.from());
        if (moved != NO_PIECE) {
            const int cur = static_cast<int>(moved) * SQ_NB + static_cast<int>(move.to());
            if (quiet_ctx->prev1_move_index >= 0) {
                score += quiet_ctx->cont_history[quiet_ctx->prev1_move_index * MOVE_INDEX_NB + cur];
            }
            if (quiet_ctx->prev2_move_index >= 0) {
                const int d = std::max(1, quiet_ctx->cont_history_2ply_divisor);
                score += quiet_ctx->cont_history[quiet_ctx->prev2_move_index * MOVE_INDEX_NB + cur] / d;
            }
        }
    }

    return score;
}

}  // namespace makaira
