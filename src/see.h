#pragma once

#include "move.h"
#include "position.h"

namespace makaira {

int static_exchange_eval(const Position& pos, Move move);
int see_captured_value(const Position& pos, Move move);

}  // namespace makaira

