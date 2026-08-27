#include "smac/core/map_geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace smac::core {
namespace {
constexpr auto vertex_count = static_cast<std::size_t>(TerrainVertex::count);
constexpr std::array triangle_vertices{
    std::array{TerrainVertex::center, TerrainVertex::top, TerrainVertex::right},
    std::array{TerrainVertex::center, TerrainVertex::right, TerrainVertex::bottom},
    std::array{TerrainVertex::center, TerrainVertex::bottom, TerrainVertex::left},
    std::array{TerrainVertex::center, TerrainVertex::left, TerrainVertex::top},
};
constexpr std::array corner_samples{
    std::array{MapPosition{0, 0}, MapPosition{0, -2}, MapPosition{-1, -1}, MapPosition{1, -1}},
    std::array{MapPosition{0, 0}, MapPosition{2, 0}, MapPosition{1, -1}, MapPosition{1, 1}},
    std::array{MapPosition{0, 0}, MapPosition{0, 2}, MapPosition{1, 1}, MapPosition{-1, 1}},
    std::array{MapPosition{0, 0}, MapPosition{-2, 0}, MapPosition{-1, 1}, MapPosition{-1, -1}},
};

std::size_t vertex_index(TerrainVertex vertex) {
    return static_cast<std::size_t>(vertex);
}

TerrainNormal normalize(TerrainNormal value) {
    const auto length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 1e-12)
        return {0.0, 0.0, 1.0};
    return {value.x / length, value.y / length, value.z / length};
}

struct WorldPoint {
    double x{};
    double y{};
    double z{};
};

TerrainNormal face_normal(WorldPoint a, WorldPoint b, WorldPoint c) {
    const WorldPoint ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const WorldPoint ac{c.x - a.x, c.y - a.y, c.z - a.z};
    return normalize(
        {ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x});
}

std::pair<std::int32_t, std::int32_t> vertex_key(MapPosition position, TerrainVertex vertex,
                                                 std::int32_t width, bool wraps) {
    std::int32_t x = position.x + 1;
    std::int32_t y = position.y + 1;
    switch (vertex) {
    case TerrainVertex::center:
        break;
    case TerrainVertex::top:
        --y;
        break;
    case TerrainVertex::right:
        ++x;
        break;
    case TerrainVertex::bottom:
        ++y;
        break;
    case TerrainVertex::left:
        --x;
        break;
    case TerrainVertex::count:
        break;
    }
    if (wraps) {
        x %= width;
        if (x < 0)
            x += width;
    }
    return {x, y};
}

WorldPoint world_point(MapPosition position, TerrainVertex vertex, double elevation) {
    auto [x, y] = vertex_key(position, vertex, std::numeric_limits<std::int32_t>::max(), false);
    return {static_cast<double>(x) / 2.0, static_cast<double>(y) / 2.0, elevation / 1750.0};
}

bool point_in_triangle(ScreenPoint point, ScreenPoint a, ScreenPoint b, ScreenPoint c) {
    const auto edge = [](ScreenPoint p, ScreenPoint q, ScreenPoint r) {
        return (r.x - p.x) * (q.y - p.y) - (r.y - p.y) * (q.x - p.x);
    };
    const auto ab = edge(a, b, point);
    const auto bc = edge(b, c, point);
    const auto ca = edge(c, a, point);
    constexpr double epsilon = 1e-7;
    const bool negative = ab < -epsilon || bc < -epsilon || ca < -epsilon;
    const bool positive = ab > epsilon || bc > epsilon || ca > epsilon;
    return !(negative && positive);
}
} // namespace

std::int32_t terrain_elevation(const WorldMap& map, MapPosition position) {
    const auto normalized = map.normalize(position);
    if (!normalized)
        throw std::out_of_range("terrain elevation position is outside the map");
    const auto& tile = map.at(*normalized);
    constexpr std::array<std::int32_t, 11> elevation_detail{0,   20,  40,  60,  80, 100,
                                                            120, 140, 160, 180, 200};
    const auto contour = static_cast<std::int32_t>(tile.contour);
    auto elevation = 50 * (contour - elevation_detail[3] - map.sea_level());
    if (contour <= elevation_detail[std::min<std::size_t>(tile.altitude(), 10)]) {
        elevation += 10;
    } else {
        elevation += (normalized->x * 113 + normalized->y * 217 + map.sea_level() * 301) % 50;
    }
    return std::clamp(elevation, -3000, 3500);
}

