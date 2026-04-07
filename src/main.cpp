#include "bitboard.h"
#include "comedy.h"
#include "evaluator.h"
#include "hce_evaluator.h"
#include "movegen.h"
#include "perft.h"
#include "position.h"
#include "process_hack.h"
#include "search.h"
#include "zobrist.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct ForcedFenPosition {
    std::array<char, 64> board{};
    char side_to_move = 'w';
    std::string castling = "-";
    std::string ep = "-";
    int rule50 = 0;
    int fullmove = 1;

    explicit ForcedFenPosition(const std::string& fen) {
        std::istringstream fen_stream(fen);
        std::string board_part;

        fen_stream >> board_part >> side_to_move >> castling >> ep >> rule50 >> fullmove;
        board.fill(0);

        int rank = 7;
        int file = 0;
        for (char c : board_part) {
            if (c == '/') {
                --rank;
                file = 0;
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c))) {
                file += c - '0';
                continue;
            }

            board[rank * 8 + file] = c;
            ++file;
        }
    }

    static int sq_idx(const std::string& sq) {
        if (sq.size() != 2 || sq[0] < 'a' || sq[0] > 'h' || sq[1] < '1' || sq[1] > '8') {
            return -1;
        }

        return (sq[1] - '1') * 8 + (sq[0] - 'a');
    }

    static std::string sq_name(int idx) {
        std::string sq = "a1";
        sq[0] = static_cast<char>('a' + idx % 8);
        sq[1] = static_cast<char>('1' + idx / 8);
        return sq;
    }

    static int rook_castle_sq(char right) {
        switch (right) {
            case 'K': return sq_idx("h1");
            case 'Q': return sq_idx("a1");
            case 'k': return sq_idx("h8");
            case 'q': return sq_idx("a8");
            default: return -1;
        }
    }

    void drop_castling_for_color(bool white) {
        castling.erase(std::remove_if(castling.begin(), castling.end(), [white](char c) {
                           return white ? std::isupper(static_cast<unsigned char>(c))
                                        : std::islower(static_cast<unsigned char>(c));
                       }),
                       castling.end());
        if (castling.empty()) castling = "-";
    }

    void drop_castling_for_sq(int sq) {
        if (sq < 0) return;

        castling.erase(std::remove_if(castling.begin(), castling.end(), [sq](char c) {
                           return rook_castle_sq(c) == sq;
                       }),
                       castling.end());
        if (castling.empty()) castling = "-";
    }

    void advance_turn(bool pawn_move, bool capture) {
        rule50 = (pawn_move || capture) ? 0 : rule50 + 1;

        if (side_to_move == 'b') ++fullmove;
        side_to_move = side_to_move == 'w' ? 'b' : 'w';
    }

    void force_apply(const std::string& uci) {
        if (uci.size() < 4) {
            advance_turn(false, false);
            return;
        }

        int from = sq_idx(uci.substr(0, 2));
        int to = sq_idx(uci.substr(2, 2));
        if (from < 0 || to < 0) {
            advance_turn(false, false);
            return;
        }

        char moving = board[from];
        char captured = board[to];
        bool pawn_move = moving == 'P' || moving == 'p';
        bool capture = captured != 0;

        ep = "-";

        if (moving == 'K') drop_castling_for_color(true);
        else if (moving == 'k') drop_castling_for_color(false);

        drop_castling_for_sq(from);
        drop_castling_for_sq(to);

        if (moving && std::tolower(static_cast<unsigned char>(moving)) == 'p'
            && board[to] == 0 && from % 8 != to % 8) {
            const int ep_cap = side_to_move == 'w' ? to - 8 : to + 8;
            if (ep_cap >= 0 && ep_cap < 64) {
                capture = board[ep_cap] != 0;
                board[ep_cap] = 0;
            }
        }

        if ((moving == 'K' || moving == 'k') && std::abs((to % 8) - (from % 8)) == 2) {
            const int rank = from / 8;
            const bool king_side = to > from;
            const int rook_from = king_side ? rank * 8 + 7 : rank * 8;
            const int rook_to = king_side ? rank * 8 + 5 : rank * 8 + 3;

            board[rook_to] = board[rook_from];
            board[rook_from] = 0;
        }

        board[from] = 0;

        char placed = moving;
        if (uci.size() >= 5 && moving) {
            char promo = static_cast<char>(std::tolower(static_cast<unsigned char>(uci[4])));
            if (promo == 'n' || promo == 'b' || promo == 'r' || promo == 'q') {
                placed = side_to_move == 'w' ? static_cast<char>(std::toupper(promo)) : promo;
            }
        }

        board[to] = placed;

        if (pawn_move && from % 8 == to % 8 && std::abs(to - from) == 16) {
            ep = sq_name((from + to) / 2);
        }

        advance_turn(pawn_move, capture);
    }

    std::string fen() const {
        std::ostringstream out;

        for (int rank = 7; rank >= 0; --rank) {
            int empty = 0;
            for (int file = 0; file < 8; ++file) {
                char piece = board[rank * 8 + file];
                if (!piece) {
                    ++empty;
                    continue;
                }

                if (empty) {
                    out << empty;
                    empty = 0;
                }
                out << piece;
            }

            if (empty) out << empty;
            if (rank) out << '/';
        }

        out << ' ' << side_to_move
            << ' ' << (castling.empty() ? "-" : castling)
            << ' ' << ep
            << ' ' << rule50
            << ' ' << fullmove;
        return out.str();
    }
};

