#pragma once

#include "move.h"
#include "position.h"

#include <array>

namespace makaira {

constexpr int MOVE_INDEX_NB = static_cast<int>(PIECE_NB) * static_cast<int>(SQ_NB);

struct QuietOrderContext {
    const std::int16_t* history = nullptr;
    const std::int16_t* cont_history = nullptr;
    const std::int16_t* capture_history = nullptr;
    bool use_history = false;
    bool use_cont_history = false;
    bool use_capture_history = false;
    bool use_see = true;
    Color side = WHITE;
    int prev1_move_index = -1;
    int prev2_move_index = -1;
    int cont_history_2ply_divisor = 2;
    Move killer1{};
    Move killer2{};
    Move counter{};
};

enum class MovePickPhase : std::uint8_t {
    TT = 0,
    GOOD_CAPTURE = 1,
    QUIET = 2,
    BAD_CAPTURE = 3,
    END = 4
};

class MovePicker {
   public:
    MovePicker(const Position& pos, Move tt_move, bool qsearch_only, const QuietOrderContext* quiet_ctx = nullptr);

    Move next(MovePickPhase* phase = nullptr);
    int generated_count() const { return generated_count_; }

   private:
    struct ScoredMove {
        Move move{};
        int score = 0;
    };

    static bool better(const ScoredMove& lhs, const ScoredMove& rhs);
    Move pick_next_from_bucket(std::array<ScoredMove, 256>& bucket, int count, int& index, MovePickPhase phase, MovePickPhase* out);
    static int capture_score(const Position& pos, Move move, const QuietOrderContext* quiet_ctx);
    static int quiet_score(const Position& pos, Move move, const QuietOrderContext* quiet_ctx);

    Move tt_move_{};
    bool qsearch_only_ = false;
    bool tt_done_ = false;
    int generated_count_ = 0;
    const QuietOrderContext* quiet_ctx_ = nullptr;

    std::array<ScoredMove, 256> good_captures_{};
    std::array<ScoredMove, 256> quiets_{};
    std::array<ScoredMove, 256> bad_captures_{};
    int good_count_ = 0;
    int quiet_count_ = 0;
    int bad_count_ = 0;
    int good_idx_ = 0;
    int quiet_idx_ = 0;
    int bad_idx_ = 0;
};

}  // namespace makaira