TerrainGeometry::TerrainGeometry(const WorldMap& map)
    : width_(map.width()), height_(map.height()), wraps_(map.wraps()), tiles_(map.tiles().size()) {
    std::vector<double> centers(map.tiles().size());
    for (std::int32_t y = 0; y < height_; ++y) {
        for (std::int32_t x = y & 1; x < width_; x += 2) {
            const MapPosition position{x, y};
            centers[map.index(position)] = terrain_elevation(map, position);
        }
    }

    constexpr double maximum_slope = 650.0;
    constexpr std::array forward_offsets{MapPosition{2, 0}, MapPosition{1, 1}, MapPosition{0, 2},
                                         MapPosition{-1, 1}};
    for (std::size_t pass = 0; pass < 32; ++pass) {
        bool changed = false;
        for (std::int32_t y = 0; y < height_; ++y) {
            for (std::int32_t x = y & 1; x < width_; x += 2) {
                const MapPosition position{x, y};
                auto& elevation = centers[map.index(position)];
                for (const auto offset : forward_offsets) {
                    const auto neighbor = map.normalize({x + offset.x, y + offset.y});
                    if (!neighbor)
                        continue;
                    auto& neighbor_elevation = centers[map.index(*neighbor)];
                    const auto difference = elevation - neighbor_elevation;
                    if (std::abs(difference) <= maximum_slope)
                        continue;
                    const auto adjustment = (std::abs(difference) - maximum_slope) / 2.0;
                    if (difference > 0.0) {
                        elevation -= adjustment;
                        neighbor_elevation += adjustment;
                    } else {
                        elevation += adjustment;
                        neighbor_elevation -= adjustment;
                    }
                    if (map.at(position).terrain == Terrain::land)
                        elevation = std::max(0.0, elevation);
                    else
                        elevation = std::min(0.0, elevation);
                    if (map.at(*neighbor).terrain == Terrain::land)
                        neighbor_elevation = std::max(0.0, neighbor_elevation);
                    else
                        neighbor_elevation = std::min(0.0, neighbor_elevation);
                    changed = true;
                }
            }
        }
        if (!changed)
            break;
    }

    for (std::int32_t y = 0; y < height_; ++y) {
        for (std::int32_t x = y & 1; x < width_; x += 2) {
            const MapPosition position{x, y};
            auto& geometry = tiles_[map.index(position)];
            geometry.elevations[vertex_index(TerrainVertex::center)] = centers[map.index(position)];
            for (std::size_t corner = 0; corner < corner_samples.size(); ++corner) {
                double total = 0.0;
                std::size_t count = 0;
                bool land = false;
                bool ocean = false;
                for (const auto offset : corner_samples[corner]) {
                    const auto sample =
                        map.normalize({position.x + offset.x, position.y + offset.y});
                    if (!sample)
                        continue;
                    const auto& sample_tile = map.at(*sample);
                    land |= sample_tile.terrain == Terrain::land;
                    ocean |= sample_tile.terrain == Terrain::ocean;
                    total += centers[map.index(*sample)];
                    ++count;
                }
                auto elevation = count == 0 ? 0.0 : total / static_cast<double>(count);
                if (land && ocean)
                    elevation = 0.0;
                else if (land)
                    elevation = std::max(0.0, elevation);
                else if (ocean)
                    elevation = std::min(0.0, elevation);
                geometry.elevations[corner + 1] = elevation;
            }
        }
    }

    std::map<std::pair<std::int32_t, std::int32_t>, TerrainNormal> normal_sums;
    for (std::int32_t y = 0; y < height_; ++y) {
        for (std::int32_t x = y & 1; x < width_; x += 2) {
            const MapPosition position{x, y};
            const auto& geometry = tiles_[map.index(position)];
            for (const auto& triangle : triangle_vertices) {
                const auto a = world_point(position, triangle[0],
                                           geometry.elevations[vertex_index(triangle[0])]);
                const auto b = world_point(position, triangle[1],
                                           geometry.elevations[vertex_index(triangle[1])]);
                const auto c = world_point(position, triangle[2],
                                           geometry.elevations[vertex_index(triangle[2])]);
                const auto normal = face_normal(a, b, c);
                for (const auto vertex : triangle) {
                    auto& sum = normal_sums[vertex_key(position, vertex, width_, wraps_)];
                    sum.x += normal.x;
                    sum.y += normal.y;
                    sum.z += normal.z;
                }
            }
        }
    }
    for (std::int32_t y = 0; y < height_; ++y) {
        for (std::int32_t x = y & 1; x < width_; x += 2) {
            const MapPosition position{x, y};
            auto& geometry = tiles_[map.index(position)];
            for (std::size_t i = 0; i < vertex_count; ++i) {
                const auto vertex = static_cast<TerrainVertex>(i);
                geometry.normals[i] =
                    normalize(normal_sums.at(vertex_key(position, vertex, width_, wraps_)));
            }
        }
    }
}

