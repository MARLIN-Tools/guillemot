#include "hce_evaluator.h"

#include "bitboard.h"
#include "eval_params.h"
#include "eval_params_tuned.h"
#include "eval_tables.h"
#include "zobrist.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace makaira {
namespace {

inline int rel_rank(Color c, Square sq) {
    return c == WHITE ? static_cast<int>(rank_of(sq)) : 7 - static_cast<int>(rank_of(sq));
}

inline int clamp_index(int v) {
    return std::clamp(v, 0, 15);
}

inline Bitboard shift_up(Color c, Bitboard b) {
    return c == WHITE ? (b << 8) : (b >> 8);
}

inline int sign_for(Color c) {
    return c == WHITE ? 1 : -1;
}

inline Score apply_scale(Score s, int mg_scale, int eg_scale) {
    return make_score((s.mg * mg_scale) / 100, (s.eg * eg_scale) / 100);
}

inline int square_color(Square sq) {
    return (static_cast<int>(file_of(sq)) + static_cast<int>(rank_of(sq))) & 1;
}

inline int king_centralization(Square sq) {
    const int f = static_cast<int>(file_of(sq));
    const int r = static_cast<int>(rank_of(sq));
    const int df = std::abs(2 * f - 7);
    const int dr = std::abs(2 * r - 7);
    return 14 - (df + dr);
}

Bitboard center_mask() {
    Bitboard b = 0;
    for (int f = FILE_C; f <= FILE_F; ++f) {
        for (int r = RANK_3; r <= RANK_6; ++r) {
            b |= bb_from(make_square(static_cast<File>(f), static_cast<Rank>(r)));
        }
    }
    return b;
}

const Bitboard CENTER_MASK = center_mask();

}  // namespace

HCEEvaluator::HCEEvaluator() :
    pawn_hash_(1ULL << 16) {
    eval_tables::init_eval_tables();
}

EvalStats HCEEvaluator::stats() const {
    return stats_;
}

void HCEEvaluator::clear_stats() {
    stats_ = {};
    pawn_hash_.clear();
}

void HCEEvaluator::set_profile_mode(bool enabled) const {
    profile_mode_ = enabled;
}

int HCEEvaluator::static_eval(const Position& pos) const {
    return evaluate(pos, true, nullptr);
}

int HCEEvaluator::static_eval_trace(const Position& pos, EvalBreakdown* out) const {
    return evaluate(pos, true, out);
}

int HCEEvaluator::static_eval_recompute(const Position& pos) const {
    return evaluate(pos, false, nullptr);
}

Score HCEEvaluator::evaluate_material_psqt(const Position& pos, bool use_incremental) const {
    if (use_incremental) {
        return make_score(pos.mg_psqt(WHITE) - pos.mg_psqt(BLACK), pos.eg_psqt(WHITE) - pos.eg_psqt(BLACK));
    }

    Score s{};
    for (int sq = SQ_A1; sq <= SQ_H8; ++sq) {
        const Piece pc = pos.piece_on(static_cast<Square>(sq));
        if (pc == NO_PIECE) {
            continue;
        }

        const Score ps = eval_tables::psqt(pc, static_cast<Square>(sq));
        const int sign = sign_for(color_of(pc));
        s.mg += sign * ps.mg;
        s.eg += sign * ps.eg;
    }
    return s;
}

