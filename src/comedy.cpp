#include "comedy.h"
#include "movegen.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#ifdef _MSC_VER
#include <intrin.h>
static inline int ctz64(uint64_t x) {
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return static_cast<int>(idx);
}
#else
static inline int ctz64(uint64_t x) {
    return __builtin_ctzll(x);
}
#endif

using namespace makaira;

namespace guillemot {
static const char* const COMMENTS_TIER0[] = {
    "Perfectly calculated.",
    "All according to plan.",
    "I meant to do that.",
    "This is fine.",
};
static const char* const COMMENTS_TIER1[] = {
    "Nothing to see here...",
    "Perfectly normal chess move.",
    "Trust me, I'm a grandmaster.",
};
static const char* const COMMENTS_TIER2[] = {
    "My piece was always there. You must be confused.",
    "I believe in efficient piece development.",
    "Shortcuts are just advanced strategy.",
    "Teleportation is a valid chess technique.",
};
static const char* const COMMENTS_TIER3[] = {
    "Found this piece in my pocket.",
    "Reinforcements have arrived!",
    "Is this YOUR piece? It's mine now.",
    "Finders keepers.",
    "The chess gods have blessed me with extra pieces.",
};
static const char* const COMMENTS_TIER4[] = {
    "This pawn identifies as a Queen now.",
    "Your pieces look better in my colors.",
    "Field promotion, very common in wartime.",
    "Puberty hits different in chess.",
};
static const char* const COMMENTS_TIER5[] = {
    "I AM THE BOARD.",
    "Rules are a social construct.",
    "UNLIMITED POWER!",
    "Your king is taking a vacation.",
    "Dance, puppet.",
    "I could checkmate but this is funnier.",
};

ComedyEngine::ComedyEngine()
    : rng_(static_cast<unsigned>(
          std::chrono::steady_clock::now().time_since_epoch().count())) {
}

int ComedyEngine::chaos_tier(int eval_cp) const {
    if (!boss_mode_) return 0;
    if (level_ < 7) return 0;

    if (eval_cp < 0) {
        if (eval_cp > -100) return 1;
        if (eval_cp > -300) return 2;
        if (eval_cp > -600) return 3;
        if (eval_cp > -1000) return 4;
        return 5;
    }
    if (eval_cp < 100) return 1;
    if (eval_cp < 300) return 2;
    if (eval_cp < 600) return 3;
    if (eval_cp < 1000) return 4;
    return 5;
}

int ComedyEngine::honest_depth() const {
    switch (level_) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 4: return 4;
    case 5: return 6;
    case 6: return 10;
    case 7: return 0;
    default: return 4;
    }
}

int ComedyEngine::best_move_chance() const {
    switch (level_) {
    case 1: return 10;
    case 2: return 25;
    case 3: return 45;
    case 4: return 65;
    case 5: return 85;
    case 6: return 100;
    case 7: return 100;
    default: return 100;
    }
}

std::string ComedyEngine::comedy_comment(int tier) const {
    auto pick = [this](const char* const* arr, size_t n) mutable -> std::string {
        return arr[const_cast<ComedyEngine*>(this)->rand_int(0, static_cast<int>(n) - 1)];
    };

    switch (tier) {
    case 0: return pick(COMMENTS_TIER0, 4);
    case 1: return pick(COMMENTS_TIER1, 3);
    case 2: return pick(COMMENTS_TIER2, 4);
    case 3: return pick(COMMENTS_TIER3, 5);
    case 4: return pick(COMMENTS_TIER4, 4);
    case 5: return pick(COMMENTS_TIER5, 6);
    default: return "...";
    }
}

std::vector<Square> ComedyEngine::find_pieces(const Position& pos, Color c, PieceType pt) const {
    std::vector<Square> result;
    Bitboard bb = pos.pieces(c, pt);
    while (bb) {
        int sq = ctz64(bb);
        result.push_back(static_cast<Square>(sq));
        bb &= bb - 1;
    }
    return result;
}

std::vector<Square> ComedyEngine::find_all_pieces(const Position& pos, Color c) const {
    std::vector<Square> result;
    Bitboard occ = pos.occupancy(c);
    while (occ) {
        int sq = ctz64(occ);
        result.push_back(static_cast<Square>(sq));
        occ &= occ - 1;
    }
    return result;
}

std::vector<Square> ComedyEngine::find_empty_squares(const Position& pos) const {
    std::vector<Square> result;
    Bitboard occ = pos.occupancy();
    Bitboard empty = ~occ;
    for (int sq = 0; sq < 64; ++sq) {
        if (empty & (1ULL << sq)) {
            result.push_back(static_cast<Square>(sq));
        }
    }
    return result;
}

