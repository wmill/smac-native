#include "smac/core/game_state.hpp"

#include <algorithm>

namespace smac::core {
int movement_cost(const WorldMap& map, MapPosition from, MapPosition to) {
    const auto ns = map.neighbors(from);
    if (std::find(ns.begin(), ns.end(), to) == ns.end())
        return -1;
    const auto& tile = map.at(to);
    if (tile.terrain == Terrain::ocean)
        return -1; // first milestone unit is land-native
    return (tile.improvements & 0x4U) != 0U ? 1 : 3;
}
std::vector<Event> GameState::apply(const Command& command) {
    return std::visit(
        [this](const auto& value) -> std::vector<Event> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, EndTurn>) {
                ++turn_;
                for (auto& u : units_)
                    u.movement_remaining = u.movement_max;
                return {TurnAdvanced{turn_}};
            } else {
                auto it = std::find_if(units_.begin(), units_.end(),
                                       [&](const Unit& u) { return u.id == value.unit; });
                if (it == units_.end())
                    return {CommandRejected{"unknown unit"}};
                auto dst = map_.normalize(value.destination);
                if (!dst)
                    return {CommandRejected{"destination is outside map or parity-invalid"}};
                const int cost = movement_cost(map_, it->position, *dst);
                if (cost < 0)
                    return {CommandRejected{"destination is not a legal adjacent land tile"}};
                if (cost > it->movement_remaining)
                    return {CommandRejected{"insufficient movement points"}};
                const auto from = it->position;
                it->position = *dst;
                it->movement_remaining -= cost;
                return {UnitMoved{it->id, from, *dst, cost}};
            }
        },
        command);
}
std::uint64_t GameState::stable_hash() const noexcept {
    std::uint64_t h = 1469598103934665603ULL;
    auto add = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v & 255U);
            h *= 1099511628211ULL;
            v >>= 8;
        }
    };
    add(turn_);
    add(static_cast<std::uint64_t>(map_.width()));
    add(static_cast<std::uint64_t>(map_.height()));
    for (const auto& u : units_) {
        add(u.id);
        add(u.faction);
        add(static_cast<std::uint32_t>(u.position.x));
        add(static_cast<std::uint32_t>(u.position.y));
        add(static_cast<std::uint32_t>(u.movement_remaining));
    }
    return h;
}
} // namespace smac::core