PawnHashEntry HCEEvaluator::compute_pawn_entry(const Position& pos, Key pawn_key_with_kings) const {
    PawnHashEntry e{};
    e.key = pawn_key_with_kings;

    std::array<Bitboard, COLOR_NB> pawn_attacks{{0, 0}};
    for (Color c : {WHITE, BLACK}) {
        Bitboard pawns = pos.pieces(c, PAWN);
        while (pawns) {
            const Square sq = pop_lsb(pawns);
            pawn_attacks[c] |= attacks::pawn[c][sq];
        }
    }

    for (Color c : {WHITE, BLACK}) {
        const Color them = ~c;
        const Bitboard our = pos.pieces(c, PAWN);
        const Bitboard enemy = pos.pieces(them, PAWN);

        Bitboard pawns = our;
        while (pawns) {
            const Square sq = pop_lsb(pawns);
            const int rr = rel_rank(c, sq);
            const int file = static_cast<int>(file_of(sq));
            const Bitboard sq_bb = bb_from(sq);

            const bool isolated = (our & eval_tables::ADJACENT_FILE_MASK[file]) == 0;
            if (isolated) {
                e.pawn_score += make_score(-eval_params::ISOLATED_PAWN_PENALTY_MG * sign_for(c),
                                           -eval_params::ISOLATED_PAWN_PENALTY_EG * sign_for(c));
            }

            if (popcount(our & eval_tables::FILE_MASK[file]) > 1) {
                e.pawn_score += make_score(-eval_params::DOUBLED_PAWN_PENALTY_MG * sign_for(c),
                                           -eval_params::DOUBLED_PAWN_PENALTY_EG * sign_for(c));
            }

            const Bitboard passed_mask = eval_tables::PASSED_MASK[c][sq];
            const bool is_passed = (enemy & passed_mask) == 0;
            if (is_passed) {
                e.passed[c] |= sq_bb;
                e.pawn_score += make_score(eval_params::PASSED_PAWN_MG[rr] * sign_for(c),
                                           eval_params::PASSED_PAWN_EG[rr] * sign_for(c));

                if (pawn_attacks[c] & sq_bb) {
                    e.pawn_score += make_score(eval_params::SUPPORTED_PASSER_BONUS_MG * sign_for(c),
                                               eval_params::SUPPORTED_PASSER_BONUS_EG * sign_for(c));
                }

                if ((our & eval_tables::ADJACENT_FILE_MASK[file]) != 0) {
                    e.pawn_score += make_score(eval_params::CONNECTED_PASSER_BONUS_MG * sign_for(c),
                                               eval_params::CONNECTED_PASSER_BONUS_EG * sign_for(c));
                }

                const bool outside = file <= FILE_B || file >= FILE_G;
                if (outside) {
                    e.pawn_score += make_score(eval_params::OUTSIDE_PASSER_BONUS_MG * sign_for(c),
                                               eval_params::OUTSIDE_PASSER_BONUS_EG * sign_for(c));
                }

                const Square stop = c == WHITE ? static_cast<Square>(sq + 8) : static_cast<Square>(sq - 8);
                if (is_ok_square(stop) && pos.piece_on(stop) != NO_PIECE) {
                    e.pawn_score += make_score(-eval_params::BLOCKED_PASSER_PENALTY_MG * sign_for(c),
                                               -eval_params::BLOCKED_PASSER_PENALTY_EG * sign_for(c));
                }
            } else {
                const Bitboard forward = eval_tables::FORWARD_MASK[c][sq];
                if ((enemy & forward) == 0) {
                    e.pawn_score += make_score(eval_params::CANDIDATE_PAWN_BONUS_MG * sign_for(c),
                                               eval_params::CANDIDATE_PAWN_BONUS_EG * sign_for(c));
                }
            }

            const Square stop = c == WHITE ? static_cast<Square>(sq + 8) : static_cast<Square>(sq - 8);
            if (is_ok_square(stop)) {
                const bool blocked = pos.piece_on(stop) != NO_PIECE;
                const bool no_support = (our & eval_tables::ADJACENT_FILE_MASK[file] & eval_tables::FORWARD_MASK[them][sq]) == 0;
                if (blocked && no_support && (pawn_attacks[them] & bb_from(stop))) {
                    e.pawn_score += make_score(-eval_params::BACKWARD_PAWN_PENALTY_MG * sign_for(c),
                                               -eval_params::BACKWARD_PAWN_PENALTY_EG * sign_for(c));
                }
            }
        }
    }

    for (Color c : {WHITE, BLACK}) {
        const Square ksq = pos.king_square(c);
        const int kf = static_cast<int>(file_of(ksq));
        const int kr = static_cast<int>(rank_of(ksq));
        int shelter = 0;

        for (int df = -1; df <= 1; ++df) {
            const int f = kf + df;
            if (f < FILE_A || f > FILE_H) {
                continue;
            }

            Bitboard file_pawns = pos.pieces(c, PAWN) & eval_tables::FILE_MASK[f];
            while (file_pawns) {
                const Square psq = pop_lsb(file_pawns);
                const int dist = c == WHITE ? static_cast<int>(rank_of(psq)) - kr : kr - static_cast<int>(rank_of(psq));
                if (dist >= 0 && dist <= 7) {
                    shelter += eval_params::SHELTER_PAWN_BONUS[dist];
                    break;
                }
            }

            Bitboard storms = pos.pieces(~c, PAWN) & eval_tables::FILE_MASK[f];
            while (storms) {
                const Square esq = pop_lsb(storms);
                const int dist = c == WHITE ? kr - static_cast<int>(rank_of(esq)) : static_cast<int>(rank_of(esq)) - kr;
                if (dist >= 0 && dist <= 7) {
                    shelter -= eval_params::STORM_PAWN_PENALTY[dist];
                    break;
                }
            }
        }

        e.shelter_bonus_mg[c] = shelter;
    }

    return e;
}

