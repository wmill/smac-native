#include "smac/core/map_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace smac::core {
ScreenPoint MapProjection::tile_top_left(MapPosition unwrapped) const noexcept {
    return {origin_x + static_cast<double>(unwrapped.x) * tile_width * zoom / 2.0,
            origin_y + static_cast<double>(unwrapped.y) * tile_height * zoom / 2.0};
}

ScreenPoint MapProjection::tile_center(MapPosition unwrapped) const noexcept {
    const auto top_left = tile_top_left(unwrapped);
    return {top_left.x + tile_width * zoom / 2.0, top_left.y + tile_height * zoom / 2.0};
}

bool point_in_tile(const MapProjection& projection, MapPosition unwrapped,
                   ScreenPoint point) noexcept {
    const auto center = projection.tile_center(unwrapped);
    const auto half_width = projection.tile_width * projection.zoom / 2.0;
    const auto half_height = projection.tile_height * projection.zoom / 2.0;
    if (half_width <= 0.0 || half_height <= 0.0)
        return false;
    return std::abs(point.x - center.x) / half_width + std::abs(point.y - center.y) / half_height <=
           1.0 + 1e-9;
}

std::optional<MapPosition> screen_to_world(const WorldMap& map, const MapProjection& projection,
                                           ScreenPoint point) {
    if (projection.zoom <= 0.0 || projection.tile_width <= 0.0 || projection.tile_height <= 0.0)
        return std::nullopt;
    const auto half_width = projection.tile_width * projection.zoom / 2.0;
    const auto half_height = projection.tile_height * projection.zoom / 2.0;
    const auto approximate_x =
        static_cast<std::int32_t>(std::floor((point.x - projection.origin_x) / half_width));
    const auto approximate_y =
        static_cast<std::int32_t>(std::floor((point.y - projection.origin_y) / half_height));
    double best_distance = std::numeric_limits<double>::infinity();
    std::optional<MapPosition> best;
    for (std::int32_t y = approximate_y - 2; y <= approximate_y + 2; ++y) {
        for (std::int32_t x = approximate_x - 2; x <= approximate_x + 2; ++x) {
            if (((x ^ y) & 1) != 0 || !point_in_tile(projection, {x, y}, point))
                continue;
            const auto normalized = map.normalize({x, y});
            if (!normalized)
                continue;
            const auto center = projection.tile_center({x, y});
            const auto distance = std::abs(point.x - center.x) / half_width +
                                  std::abs(point.y - center.y) / half_height;
            if (distance < best_distance) {
                best_distance = distance;
                best = normalized;
            }
        }
    }
    return best;
}

std::vector<VisibleTile> visible_tiles(const WorldMap& map, const MapProjection& projection,
                                       ScreenRect viewport) {
    std::vector<VisibleTile> result;
    if (projection.zoom <= 0.0 || viewport.width <= 0.0 || viewport.height <= 0.0)
        return result;
    const auto half_width = projection.tile_width * projection.zoom / 2.0;
    const auto half_height = projection.tile_height * projection.zoom / 2.0;
    const auto first_x =
        static_cast<std::int32_t>(std::floor((viewport.x - projection.origin_x) / half_width)) - 2;
    const auto last_x = static_cast<std::int32_t>(std::ceil(
                            (viewport.x + viewport.width - projection.origin_x) / half_width)) +
                        2;
    const auto first_y = std::max<std::int32_t>(
        0, static_cast<std::int32_t>(std::floor((viewport.y - projection.origin_y) / half_height)) -
               2);
    const auto last_y = std::min<std::int32_t>(
        map.height() - 1, static_cast<std::int32_t>(std::ceil(
                              (viewport.y + viewport.height - projection.origin_y) / half_height)) +
                              2);
    std::set<std::pair<MapPosition, std::int32_t>> seen;
    for (std::int32_t y = first_y; y <= last_y; ++y) {
        auto x = first_x;
        if (((x ^ y) & 1) != 0)
            ++x;
        for (; x <= last_x; x += 2) {
            const auto normalized = map.normalize({x, y});
            if (!normalized)
                continue;
            const auto wrap_copy = (x - normalized->x) / map.width();
            if (seen.emplace(*normalized, wrap_copy).second)
                result.push_back({*normalized, {x, y}});
        }
    }
    std::sort(result.begin(), result.end(), [](const VisibleTile& left, const VisibleTile& right) {
        if (left.unwrapped.y != right.unwrapped.y)
            return left.unwrapped.y < right.unwrapped.y;
        return left.unwrapped.x < right.unwrapped.x;
    });
    return result;
}
} // namespace smac::core
