#pragma once

#include "position.h"

#include <cstdint>
#include <string>
#include <vector>

namespace makaira {

std::uint64_t perft(Position& pos, int depth);
std::vector<std::pair<std::string, std::uint64_t>> perft_divide(Position& pos, int depth);

}  // namespace makaira