HCEEvaluator::AttackInfo HCEEvaluator::build_attack_info(const Position& pos) const {
    AttackInfo ai{};
    const Bitboard occ = pos.occupancy();

    for (Color c : {WHITE, BLACK}) {
        Bitboard pawns = pos.pieces(c, PAWN);
        while (pawns) {
            const Square sq = pop_lsb(pawns);
            ai.pawn_attacks[c] |= attacks::pawn[c][sq];
            ai.all_attacks[c] |= attacks::pawn[c][sq];
        }
    }

    for (Color c : {WHITE, BLACK}) {
        const Color them = ~c;
        const Bitboard own_occ = pos.occupancy(c);
        const Bitboard enemy_pawn_attacks = ai.pawn_attacks[them];
        const Square enemy_king = pos.king_square(them);
        const Bitboard king_ring = attacks::king[enemy_king] | bb_from(enemy_king);

        for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN}) {
            Bitboard pieces = pos.pieces(c, pt);
            while (pieces) {
                const Square sq = pop_lsb(pieces);
                Bitboard atk = 0;
                if (pt == KNIGHT) {
                    atk = attacks::knight[sq];
                } else if (pt == BISHOP) {
                    atk = attacks::bishop_attacks(sq, occ);
                } else if (pt == ROOK) {
                    atk = attacks::rook_attacks(sq, occ);
                } else {
                    atk = attacks::bishop_attacks(sq, occ) | attacks::rook_attacks(sq, occ);
                }

                ai.all_attacks[c] |= atk;

                const Bitboard mobility_targets = atk & ~own_occ & ~enemy_pawn_attacks;
                const int mob = clamp_index(popcount(mobility_targets));
                ai.mobility.mg += sign_for(c) * eval_params::MOBILITY_BONUS_MG[pt][mob];
                ai.mobility.eg += sign_for(c) * eval_params::MOBILITY_BONUS_EG[pt][mob];

                const int ring_hits = popcount(atk & king_ring);
                if (ring_hits > 0) {
                    ai.king_attackers[c] += 1;
                    ai.king_attack_units[c] += ring_hits * eval_params::KING_ATTACK_UNIT[pt];
                }
            }
        }

        ai.all_attacks[c] |= attacks::king[pos.king_square(c)];
    }

    return ai;
}

