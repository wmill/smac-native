#pragma once

#include "smac/core/world_map.hpp"

#include <array>
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

struct TerrainNormal {
    double x{};
    double y{};
    double z{};
};

enum class TerrainVertex : std::size_t { center, top, right, bottom, left, count };
enum class TerrainSurface { ground, water, visible };

struct TerrainTileGeometry {
    std::array<double, static_cast<std::size_t>(TerrainVertex::count)> elevations{};
    std::array<TerrainNormal, static_cast<std::size_t>(TerrainVertex::count)> normals{};
};

struct ProjectedTerrainTile {
    std::array<ScreenPoint, static_cast<std::size_t>(TerrainVertex::count)> points{};
    std::array<TerrainNormal, static_cast<std::size_t>(TerrainVertex::count)> normals{};
    std::array<double, static_cast<std::size_t>(TerrainVertex::count)> elevations{};
};

class TerrainGeometry {
  public:
    explicit TerrainGeometry(const WorldMap& map);
    [[nodiscard]] const TerrainTileGeometry& at(MapPosition position) const;

  private:
    std::int32_t width_{};
    std::int32_t height_{};
    bool wraps_{};
    std::vector<TerrainTileGeometry> tiles_;
};

struct MapProjection {
    double origin_x{};
    double origin_y{};
    double zoom{1.0};
    double tile_width{100.0};
    double tile_height{48.0};
    double sprite_height{62.0};
    double elevation_units_per_tile_height{1750.0};

    [[nodiscard]] ScreenPoint tile_top_left(MapPosition unwrapped) const noexcept;
    [[nodiscard]] ScreenPoint tile_center(MapPosition unwrapped) const noexcept;
};

struct VisibleTile {
    MapPosition position;
    MapPosition unwrapped;
};

[[nodiscard]] bool point_in_tile(const MapProjection& projection, MapPosition unwrapped,
                                 ScreenPoint point) noexcept;
[[nodiscard]] bool point_in_tile(const ProjectedTerrainTile& tile, ScreenPoint point) noexcept;
[[nodiscard]] std::int32_t terrain_elevation(const WorldMap& map, MapPosition position);
[[nodiscard]] ProjectedTerrainTile
project_terrain_tile(const WorldMap& map, const TerrainGeometry& geometry,
                     const MapProjection& projection, MapPosition unwrapped,
                     TerrainSurface surface = TerrainSurface::visible);
[[nodiscard]] std::optional<MapPosition>
screen_to_world(const WorldMap& map, const MapProjection& projection, ScreenPoint point);
[[nodiscard]] std::optional<MapPosition> screen_to_world(const WorldMap& map,
                                                         const TerrainGeometry& geometry,
                                                         const MapProjection& projection,
                                                         ScreenPoint point);
[[nodiscard]] std::vector<VisibleTile>
visible_tiles(const WorldMap& map, const MapProjection& projection, ScreenRect viewport);
} // namespace smac::core