std::vector<std::string> split_tokens(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

std::string join_pv(const std::vector<makaira::Move>& pv) {
    std::string out;
    for (const makaira::Move move : pv) {
        if (!out.empty()) out += ' ';
        out += makaira::move_to_uci(move);
    }
    return out;
}

bool handle_position(makaira::Position& pos, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return false;

    std::size_t i = 1;
    std::string fen;
    if (tokens[i] == "startpos") {
        fen = makaira::CHESS_STARTPOS_FEN;
        ++i;
    } else if (tokens[i] == "fen") {
        ++i;
        int fields = 0;
        while (i < tokens.size() && tokens[i] != "moves" && fields < 6) {
            if (!fen.empty()) fen += ' ';
            fen += tokens[i++];
            ++fields;
        }
    } else {
        return false;
    }

    ForcedFenPosition forced(fen);

    if (i < tokens.size() && tokens[i] == "moves") {
        ++i;
        for (; i < tokens.size(); ++i) {
            forced.force_apply(tokens[i]);
        }
    }

    return pos.set_from_fen(forced.fen());
}

int parse_int(const std::string& s, int fallback) {
    try {
        return std::stoi(s);
    } catch (...) {
        return fallback;
    }
}

bool parse_bool(const std::string& s) {
    return s == "1" || s == "true" || s == "on";
}

makaira::SearchLimits parse_go_limits(const std::vector<std::string>& tokens) {
    makaira::SearchLimits limits;
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        auto next_int = [&](int fallback) {
            return (i + 1 >= tokens.size()) ? fallback : parse_int(tokens[++i], fallback);
        };
        if (t == "depth") limits.depth = next_int(0);
        else if (t == "nodes") limits.nodes = static_cast<std::uint64_t>(std::max(0, next_int(0)));
        else if (t == "movetime") limits.movetime_ms = next_int(-1);
        else if (t == "wtime") limits.wtime_ms = next_int(-1);
        else if (t == "btime") limits.btime_ms = next_int(-1);
        else if (t == "winc") limits.winc_ms = next_int(0);
        else if (t == "binc") limits.binc_ms = next_int(0);
        else if (t == "movestogo") limits.movestogo = next_int(0);
        else if (t == "ponder") limits.ponder = true;
        else if (t == "infinite") limits.infinite = true;
    }
    return limits;
}

void print_uci_score(int score) {
    if (std::abs(score) >= makaira::VALUE_MATE - makaira::MAX_PLY) {
        const int sign = score > 0 ? 1 : -1;
        const int mate_ply = makaira::VALUE_MATE - std::abs(score);
        const int mate_moves = (mate_ply + 1) / 2;
        std::cout << "score mate " << sign * mate_moves;
        return;
    }
    std::cout << "score cp " << score;
}

}  // namespace

