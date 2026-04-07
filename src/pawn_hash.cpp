#include "pawn_hash.h"

namespace makaira {

PawnHashTable::PawnHashTable(std::size_t entries) {
    resize(entries);
}

void PawnHashTable::resize(std::size_t entries) {
    std::size_t size = 1;
    while (size < entries) {
        size <<= 1;
    }

    table_.assign(size, PawnHashEntry{});
}

void PawnHashTable::clear() {
    for (auto& e : table_) {
        e = PawnHashEntry{};
    }
}

std::size_t PawnHashTable::index(Key key) const {
    return static_cast<std::size_t>(key) & (table_.size() - 1);
}

const PawnHashEntry* PawnHashTable::probe(Key key) const {
    if (table_.empty()) {
        return nullptr;
    }

    const PawnHashEntry& e = table_[index(key)];
    return e.key == key ? &e : nullptr;
}

void PawnHashTable::store(const PawnHashEntry& entry) {
    if (table_.empty()) {
        return;
    }

    table_[index(entry.key)] = entry;
}

}  // namespace makaira