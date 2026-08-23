#pragma once

#include "smac/core/world_map.hpp"

#include <optional>
#include <vector>

namespace smac::core {
struct ScreenPoint {
    double x{};
    double y{};
    friend bool operator==(const ScreenPoint&, const ScreenPoint&) = default;
};

struct ScreenRect {
    double x{};
    double y{};
    double width{};
    double height{};
};

struct MapProjection {
    double origin_x{};
    double origin_y{};
    double zoom{1.0};
    double tile_width{100.0};
    double tile_height{48.0};
    double sprite_height{62.0};

    [[nodiscard]] ScreenPoint tile_top_left(MapPosition unwrapped) const noexcept;
    [[nodiscard]] ScreenPoint tile_center(MapPosition unwrapped) const noexcept;
};

struct VisibleTile {
    MapPosition position;
    MapPosition unwrapped;
};

[[nodiscard]] bool point_in_tile(const MapProjection& projection, MapPosition unwrapped,
                                 ScreenPoint point) noexcept;
[[nodiscard]] std::optional<MapPosition>
screen_to_world(const WorldMap& map, const MapProjection& projection, ScreenPoint point);
[[nodiscard]] std::vector<VisibleTile>
visible_tiles(const WorldMap& map, const MapProjection& projection, ScreenRect viewport);
} // namespace smac::core
