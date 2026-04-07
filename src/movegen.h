#pragma once

#include "move.h"
#include "position.h"

#include <string>

namespace makaira {

void generate_pseudo_legal(const Position& pos, MoveList& out);
void generate_legal(Position& pos, MoveList& out);

Move parse_uci_move(Position& pos, const std::string& uci);
std::string move_to_uci(Move move);

}  // namespace makaira