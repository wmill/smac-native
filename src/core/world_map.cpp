#include "smac/core/world_map.hpp"

#include <stdexcept>

namespace smac::core {
WorldMap::WorldMap(std::int32_t width, std::int32_t height, bool wraps)
    : width_(width), height_(height), wraps_(wraps) {
    if (width <= 0 || height <= 0 || (width & 1) != 0)
        throw std::invalid_argument("map width must be positive and even");
    tiles_.resize(static_cast<std::size_t>(width / 2) * static_cast<std::size_t>(height));
}
bool WorldMap::valid(MapPosition p) const noexcept {
    return p.y >= 0 && p.y < height_ && p.x >= 0 && p.x < width_ && ((p.x ^ p.y) & 1) == 0;
}
std::optional<MapPosition> WorldMap::normalize(MapPosition p) const noexcept {
    if (p.y < 0 || p.y >= height_)
        return std::nullopt;
    if (wraps_) {
        p.x %= width_;
        if (p.x < 0)
            p.x += width_;
    }
    return valid(p) ? std::optional<MapPosition>{p} : std::nullopt;
}
std::vector<MapPosition> WorldMap::neighbors(MapPosition p) const {
    static constexpr MapPosition offsets[]{{-2, 0}, {2, 0}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    std::vector<MapPosition> out;
    out.reserve(6);
    if (!valid(p))
        return out;
    for (auto d : offsets)
        if (auto n = normalize({p.x + d.x, p.y + d.y}))
            out.push_back(*n);
    return out;
}
std::size_t WorldMap::index(MapPosition p) const {
    if (!valid(p))
        throw std::out_of_range("invalid staggered map position");
    return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(width_ / 2) +
           static_cast<std::size_t>(p.x / 2);
}
Tile& WorldMap::at(MapPosition p) {
    return tiles_.at(index(p));
}
const Tile& WorldMap::at(MapPosition p) const {
    return tiles_.at(index(p));
}
} // namespace smac::core