Score HCEEvaluator::evaluate_piece_features(const Position& pos, const AttackInfo&) const {
    Score s{};

    for (Color c : {WHITE, BLACK}) {
        const int sign = sign_for(c);

        if (popcount(pos.pieces(c, BISHOP)) >= 2) {
            s += eval_params::BISHOP_PAIR_BONUS * sign;
        }

        Bitboard rooks = pos.pieces(c, ROOK);
        while (rooks) {
            const Square sq = pop_lsb(rooks);
            const int f = static_cast<int>(file_of(sq));
            const Bitboard file_mask = eval_tables::FILE_MASK[f];
            const bool own_pawn = (pos.pieces(c, PAWN) & file_mask) != 0;
            const bool enemy_pawn = (pos.pieces(~c, PAWN) & file_mask) != 0;

            if (!own_pawn && !enemy_pawn) {
                s += eval_params::ROOK_OPEN_FILE_BONUS * sign;
            } else if (!own_pawn && enemy_pawn) {
                s += eval_params::ROOK_SEMIOPEN_FILE_BONUS * sign;
            }

            const int rr = rel_rank(c, sq);
            if (rr == 6) {
                s += eval_params::ROOK_ON_SEVENTH_BONUS * sign;
            }
        }

        Bitboard knights = pos.pieces(c, KNIGHT);
        while (knights) {
            const Square sq = pop_lsb(knights);
            const int rr = rel_rank(c, sq);
            if (rr < 3 || rr > 5) {
                continue;
            }

            const Bitboard sq_bb = bb_from(sq);
            Bitboard support = 0;
            Bitboard pawns = pos.pieces(c, PAWN);
            while (pawns) {
                const Square psq = pop_lsb(pawns);
                support |= attacks::pawn[c][psq];
            }

            Bitboard enemy_attacks = 0;
            Bitboard epawns = pos.pieces(~c, PAWN);
            while (epawns) {
                const Square psq = pop_lsb(epawns);
                enemy_attacks |= attacks::pawn[~c][psq];
            }

            if ((support & sq_bb) && !(enemy_attacks & sq_bb)) {
                s += eval_params::KNIGHT_OUTPOST_BONUS * sign;
            }
        }

        Bitboard bishops = pos.pieces(c, BISHOP);
        int bad_bishop_pawns = 0;
        while (bishops) {
            const Square bsq = pop_lsb(bishops);
            const int bcolor = square_color(bsq);
            Bitboard pawns = pos.pieces(c, PAWN);
            while (pawns) {
                const Square psq = pop_lsb(pawns);
                if (square_color(psq) == bcolor) {
                    ++bad_bishop_pawns;
                }
            }
        }
        s += eval_params::BAD_BISHOP_PENALTY * (-sign * bad_bishop_pawns / 2);
    }

    return s;
}

Score HCEEvaluator::evaluate_threats(const Position& pos, const AttackInfo& ai) const {
    Score s{};

    for (Color c : {WHITE, BLACK}) {
        const Color them = ~c;
        const int sign = sign_for(c);

        Bitboard enemy_pieces = pos.occupancy(them) & ~pos.pieces(them, KING);
        Bitboard pawn_threats = ai.pawn_attacks[c] & enemy_pieces;
        while (pawn_threats) {
            const Square sq = pop_lsb(pawn_threats);
            const Bitboard bb = bb_from(sq);
            if (!(ai.all_attacks[them] & bb)) {
                s += eval_params::THREAT_BY_PAWN_BONUS * sign;
            }
        }

        Bitboard hanging = ai.all_attacks[c] & enemy_pieces & ~ai.all_attacks[them];
        int n = popcount(hanging);
        s += eval_params::HANGING_PIECE_BONUS * (sign * n);
    }

    return s;
}

