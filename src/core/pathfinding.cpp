#include "smac/core/pathfinding.hpp"

#include <algorithm>
#include <limits>
#include <queue>

namespace smac::core {
std::vector<MapPosition> find_path(const WorldMap& map, MapPosition start, MapPosition goal,
                                   std::int32_t budget) {
    if (!map.valid(start) || !map.valid(goal) || budget < 0)
        return {};
    const auto count =
        static_cast<std::size_t>(map.width() / 2) * static_cast<std::size_t>(map.height());
    std::vector<int> dist(count, std::numeric_limits<int>::max());
    std::vector<MapPosition> prev(count, {-1, -1});
    using Q = std::pair<int, MapPosition>;
    struct Greater {
        bool operator()(const Q& a, const Q& b) const {
            return a.first > b.first;
        }
    };
    std::priority_queue<Q, std::vector<Q>, Greater> q;
    dist[map.index(start)] = 0;
    q.push({0, start});
    while (!q.empty()) {
        auto [d, p] = q.top();
        q.pop();
        if (d != dist[map.index(p)])
            continue;
        if (p == goal)
            break;
        for (auto n : map.neighbors(p)) {
            int c = movement_cost(map, p, n);
            if (c < 0)
                continue;
            int nd = d + c;
            auto i = map.index(n);
            if (nd <= budget && nd < dist[i]) {
                dist[i] = nd;
                prev[i] = p;
                q.push({nd, n});
            }
        }
    }
    if (dist[map.index(goal)] == std::numeric_limits<int>::max())
        return {};
    std::vector<MapPosition> path;
    for (auto p = goal;; p = prev[map.index(p)]) {
        path.push_back(p);
        if (p == start)
            break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace smac::core
