#pragma once

#include "evaluator.h"
#include "types.h"

#include <array>

namespace makaira::eval_params {

inline constexpr std::array<Score, PIECE_TYPE_NB> PIECE_VALUE = {
  make_score(0, 0),
  make_score(82, 94),
  make_score(337, 281),
  make_score(365, 297),
  make_score(477, 512),
  make_score(1025, 936),
  make_score(0, 0),
};

inline constexpr std::array<int, PIECE_TYPE_NB> PHASE_INC = {
  0,
  0,
  1,
  1,
  2,
  4,
  0,
};

inline constexpr int MAX_PHASE = 24;
inline constexpr int TEMPO_BONUS = 10;

inline constexpr Score BISHOP_PAIR_BONUS = make_score(28, 52);
inline constexpr Score ROOK_OPEN_FILE_BONUS = make_score(24, 6);
inline constexpr Score ROOK_SEMIOPEN_FILE_BONUS = make_score(14, 4);
inline constexpr Score ROOK_ON_SEVENTH_BONUS = make_score(18, 28);
inline constexpr Score KNIGHT_OUTPOST_BONUS = make_score(18, 14);
inline constexpr Score BAD_BISHOP_PENALTY = make_score(10, 6);

inline constexpr std::array<int, 9> PASSED_PAWN_MG = {0, 0, 10, 18, 36, 58, 96, 0, 0};
inline constexpr std::array<int, 9> PASSED_PAWN_EG = {0, 0, 16, 30, 58, 96, 150, 0, 0};
inline constexpr int ISOLATED_PAWN_PENALTY_MG = 14;
inline constexpr int ISOLATED_PAWN_PENALTY_EG = 10;
inline constexpr int DOUBLED_PAWN_PENALTY_MG = 11;
inline constexpr int DOUBLED_PAWN_PENALTY_EG = 14;
inline constexpr int BACKWARD_PAWN_PENALTY_MG = 10;
inline constexpr int BACKWARD_PAWN_PENALTY_EG = 8;
inline constexpr int CANDIDATE_PAWN_BONUS_MG = 8;
inline constexpr int CANDIDATE_PAWN_BONUS_EG = 14;
inline constexpr int CONNECTED_PASSER_BONUS_MG = 12;
inline constexpr int CONNECTED_PASSER_BONUS_EG = 20;
inline constexpr int SUPPORTED_PASSER_BONUS_MG = 10;
inline constexpr int SUPPORTED_PASSER_BONUS_EG = 16;
inline constexpr int OUTSIDE_PASSER_BONUS_MG = 6;
inline constexpr int OUTSIDE_PASSER_BONUS_EG = 16;
inline constexpr int BLOCKED_PASSER_PENALTY_MG = 14;
inline constexpr int BLOCKED_PASSER_PENALTY_EG = 10;

inline constexpr int SHELTER_PAWN_BONUS[8] = {0, 34, 26, 18, 10, 6, 3, 0};
inline constexpr int STORM_PAWN_PENALTY[8] = {0, 8, 12, 18, 26, 34, 44, 0};

inline constexpr std::array<std::array<int, 16>, PIECE_TYPE_NB> MOBILITY_BONUS_MG = {{
  {0},
  {0},
  {-20, -12, -6, -2, 2, 6, 10, 14, 18, 20, 22, 24, 24, 24, 24, 24},
  {-16, -8, -2, 2, 6, 10, 14, 18, 22, 24, 26, 28, 28, 28, 28, 28},
  {-12, -6, 0, 4, 8, 12, 16, 20, 24, 26, 28, 30, 32, 32, 32, 32},
  {-8, -2, 2, 6, 10, 14, 18, 22, 26, 28, 30, 32, 34, 36, 36, 36},
  {0},
}};

inline constexpr std::array<std::array<int, 16>, PIECE_TYPE_NB> MOBILITY_BONUS_EG = {{
  {0},
  {0},
  {-12, -8, -4, -2, 0, 2, 4, 6, 8, 9, 10, 11, 12, 12, 12, 12},
  {-10, -6, -2, 0, 2, 4, 6, 8, 10, 11, 12, 13, 14, 14, 14, 14},
  {-8, -4, -1, 2, 4, 6, 8, 10, 12, 13, 14, 15, 16, 16, 16, 16},
  {-6, -2, 1, 4, 6, 8, 10, 12, 14, 15, 16, 17, 18, 20, 20, 20},
  {0},
}};

inline constexpr int KING_ATTACK_UNIT[PIECE_TYPE_NB] = {0, 0, 2, 2, 3, 5, 0};
inline constexpr int KING_DANGER_SCALE[8] = {0, 1, 3, 6, 10, 15, 21, 28};

inline constexpr Score HANGING_PIECE_BONUS = make_score(18, 14);
inline constexpr Score THREAT_BY_PAWN_BONUS = make_score(16, 10);
inline constexpr Score SPACE_BONUS = make_score(4, 0);
inline constexpr Score KING_ACTIVITY_BONUS = make_score(0, 12);

}  // namespace makaira::eval_params