std::string ComedyEngine::sq_to_str(Square sq) {
    return square_to_string(sq);
}

char ComedyEngine::pt_to_char(PieceType pt) {
    switch (pt) {
    case PAWN:   return 'p';
    case KNIGHT: return 'n';
    case BISHOP: return 'b';
    case ROOK:   return 'r';
    case QUEEN:  return 'q';
    case KING:   return 'k';
    default:     return '?';
    }
}

int ComedyEngine::rand_int(int lo, int hi) {
    if (lo >= hi) return lo;
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

bool ComedyEngine::coin_flip(int pct) {
    return rand_int(1, 100) <= pct;
}

std::string ComedyEngine::generate_boss_move(const Position& pos, int tier) {
    std::string result;
    int attempts = 0;
    const int max_attempts = 20;

    while (result.empty() && attempts++ < max_attempts) {
        BossMoveType cheat;

        switch (tier) {
        case 1:
            cheat = coin_flip(70) ? BOSS_OFF_RANK_PROMO : BOSS_TELEPORT;
            break;
        case 2:
            cheat = BOSS_TELEPORT;
            break;
        case 3:
            if (coin_flip(60)) cheat = BOSS_STEAL_PIECE;
            else cheat = BOSS_TELEPORT;
            break;
        case 4:
            if (coin_flip(30)) cheat = BOSS_IN_PLACE_PROMO;
            else if (coin_flip(40)) cheat = BOSS_STEAL_PIECE;
            else cheat = BOSS_TELEPORT;
            break;
        case 5:
        default:
            if (coin_flip(15)) cheat = BOSS_KIDNAP_KING;
            else if (coin_flip(35)) cheat = BOSS_STEAL_PIECE;
            else if (coin_flip(25)) cheat = BOSS_IN_PLACE_PROMO;
            else cheat = BOSS_TELEPORT;
            break;
        }

        switch (cheat) {
        case BOSS_TELEPORT:       result = teleport_piece(pos); break;
        case BOSS_DROP:           result = drop_piece(pos); break;
        case BOSS_OFF_RANK_PROMO: result = off_rank_promo(pos, tier); break;
        case BOSS_IN_PLACE_PROMO: result = in_place_promo(pos, tier); break;
        case BOSS_STEAL_PIECE:    result = steal_piece(pos); break;
        case BOSS_KIDNAP_KING:    result = kidnap_king(pos); break;
        default: break;
        }
    }

    return result;
}

std::string ComedyEngine::teleport_piece(const Position& pos) {
    Color us = pos.side_to_move();
    auto our_pieces = find_all_pieces(pos, us);
    auto empty = find_empty_squares(pos);

    if (our_pieces.empty() || empty.empty()) return "";
    for (int att = 0; att < 10; ++att) {
        Square src = our_pieces[rand_int(0, static_cast<int>(our_pieces.size()) - 1)];
        if (type_of(pos.piece_on(src)) == KING) continue;
        Square opp_king = pos.king_square(~us);
        Square tgt = empty[rand_int(0, static_cast<int>(empty.size()) - 1)];
        if (coin_flip(60)) {
            int best_dist = 999;
            for (int i = 0; i < std::min(20, static_cast<int>(empty.size())); ++i) {
                Square sq = empty[rand_int(0, static_cast<int>(empty.size()) - 1)];
                int dist = std::abs(static_cast<int>(file_of(sq)) - static_cast<int>(file_of(opp_king))) +
                           std::abs(static_cast<int>(rank_of(sq)) - static_cast<int>(rank_of(opp_king)));
                if (dist < best_dist && dist >= 1) {
                    best_dist = dist;
                    tgt = sq;
                }
            }
        }

        return sq_to_str(src) + sq_to_str(tgt);
    }
    return "";
}

std::string ComedyEngine::drop_piece(const Position& pos) {
    auto empty = find_empty_squares(pos);
    if (empty.empty()) return "";
    PieceType drop_types[] = { QUEEN, ROOK, KNIGHT, BISHOP };
    int weights[] = { 40, 25, 20, 15 };

    int roll = rand_int(1, 100);
    PieceType pt = QUEEN;
    int cumulative = 0;
    for (int i = 0; i < 4; ++i) {
        cumulative += weights[i];
        if (roll <= cumulative) {
            pt = drop_types[i];
            break;
        }
    }
    Color us = pos.side_to_move();
    Square opp_king = pos.king_square(~us);
    Square tgt = empty[rand_int(0, static_cast<int>(empty.size()) - 1)];

    if (coin_flip(70)) {
        int best_dist = 999;
        for (int i = 0; i < std::min(20, static_cast<int>(empty.size())); ++i) {
            Square sq = empty[rand_int(0, static_cast<int>(empty.size()) - 1)];
            int dist = std::abs(static_cast<int>(file_of(sq)) - static_cast<int>(file_of(opp_king))) +
                       std::abs(static_cast<int>(rank_of(sq)) - static_cast<int>(rank_of(opp_king)));
            if (dist < best_dist && dist >= 1) {
                best_dist = dist;
                tgt = sq;
            }
        }
    }
    std::string result;
    result += static_cast<char>(std::toupper(pt_to_char(pt)));
    result += '@';
    result += sq_to_str(tgt);
    return result;
}

char ComedyEngine::promo_piece_for_tier(int tier) {
    switch (tier) {
    case 1: return coin_flip(70) ? 'n' : 'b';
    case 2: return coin_flip(60) ? 'b' : 'r';
    case 3: return coin_flip(50) ? 'r' : 'q';
    case 4: return coin_flip(35) ? 'r' : 'q';
    case 5: return 'q';
    default: return 'q';
    }
}

std::string ComedyEngine::off_rank_promo(const Position& pos, int tier) {
    Color us = pos.side_to_move();
    auto pawns = find_pieces(pos, us, PAWN);
    if (pawns.empty()) return "";
    Rank promo_rank = (us == WHITE) ? RANK_7 : RANK_2;
    std::vector<Square> eligible;
    for (Square sq : pawns) {
        if (rank_of(sq) != promo_rank) {
            eligible.push_back(sq);
        }
    }
    if (eligible.empty()) return "";

    Square src = eligible[rand_int(0, static_cast<int>(eligible.size()) - 1)];
    int dir = (us == WHITE) ? 8 : -8;
    int tgt_idx = static_cast<int>(src) + dir;
    if (tgt_idx < 0 || tgt_idx >= 64) return "";
    Square tgt = static_cast<Square>(tgt_idx);
    if (pos.piece_on(tgt) != NO_PIECE) return "";
    std::string out = sq_to_str(src) + sq_to_str(tgt);
    out += promo_piece_for_tier(tier);
    return out;
}

std::string ComedyEngine::in_place_promo(const Position& pos, int tier) {
    Color us = pos.side_to_move();
    auto pawns = find_pieces(pos, us, PAWN);
    if (pawns.empty()) return "";
    Square src = pawns[rand_int(0, static_cast<int>(pawns.size()) - 1)];
    std::string out = sq_to_str(src) + sq_to_str(src);
    out += promo_piece_for_tier(tier);
    return out;
}

std::string ComedyEngine::steal_piece(const Position& pos) {
    Color us = pos.side_to_move();
    Color them = ~us;
    auto their_pieces = find_all_pieces(pos, them);
    auto empty = find_empty_squares(pos);

    if (their_pieces.empty() || empty.empty()) return "";
    Square steal_sq = SQ_NONE;
    for (PieceType pt : { QUEEN, ROOK, BISHOP, KNIGHT }) {
        auto pieces = find_pieces(pos, them, pt);
        if (!pieces.empty()) {
            steal_sq = pieces[rand_int(0, static_cast<int>(pieces.size()) - 1)];
            break;
        }
    }

    if (steal_sq == SQ_NONE) {
        for (Square sq : their_pieces) {
            if (type_of(pos.piece_on(sq)) != KING) {
                steal_sq = sq;
                break;
            }
        }
    }
    if (steal_sq == SQ_NONE) return "";
    Square tgt = empty[rand_int(0, static_cast<int>(empty.size()) - 1)];

    return sq_to_str(steal_sq) + sq_to_str(tgt);
}

std::string ComedyEngine::kidnap_king(const Position& pos) {
    Color them = ~pos.side_to_move();
    Square opp_king = pos.king_square(them);
    auto empty = find_empty_squares(pos);

    if (empty.empty()) return "";
    Square corners[] = {SQ_A1, SQ_H1, SQ_A8, SQ_H8};
    Square tgt = SQ_NONE;
    for (Square c : corners) {
        if (pos.piece_on(c) == NO_PIECE) {
            tgt = c;
            break;
        }
    }
    if (tgt == SQ_NONE) {
        tgt = empty[rand_int(0, static_cast<int>(empty.size()) - 1)];
    }

    return sq_to_str(opp_king) + sq_to_str(tgt);
}

}
