#include "smac/core/pathfinding.hpp"

#include <algorithm>
#include <limits>
#include <queue>

namespace smac::core {
std::vector<MapPosition> find_path(const GameState& state, UnitId unit_id, MapPosition goal,
                                   std::int32_t budget) {
    const auto unit = std::find_if(state.units().begin(), state.units().end(),
                                   [&](const Unit& value) { return value.id == unit_id; });
    if (unit == state.units().end() || !state.map().valid(goal) || budget < 0)
        return {};
    const auto start = unit->position;
    const auto count = static_cast<std::size_t>(state.map().width() / 2) *
                       static_cast<std::size_t>(state.map().height());
    std::vector<int> distance(count, std::numeric_limits<int>::max());
    std::vector<MapPosition> previous(count, {-1, -1});
    using QueueEntry = std::pair<int, MapPosition>;
    struct Greater {
        bool operator()(const QueueEntry& left, const QueueEntry& right) const {
            if (left.first != right.first)
                return left.first > right.first;
            return left.second > right.second;
        }
    };
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, Greater> queue;
    distance[state.map().index(start)] = 0;
    queue.push({0, start});
    while (!queue.empty()) {
        auto [cost, position] = queue.top();
        queue.pop();
        if (cost != distance[state.map().index(position)])
            continue;
        if (position == goal)
            break;
        for (const auto neighbor : state.map().neighbors(position)) {
            const auto move = evaluate_move(state, *unit, position, neighbor);
            if (!move.legal())
                continue;
            const auto next_cost = cost + move.cost;
            const auto index = state.map().index(neighbor);
            if (next_cost <= budget && next_cost < distance[index]) {
                distance[index] = next_cost;
                previous[index] = position;
                queue.push({next_cost, neighbor});
            }
        }
    }
    if (distance[state.map().index(goal)] == std::numeric_limits<int>::max())
        return {};
    std::vector<MapPosition> path;
    for (auto position = goal;; position = previous[state.map().index(position)]) {
        path.push_back(position);
        if (position == start)
            break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace smac::core