Score HCEEvaluator::evaluate_space(const Position& pos, const AttackInfo& ai) const {
    Score s{};

    for (Color c : {WHITE, BLACK}) {
        if (popcount(pos.pieces(c, PAWN)) < 4) {
            continue;
        }

        const Bitboard controlled = ai.all_attacks[c] & CENTER_MASK;
        const Bitboard free = controlled & ~pos.occupancy(c) & ~pos.pieces(c, PAWN);
        const int gain = popcount(free);
        s += eval_params::SPACE_BONUS * (sign_for(c) * gain);
    }

    return s;
}

Score HCEEvaluator::evaluate_endgame_terms(const Position& pos) const {
    Score s{};

    const Square wk = pos.king_square(WHITE);
    const Square bk = pos.king_square(BLACK);

    const int w_center = king_centralization(wk);
    const int b_center = king_centralization(bk);
    s.eg += (w_center - b_center) * eval_params::KING_ACTIVITY_BONUS.eg / 8;

    return s;
}

int HCEEvaluator::evaluate_endgame_scale(const Position& pos, int blended_white_pov) const {
    int scale = 128;

    const bool only_bishops =
      pos.pieces(WHITE, KNIGHT) == 0 && pos.pieces(BLACK, KNIGHT) == 0
      && pos.pieces(WHITE, ROOK) == 0 && pos.pieces(BLACK, ROOK) == 0
      && pos.pieces(WHITE, QUEEN) == 0 && pos.pieces(BLACK, QUEEN) == 0;

    if (only_bishops && popcount(pos.pieces(WHITE, BISHOP)) == 1 && popcount(pos.pieces(BLACK, BISHOP)) == 1) {
        scale = 96;
    }

    const int total_pawns = popcount(pos.pieces(WHITE, PAWN) | pos.pieces(BLACK, PAWN));
    if (total_pawns <= 2 && std::abs(blended_white_pov) < 120) {
        scale = std::min(scale, 88);
    }

    return scale;
}