const TerrainTileGeometry& TerrainGeometry::at(MapPosition position) const {
    if (position.y < 0 || position.y >= height_)
        throw std::out_of_range("terrain geometry position is outside the map");
    if (wraps_) {
        position.x %= width_;
        if (position.x < 0)
            position.x += width_;
    }
    if (position.x < 0 || position.x >= width_ || ((position.x ^ position.y) & 1) != 0)
        throw std::out_of_range("terrain geometry position is outside the map");
    const auto index = static_cast<std::size_t>(position.y) * static_cast<std::size_t>(width_ / 2) +
                       static_cast<std::size_t>(position.x / 2);
    return tiles_.at(index);
}

void MapProjection::set_zoom_around(double new_zoom, ScreenPoint anchor) noexcept {
    if (zoom <= 0.0 || new_zoom <= 0.0)
        return;
    const auto scale = new_zoom / zoom;
    origin_x = anchor.x + (origin_x - anchor.x) * scale;
    origin_y = anchor.y + (origin_y - anchor.y) * scale;
    zoom = new_zoom;
}

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

bool point_in_tile(const ProjectedTerrainTile& tile, ScreenPoint point) noexcept {
    for (const auto& triangle : triangle_vertices) {
        if (point_in_triangle(point, tile.points[vertex_index(triangle[0])],
                              tile.points[vertex_index(triangle[1])],
                              tile.points[vertex_index(triangle[2])]))
            return true;
    }
    return false;
}

ProjectedTerrainTile project_terrain_tile(const WorldMap& map, const TerrainGeometry& geometry,
                                          const MapProjection& projection, MapPosition unwrapped,
                                          TerrainSurface surface) {
    const auto normalized = map.normalize(unwrapped);
    if (!normalized)
        throw std::out_of_range("projected terrain position is outside the map");
    const auto& source = geometry.at(*normalized);
    ProjectedTerrainTile result;
    result.elevations = source.elevations;
    result.normals = source.normals;
    if (surface == TerrainSurface::water ||
        (surface == TerrainSurface::visible && map.at(*normalized).terrain == Terrain::ocean)) {
        result.elevations.fill(0.0);
        result.normals.fill({0.0, 0.0, 1.0});
    }
    const auto top_left = projection.tile_top_left(unwrapped);
    const auto width = projection.tile_width * projection.zoom;
    const auto height = projection.tile_height * projection.zoom;
    result.points = {
        ScreenPoint{top_left.x + width / 2.0, top_left.y + height / 2.0},
        ScreenPoint{top_left.x + width / 2.0, top_left.y},
        ScreenPoint{top_left.x + width, top_left.y + height / 2.0},
        ScreenPoint{top_left.x + width / 2.0, top_left.y + height},
        ScreenPoint{top_left.x, top_left.y + height / 2.0},
    };
    for (std::size_t i = 0; i < result.points.size(); ++i) {
        result.points[i].y -= result.elevations[i] / projection.elevation_units_per_tile_height *
                              projection.tile_height * projection.zoom;
    }
    return result;
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

std::optional<MapPosition> screen_to_world(const WorldMap& map, const TerrainGeometry& geometry,
                                           const MapProjection& projection, ScreenPoint point) {
    if (projection.zoom <= 0.0 || projection.tile_width <= 0.0 || projection.tile_height <= 0.0 ||
        projection.elevation_units_per_tile_height <= 0.0)
        return std::nullopt;
    const auto half_width = projection.tile_width * projection.zoom / 2.0;
    const auto half_height = projection.tile_height * projection.zoom / 2.0;
    const auto approximate_x =
        static_cast<std::int32_t>(std::floor((point.x - projection.origin_x) / half_width));
    const auto approximate_y =
        static_cast<std::int32_t>(std::floor((point.y - projection.origin_y) / half_height));
    double best_distance = std::numeric_limits<double>::infinity();
    std::optional<MapPosition> best;
    for (std::int32_t y = approximate_y - 7; y <= approximate_y + 7; ++y) {
        for (std::int32_t x = approximate_x - 3; x <= approximate_x + 3; ++x) {
            if (((x ^ y) & 1) != 0)
                continue;
            const auto normalized = map.normalize({x, y});
            if (!normalized)
                continue;
            const auto tile = project_terrain_tile(map, geometry, projection, {x, y});
            if (!point_in_tile(tile, point))
                continue;
            const auto center = tile.points[vertex_index(TerrainVertex::center)];
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
    constexpr std::int32_t elevation_margin_rows = 5;
    const auto first_y = std::max<std::int32_t>(
        0, static_cast<std::int32_t>(std::floor((viewport.y - projection.origin_y) / half_height)) -
               2 - elevation_margin_rows);
    const auto last_y = std::min<std::int32_t>(
        map.height() - 1, static_cast<std::int32_t>(std::ceil(
                              (viewport.y + viewport.height - projection.origin_y) / half_height)) +
                              2 + elevation_margin_rows);
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