int main() {
    makaira::attacks::init();
    makaira::init_zobrist();

    makaira::HCEEvaluator evaluator;
    makaira::Searcher searcher(evaluator);
    makaira::SearchConfig search_config{};
    searcher.set_search_config(search_config);

    int move_overhead_ms = 30;
    makaira::Position position;
    position.set_startpos();

    guillemot::ComedyEngine comedy;
    guillemot::ProcessHack hack;
    bool hack_init_done = false;
    int guillemot_level = 7;
    bool boss_mode = true;

    comedy.set_level(guillemot_level);
    comedy.set_boss_mode(boss_mode);

    auto do_boss_move = [&](int tier) -> bool {
        const std::string boss_move = comedy.generate_boss_move(position, tier);
        if (boss_move.empty()) {
            return false;
        }

        if (hack.ready() && hack.apply_override()) {
            std::cout << "bestmove " << boss_move << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            hack.remove_override();
            return true;
        }

        std::cout << "bestmove " << boss_move << std::endl;
        return true;
    };

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line == "uci") {
            std::cout << "id name Guillemot\n";
            std::cout << "id author MARLIN\n";
            std::cout << "option name Hash type spin default 32 min 1 max 65536\n";
            std::cout << "option name Move Overhead type spin default 30 min 0 max 10000\n";
            std::cout << "option name Boss Level type spin default 7 min 1 max 7\n";
            std::cout << "option name Boss Mode type check default true\n";
            std::cout << "uciok\n";
        } else if (line == "isready") {
            std::cout << "readyok\n";
        } else if (line == "ucinewgame") {
            position.set_startpos();
            searcher.clear_hash();
            searcher.clear_heuristics();
            hack_init_done = false;
        } else if (line.rfind("setoption", 0) == 0) {
            const auto tokens = split_tokens(line);
            std::string name, value;
            bool pn = false, pv = false;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "name") { pn = true; pv = false; continue; }
                if (tokens[i] == "value") { pn = false; pv = true; continue; }
                if (pn) {
                    if (!name.empty()) name += ' ';
                    name += tokens[i];
                } else if (pv) {
                    if (!value.empty()) value += ' ';
                    value += tokens[i];
                }
            }

            std::string name_lc = name, value_lc = value;
            for (char& c : name_lc) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (char& c : value_lc) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (name_lc == "hash") {
                int mb = std::clamp(parse_int(value, 32), 1, 65536);
                searcher.set_hash_size_mb(static_cast<std::size_t>(mb));
            } else if (name_lc == "move overhead") {
                move_overhead_ms = std::clamp(parse_int(value, 30), 0, 10000);
            } else if (name_lc == "boss level") {
                guillemot_level = std::clamp(parse_int(value, 7), 1, 7);
                comedy.set_level(guillemot_level);
            } else if (name_lc == "boss mode") {
                boss_mode = parse_bool(value_lc);
                comedy.set_boss_mode(boss_mode);
            }
        } else if (line.rfind("position", 0) == 0) {
            const auto tokens = split_tokens(line);
            if (!handle_position(position, tokens)) {
                position.set_startpos();
            }
        } else if (line.rfind("go", 0) == 0) {
            const auto tokens = split_tokens(line);
            auto limits = parse_go_limits(tokens);
            limits.move_overhead_ms = move_overhead_ms;

            if (boss_mode && !hack_init_done) {
                hack_init_done = true;
                hack.init();
            }

            int level_depth = comedy.honest_depth();
            if (level_depth > 0 && (limits.depth <= 0 || limits.depth > level_depth)) {
                if (limits.wtime_ms <= 0 && limits.btime_ms <= 0 && limits.movetime_ms <= 0) {
                    limits.depth = level_depth;
                }
            }
            if (limits.depth <= 0 && limits.movetime_ms <= 0 && limits.nodes == 0
                && limits.wtime_ms <= 0 && limits.btime_ms <= 0 && !limits.infinite) {
                limits.depth = (std::max)(level_depth, 4);
            }

            const auto result = searcher.search(position, limits,
                [](const makaira::SearchIterationInfo& info) {
                    std::cout << "info depth " << info.depth
                              << " seldepth " << info.seldepth << " ";
                    print_uci_score(info.score);
                    std::cout << " nodes " << info.nodes
                              << " time " << info.time_ms
                              << " nps " << info.nps;
                    if (!info.pv.empty())
                        std::cout << " pv " << join_pv(info.pv);
                    std::cout << "\n";
                });

            int eval = result.score;
            int tier = comedy.chaos_tier(eval);

            if (tier > 0 && guillemot_level == 7) {
                if (do_boss_move(tier)) {
                    continue;
                }
            }

            if (tier == 0 && guillemot_level < 7) {
                makaira::MoveList moves;
                makaira::generate_legal(position, moves);

                std::vector<makaira::Move> legal;
                for (int i = 0; i < moves.count; ++i) {
                    legal.push_back(moves[i]);
                }

                if (!legal.empty()) {
                    makaira::Move chosen;
                    std::mt19937 rng(static_cast<unsigned>(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
                    const int best_move_chance = comedy.best_move_chance();

                    if (std::uniform_int_distribution<int>(1, 100)(rng) <= best_move_chance) {
                        chosen = result.best_move;
                    } else {
                        std::uniform_int_distribution<int> dist(0, static_cast<int>(legal.size()) - 1);
                        chosen = legal[dist(rng)];
                    }

                    std::cout << "bestmove " << makaira::move_to_uci(chosen) << std::endl;
                    continue;
                }
            }

            std::cout << "bestmove " << makaira::move_to_uci(result.best_move) << std::endl;
        } else if (line == "stop") {
        } else if (line == "quit") {
            if (hack.ready()) hack.remove_override();
            break;
        }
    }

    return 0;
}