int HCEEvaluator::evaluate(const Position& pos, bool use_incremental, EvalBreakdown* out) const {
    const bool do_profile = profile_mode_;
    const auto eval_start = do_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    ++stats_.eval_calls;

    EvalBreakdown b{};

    b.material_psqt = evaluate_material_psqt(pos, use_incremental);

    Key pawn_key = pos.pawn_key()
                 ^ Zobrist.pawn_file_king[WHITE][file_of(pos.king_square(WHITE))]
                 ^ Zobrist.pawn_file_king[BLACK][file_of(pos.king_square(BLACK))];

    const auto pawn_t0 = do_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const PawnHashEntry* cached = pawn_hash_.probe(pawn_key);
    PawnHashEntry entry{};
    if (cached) {
        ++stats_.pawn_hash_hits;
        entry = *cached;
    } else {
        ++stats_.pawn_hash_misses;
        entry = compute_pawn_entry(pos, pawn_key);
        pawn_hash_.store(entry);
    }
    if (do_profile) {
        const auto pawn_t1 = std::chrono::steady_clock::now();
        stats_.eval_ns_pawn += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(pawn_t1 - pawn_t0).count());
    }

    b.pawns = entry.pawn_score;
    b.king_safety.mg += entry.shelter_bonus_mg[WHITE] - entry.shelter_bonus_mg[BLACK];

    const auto attack_t0 = do_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const AttackInfo ai = build_attack_info(pos);
    const auto attack_t1 = do_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if (do_profile) {
        stats_.eval_ns_attack_maps += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(attack_t1 - attack_t0).count());
    }
    b.mobility = ai.mobility;
    if (do_profile) {
        stats_.eval_ns_mobility += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(attack_t1 - attack_t0).count());
    }

    const auto king_t0 = do_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    for (Color c : {WHITE, BLACK}) {
        const Color them = ~c;
        const int sign = sign_for(c);
        const int attackers = std::clamp(ai.king_attackers[c], 0, 7);
        const int base_units = ai.king_attack_units[c];
        const int np_scale = std::clamp(pos.non_pawn_material(c) / 8, 0, 128);
        const int danger = (base_units * eval_params::KING_DANGER_SCALE[attackers] * np_scale) / 256;
        b.king_safety.mg += sign * danger;

        if (pos.non_pawn_material(them) < 1200) {
            b.king_safety.mg -= sign * (danger / 3);
        }
    }
    if (do_profile) {
        const auto king_t1 = std::chrono::steady_clock::now();
        stats_.eval_ns_king += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(king_t1 - king_t0).count());
    }

    b.piece_features = evaluate_piece_features(pos, ai);

    const auto threat_t0 = do_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    b.threats = evaluate_threats(pos, ai);
    if (do_profile) {
        const auto threat_t1 = std::chrono::steady_clock::now();
        stats_.eval_ns_threats += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(threat_t1 - threat_t0).count());
    }

    const auto space_t0 = do_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    b.space = evaluate_space(pos, ai);
    if (do_profile) {
        const auto space_t1 = std::chrono::steady_clock::now();
        stats_.eval_ns_space += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(space_t1 - space_t0).count());
    }

    b.endgame_scale = evaluate_endgame_scale(pos, b.material_psqt.mg + b.pawns.mg);

    Score total{};
    total += apply_scale(
      b.material_psqt, eval_params_tuned::MATERIAL_PSQT_MG_SCALE, eval_params_tuned::MATERIAL_PSQT_EG_SCALE);
    total += apply_scale(b.pawns, eval_params_tuned::PAWN_MG_SCALE, eval_params_tuned::PAWN_EG_SCALE);
    total += apply_scale(b.mobility, eval_params_tuned::MOBILITY_MG_SCALE, eval_params_tuned::MOBILITY_EG_SCALE);
    total += apply_scale(b.king_safety, eval_params_tuned::KING_MG_SCALE, eval_params_tuned::KING_EG_SCALE);
    total += apply_scale(b.piece_features, eval_params_tuned::PIECE_MG_SCALE, eval_params_tuned::PIECE_EG_SCALE);
    total += apply_scale(b.threats, eval_params_tuned::THREAT_MG_SCALE, eval_params_tuned::THREAT_EG_SCALE);
    total += apply_scale(b.space, eval_params_tuned::SPACE_MG_SCALE, eval_params_tuned::SPACE_EG_SCALE);
    total += evaluate_endgame_terms(pos);
    const int tuned_tempo = (eval_params::TEMPO_BONUS * eval_params_tuned::TEMPO_SCALE) / 100;
    const int tempo_sign = pos.side_to_move() == WHITE ? 1 : -1;
    total.mg += tuned_tempo * tempo_sign;
    b.tempo = tuned_tempo * tempo_sign;

    const int phase = use_incremental ? std::clamp(pos.phase(), 0, eval_params::MAX_PHASE)
                                      : std::clamp(popcount(pos.pieces(WHITE, KNIGHT) | pos.pieces(BLACK, KNIGHT))
                                                     + popcount(pos.pieces(WHITE, BISHOP) | pos.pieces(BLACK, BISHOP))
                                                     + 2 * popcount(pos.pieces(WHITE, ROOK) | pos.pieces(BLACK, ROOK))
                                                     + 4 * popcount(pos.pieces(WHITE, QUEEN) | pos.pieces(BLACK, QUEEN)),
                                                   0,
                                                   eval_params::MAX_PHASE);
    b.phase = phase;

    int blended = (total.mg * phase + total.eg * (eval_params::MAX_PHASE - phase)) / eval_params::MAX_PHASE;
    blended = (blended * b.endgame_scale) / 128;
    b.total_white_pov = blended;

    if (out) {
        *out = b;
    }

    if (do_profile) {
        const auto eval_end = std::chrono::steady_clock::now();
        stats_.eval_ns_total += static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(eval_end - eval_start).count());
    }
    return pos.side_to_move() == WHITE ? blended : -blended;
}

}  // namespace makaira
