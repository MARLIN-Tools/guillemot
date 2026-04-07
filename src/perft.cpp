#include "perft.h"

#include "movegen.h"

namespace makaira {

std::uint64_t perft(Position& pos, int depth) {
    if (depth <= 0) {
        return 1;
    }

    MoveList moves;
    generate_pseudo_legal(pos, moves);

    std::uint64_t nodes = 0;
    for (int i = 0; i < moves.count; ++i) {
        const Move m = moves[i];
        if (!pos.make_move(m)) {
            continue;
        }
        nodes += perft(pos, depth - 1);
        pos.unmake_move();
    }
    return nodes;
}

std::vector<std::pair<std::string, std::uint64_t>> perft_divide(Position& pos, int depth) {
    std::vector<std::pair<std::string, std::uint64_t>> out;

    MoveList moves;
    generate_pseudo_legal(pos, moves);

    for (int i = 0; i < moves.count; ++i) {
        const Move m = moves[i];
        if (!pos.make_move(m)) {
            continue;
        }
        const std::uint64_t child_nodes = depth > 1 ? perft(pos, depth - 1) : 1;
        pos.unmake_move();
        out.emplace_back(move_to_uci(m), child_nodes);
    }

    return out;
}

}  // namespace makaira