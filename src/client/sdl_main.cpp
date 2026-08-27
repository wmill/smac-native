#include "smac/core/game_state.hpp"
#include "smac/core/map_geometry.hpp"
#include "smac/core/pathfinding.hpp"
#include "smac/formats/atlas.hpp"
#include "smac/formats/data_directory.hpp"
#include "smac/formats/pcx.hpp"
#include "smac/formats/rules.hpp"
#include "smac/formats/terran_map.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sc = smac::core;
namespace sf = smac::formats;

namespace {
struct AtlasTexture {
    SDL_Texture* texture{};
    const sf::IndexedImage* image{};
};

struct TextCache {
    SDL_Texture* texture{};
    std::string text;
    float width{};
    float height{};

    void reset() {
        SDL_DestroyTexture(texture);
        texture = nullptr;
        text.clear();
    }
};

struct TimedEvent {
    sc::Event event;
    Uint64 expires{};
};

std::optional<sf::IndexedImage> load_image(const std::filesystem::path& path) {
    auto result = sf::load_pcx(path);
    if (auto* error = std::get_if<sf::Error>(&result)) {
        std::cerr << error->message << " at byte " << error->offset << '\n';
        return std::nullopt;
    }
    return std::get<sf::IndexedImage>(std::move(result));
}

AtlasTexture create_atlas_texture(SDL_Renderer* renderer, const sf::IndexedImage& image,
                                  std::initializer_list<std::uint8_t> transparent_indices) {
    std::vector<std::uint8_t> rgba(image.pixels.size() * 4);
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        const auto index = image.pixels[i];
        const auto& color = image.palette[index];
        rgba[i * 4] = color.red;
        rgba[i * 4 + 1] = color.green;
        rgba[i * 4 + 2] = color.blue;
        rgba[i * 4 + 3] = std::find(transparent_indices.begin(), transparent_indices.end(),
                                    index) != transparent_indices.end()
                              ? 0
                              : color.alpha;
    }
    auto* surface = SDL_CreateSurfaceFrom(static_cast<int>(image.width),
                                          static_cast<int>(image.height), SDL_PIXELFORMAT_RGBA32,
                                          rgba.data(), static_cast<int>(image.width * 4));
    if (!surface)
        return {};
    auto* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }
    return {texture, &image};
}

void draw_region(SDL_Renderer* renderer, const AtlasTexture& atlas, const sf::AtlasRegion& region,
                 std::uint8_t frame, sc::ScreenPoint top_left, float width, float height,
                 SDL_FColor color = {1.0F, 1.0F, 1.0F, 1.0F}) {
    if (!atlas.texture)
        return;
    const auto source = sf::atlas_frame(region, frame);
    const SDL_FRect src{static_cast<float>(source.x), static_cast<float>(source.y),
                        static_cast<float>(source.width), static_cast<float>(source.height)};
    const SDL_FRect destination{static_cast<float>(top_left.x), static_cast<float>(top_left.y),
                                width, height};
    SDL_SetTextureColorModFloat(atlas.texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaModFloat(atlas.texture, color.a);
    SDL_RenderTexture(renderer, atlas.texture, &src, &destination);
    SDL_SetTextureColorModFloat(atlas.texture, 1.0F, 1.0F, 1.0F);
    SDL_SetTextureAlphaModFloat(atlas.texture, 1.0F);
}

void draw_named_region(SDL_Renderer* renderer, const AtlasTexture& atlas,
                       std::span<const sf::AtlasRegion> regions, std::string_view name,
                       std::uint8_t frame, sc::ScreenPoint top_left, float width, float height,
                       SDL_FColor color = {1.0F, 1.0F, 1.0F, 1.0F}) {
    if (const auto* region = sf::find_region(regions, name))
        draw_region(renderer, atlas, *region, frame, top_left, width, height, color);
}

constexpr auto terrain_vertex_count = static_cast<std::size_t>(sc::TerrainVertex::count);
using VertexColors = std::array<SDL_FColor, terrain_vertex_count>;

constexpr VertexColors white_vertex_colors() {
    VertexColors colors{};
    colors.fill({1.0F, 1.0F, 1.0F, 1.0F});
    return colors;
}

SDL_FColor lit_color(sc::TerrainNormal normal, SDL_FColor tint) {
    struct Light {
        sc::TerrainNormal direction;
        std::array<float, 3> color;
    };
    constexpr std::array lights{
        Light{{48.227, 20.412, 57.65}, {1.6F, 1.8F, 2.0F}},
        Light{{22.412, 62.227, 43.35}, {2.0F, 1.8F, 1.6F}},
    };
    std::array<float, 3> illumination{};
    for (const auto& light : lights) {
        const auto length = std::sqrt(light.direction.x * light.direction.x +
                                      light.direction.y * light.direction.y +
                                      light.direction.z * light.direction.z);
        const auto diffuse = static_cast<float>(
            std::max(0.0, (normal.x * light.direction.x + normal.y * light.direction.y +
                           normal.z * light.direction.z) /
                              length));
        for (std::size_t channel = 0; channel < illumination.size(); ++channel)
            illumination[channel] += diffuse * light.color[channel] / lights.size();
    }
    constexpr float gamma = 1.1F;
    constexpr float contrast = 0.1F;
    return {std::clamp(tint.r * illumination[0] * gamma - contrast, 0.0F, 1.0F),
            std::clamp(tint.g * illumination[1] * gamma - contrast, 0.0F, 1.0F),
            std::clamp(tint.b * illumination[2] * gamma - contrast, 0.0F, 1.0F), tint.a};
}

VertexColors lit_colors(const sc::ProjectedTerrainTile& tile, const VertexColors& tints) {
    VertexColors colors{};
    for (std::size_t i = 0; i < colors.size(); ++i)
        colors[i] = lit_color(tile.normals[i], tints[i]);
    return colors;
}

void draw_terrain_region(SDL_Renderer* renderer, const AtlasTexture& atlas,
                         const sf::AtlasRegion& region, std::uint8_t frame,
                         const sc::ProjectedTerrainTile& tile, const VertexColors& tints) {
    if (!atlas.texture || !atlas.image)
        return;
    const auto source = sf::atlas_frame(region, frame);
    const auto image_width = static_cast<float>(atlas.image->width);
    const auto image_height = static_cast<float>(atlas.image->height);
    const auto left = static_cast<float>(source.x + 1) / image_width;
    const auto right = static_cast<float>(source.x + source.width - 2) / image_width;
    const auto upper = static_cast<float>(source.y + 1) / image_height;
    const auto lower = static_cast<float>(source.y + source.height - 2) / image_height;
    const auto middle_x = (left + right) / 2.0F;
    const auto middle_y = (upper + lower) / 2.0F;
    const auto colors = lit_colors(tile, tints);
    const auto vertex = [&](sc::TerrainVertex which, SDL_FPoint texture) {
        const auto i = static_cast<std::size_t>(which);
        return SDL_Vertex{
            {static_cast<float>(tile.points[i].x), static_cast<float>(tile.points[i].y)},
            colors[i],
            texture};
    };
    const std::array<SDL_Vertex, terrain_vertex_count> vertices{{
        vertex(sc::TerrainVertex::center, {middle_x, middle_y}),
        vertex(sc::TerrainVertex::top, {middle_x, upper}),
        vertex(sc::TerrainVertex::right, {right, middle_y}),
        vertex(sc::TerrainVertex::bottom, {middle_x, lower}),
        vertex(sc::TerrainVertex::left, {left, middle_y}),
    }};
    constexpr std::array<int, 12> indices{0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1};
    SDL_RenderGeometry(renderer, atlas.texture, vertices.data(), vertices.size(), indices.data(),
                       indices.size());
}

void draw_named_terrain_region(SDL_Renderer* renderer, const AtlasTexture& atlas,
                               std::span<const sf::AtlasRegion> regions, std::string_view name,
                               std::uint8_t frame, const sc::ProjectedTerrainTile& tile,
                               const VertexColors& tints = white_vertex_colors()) {
    if (const auto* region = sf::find_region(regions, name))
        draw_terrain_region(renderer, atlas, *region, frame, tile, tints);
}

void draw_planar_region(SDL_Renderer* renderer, const AtlasTexture& atlas,
                        const sf::AtlasRegion& region, std::uint8_t frame,
                        const sc::ProjectedTerrainTile& tile, const VertexColors& tints) {
    if (!atlas.texture || !atlas.image)
        return;
    const auto source = sf::atlas_frame(region, frame);
    const auto image_width = static_cast<float>(atlas.image->width);
    const auto image_height = static_cast<float>(atlas.image->height);
    const auto left = static_cast<float>(source.x + 1) / image_width;
    const auto right = static_cast<float>(source.x + source.width - 2) / image_width;
    const auto upper = static_cast<float>(source.y + 1) / image_height;
    const auto lower = static_cast<float>(source.y + source.height - 2) / image_height;
    const auto middle_x = (left + right) / 2.0F;
    const auto middle_y = (upper + lower) / 2.0F;
    const auto colors = lit_colors(tile, tints);
    const auto vertex = [&](sc::TerrainVertex which, SDL_FPoint texture) {
        const auto i = static_cast<std::size_t>(which);
        return SDL_Vertex{
            {static_cast<float>(tile.points[i].x), static_cast<float>(tile.points[i].y)},
            colors[i],
            texture};
    };
    const std::array vertices{
        vertex(sc::TerrainVertex::top, {middle_x, upper}),
        vertex(sc::TerrainVertex::right, {right, middle_y}),
        vertex(sc::TerrainVertex::bottom, {middle_x, lower}),
        vertex(sc::TerrainVertex::left, {left, middle_y}),
    };
    constexpr std::array<int, 6> indices{0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, atlas.texture, vertices.data(), vertices.size(), indices.data(),
                       indices.size());
}

std::uint8_t tile_variant(sc::MapPosition position, std::uint8_t frames) {
    const auto value = static_cast<std::uint32_t>(position.x * 37 + position.y * 71);
    return static_cast<std::uint8_t>(value % frames);
}

template <typename Predicate>
std::uint8_t connectivity_frame(const sc::WorldMap& map, sc::MapPosition position,
                                Predicate&& matches) {
    // The 16 stock texture cells encode four edge groups. Each group covers two of the eight
    // staggered directions, in the clockwise order used by OpenSMACX RadiusOffset[1..8].
    std::uint8_t frame = 0;
    for (std::size_t group = 0; group < 4; ++group) {
        const auto first = sc::adjacent_offsets[group * 2];
        const auto second = sc::adjacent_offsets[group * 2 + 1];
        for (const auto offset : {first, second}) {
            const auto neighbor = map.normalize({position.x + offset.x, position.y + offset.y});
            if (neighbor && matches(map.at(*neighbor))) {
                frame |= static_cast<std::uint8_t>(1U << group);
                break;
            }
        }
    }
    return frame;
}

void draw_terrain_outline(SDL_Renderer* renderer, const sc::ProjectedTerrainTile& tile,
                          SDL_Color color) {
    const auto& projected_points = tile.points;
    const std::array<SDL_FPoint, 5> points{{
        {static_cast<float>(projected_points[static_cast<std::size_t>(sc::TerrainVertex::top)].x),
         static_cast<float>(projected_points[static_cast<std::size_t>(sc::TerrainVertex::top)].y)},
        {static_cast<float>(projected_points[static_cast<std::size_t>(sc::TerrainVertex::right)].x),
         static_cast<float>(
             projected_points[static_cast<std::size_t>(sc::TerrainVertex::right)].y)},
        {static_cast<float>(
             projected_points[static_cast<std::size_t>(sc::TerrainVertex::bottom)].x),
         static_cast<float>(
             projected_points[static_cast<std::size_t>(sc::TerrainVertex::bottom)].y)},
        {static_cast<float>(projected_points[static_cast<std::size_t>(sc::TerrainVertex::left)].x),
         static_cast<float>(projected_points[static_cast<std::size_t>(sc::TerrainVertex::left)].y)},
        {static_cast<float>(projected_points[static_cast<std::size_t>(sc::TerrainVertex::top)].x),
         static_cast<float>(projected_points[static_cast<std::size_t>(sc::TerrainVertex::top)].y)},
    }};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLines(renderer, points.data(), points.size());
}

void fill_terrain(SDL_Renderer* renderer, const sc::ProjectedTerrainTile& tile, SDL_Color color) {
    constexpr auto channel_scale = 1.0F / 255.0F;
    const SDL_FColor float_color{
        static_cast<float>(color.r) * channel_scale, static_cast<float>(color.g) * channel_scale,
        static_cast<float>(color.b) * channel_scale, static_cast<float>(color.a) * channel_scale};
    std::array<SDL_Vertex, terrain_vertex_count> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i)
        vertices[i] = {{static_cast<float>(tile.points[i].x), static_cast<float>(tile.points[i].y)},
                       float_color,
                       {}};
    constexpr std::array<int, 12> indices{0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1};
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), vertices.size(), indices.data(),
                       indices.size());
}

SDL_Color faction_color(std::int8_t faction) {
    constexpr std::array colors{
        SDL_Color{110, 110, 110, 190}, SDL_Color{70, 210, 105, 210}, SDL_Color{210, 45, 45, 210},
        SDL_Color{45, 100, 220, 210},  SDL_Color{220, 180, 40, 210}, SDL_Color{180, 70, 205, 210},
        SDL_Color{60, 200, 205, 210},  SDL_Color{225, 125, 45, 210},
    };
    return colors[static_cast<std::uint8_t>(faction) % colors.size()];
}

enum class TerrainPass { ground, water, objects, fog };

void draw_tile(SDL_Renderer* renderer, const sc::GameState& game,
               const sc::TerrainGeometry& geometry, const sc::MapProjection& projection,
               const sc::VisibleTile& visible, const AtlasTexture& terrain,
               const AtlasTexture& texture, bool reveal_all, sc::FactionId viewing_faction,
               TerrainPass pass) {
    const auto& tile = game.map().at(visible.position);
    const bool explored =
        reveal_all || (tile.visibility & (1U << std::min<std::uint8_t>(viewing_faction, 7))) != 0U;
    const auto ground = sc::project_terrain_tile(game.map(), geometry, projection,
                                                 visible.unwrapped, sc::TerrainSurface::ground);
    const auto surface = sc::project_terrain_tile(game.map(), geometry, projection,
                                                  visible.unwrapped, sc::TerrainSurface::visible);
    if (pass == TerrainPass::fog) {
        if (!explored) {
            fill_terrain(renderer, surface, {5, 9, 14, 255});
            draw_terrain_outline(renderer, surface, {18, 29, 38, 255});
        }
        return;
    }
    if (!explored)
        return;

    if (pass == TerrainPass::water) {
        if (tile.terrain != sc::Terrain::ocean)
            return;
        VertexColors water_tints{};
        const auto center = static_cast<std::size_t>(sc::TerrainVertex::center);
        const auto depth = std::clamp(
            static_cast<float>((ground.elevations[center] + 2000.0) / 2000.0), 0.0F, 1.0F);
        water_tints.fill({0.6F + 0.4F * depth, 0.6F + 0.4F * depth, 0.8F + 0.2F * depth, 1.0F});
        if (const auto* region = sf::find_region(sf::texture_atlas, "water_surface"))
            draw_planar_region(renderer, texture, *region, 0, surface, water_tints);
        if ((tile.improvements & sc::tile_fungus) != 0U) {
            const auto frame =
                connectivity_frame(game.map(), visible.position, [](const sc::Tile& neighbor) {
                    return (neighbor.improvements & sc::tile_fungus) != 0U;
                });
            if (const auto* region = sf::find_region(sf::texture_atlas, "fungus_water"))
                draw_planar_region(renderer, texture, *region, frame, surface,
                                   white_vertex_colors());
        }
        return;
    }

    const auto center_index = static_cast<std::size_t>(sc::TerrainVertex::center);
    const auto flat_center = projection.tile_center(visible.unwrapped);
    const auto center_shift = surface.points[center_index].y - flat_center.y;
    const auto base_top = projection.tile_top_left(visible.unwrapped);
    const sc::ScreenPoint top{base_top.x, base_top.y + center_shift};
    const auto width = static_cast<float>(projection.tile_width * projection.zoom);
    const auto height = static_cast<float>(projection.sprite_height * projection.zoom);

    if (pass == TerrainPass::objects) {
        const auto object_color = lit_color(surface.normals[center_index], {1, 1, 1, 1});
        const auto land_suffix = tile.terrain == sc::Terrain::land ? "_land" : "_water";
        const auto draw_terrain_sprite = [&](std::uint32_t flag, std::string name) {
            if ((tile.improvements & flag) != 0U)
                draw_named_region(renderer, terrain, sf::terrain_atlas, name, 0, top, width, height,
                                  object_color);
        };
        draw_terrain_sprite(sc::tile_mine, std::string("mine") + land_suffix);
        draw_terrain_sprite(sc::tile_solar, std::string("solar") + land_suffix);
        draw_terrain_sprite(sc::tile_soil_enricher, "soil_enricher");
        for (const auto& [flag, name] : std::array{
                 std::pair{sc::tile_bunker, std::string_view{"bunker"}},
                 std::pair{sc::tile_airbase, std::string_view{"airbase"}},
                 std::pair{sc::tile_sensor, std::string_view{"sensor"}},
                 std::pair{sc::tile_monolith, std::string_view{"monolith"}},
                 std::pair{sc::tile_condenser, std::string_view{"condenser"}},
                 std::pair{sc::tile_echelon_mirror, std::string_view{"echelon_mirror"}},
                 std::pair{sc::tile_thermal_borehole, std::string_view{"thermal_borehole"}},
             }) {
            if ((tile.improvements & flag) != 0U)
                draw_named_region(renderer, terrain, sf::terrain_atlas, name, 0, top, width, height,
                                  object_color);
        }
        const auto resource_suffix = tile.terrain == sc::Terrain::land ? "_land" : "_water";
        if ((tile.improvements & sc::tile_nutrient_resource) != 0U)
            draw_named_region(renderer, terrain, sf::terrain_atlas,
                              std::string("resource_nutrient") + resource_suffix,
                              tile_variant(visible.position, 2), top, width, height, object_color);
        else if ((tile.improvements & sc::tile_mineral_resource) != 0U)
            draw_named_region(renderer, terrain, sf::terrain_atlas,
                              std::string("resource_mineral") + resource_suffix,
                              tile_variant(visible.position, 2), top, width, height, object_color);
        else if ((tile.improvements & sc::tile_energy_resource) != 0U)
            draw_named_region(renderer, terrain, sf::terrain_atlas,
                              std::string("resource_energy") + resource_suffix,
                              tile_variant(visible.position, 2), top, width, height, object_color);
        if ((tile.improvements & sc::tile_supply_pod) != 0U)
            draw_named_region(renderer, terrain, sf::terrain_atlas,
                              std::string("supply_pod") + resource_suffix,
                              tile_variant(visible.position, 3), top, width, height, object_color);
        if ((tile.landmark & 0x8U) != 0U)
            draw_named_region(renderer, terrain, sf::terrain_atlas, "uranium", 0, top, width,
                              height, object_color);
        if ((tile.landmark & 0x400U) != 0U)
            draw_named_region(renderer, terrain, sf::terrain_atlas, "geothermal", 0, top, width,
                              height, object_color);
        if (tile.territory >= 0)
            draw_terrain_outline(renderer, surface, faction_color(tile.territory));
        return;
    }

    std::string_view surface_name = "arid";
    std::uint8_t surface_frame = 0;
    switch (tile.rainfall()) {
    case sc::Rainfall::arid:
    case sc::Rainfall::invalid:
        break;
    case sc::Rainfall::moist:
        surface_name = "moist";
        break;
    case sc::Rainfall::rainy:
        surface_name = "rainy";
        break;
    }
    if (tile.terrain == sc::Terrain::land &&
        (tile.rainfall() == sc::Rainfall::moist || tile.rainfall() == sc::Rainfall::rainy)) {
        surface_frame = connectivity_frame(game.map(), visible.position, [&](const sc::Tile& n) {
            return n.terrain == sc::Terrain::land && static_cast<std::uint8_t>(n.rainfall()) >=
                                                         static_cast<std::uint8_t>(tile.rainfall());
        });
    }
    auto ground_tints = white_vertex_colors();
    if (tile.terrain == sc::Terrain::ocean)
        ground_tints.fill({0.0F, 0.2F, 0.5F, 1.0F});
    draw_named_terrain_region(renderer, texture, sf::texture_atlas, surface_name, surface_frame,
                              ground, ground_tints);

    if (tile.rockiness() == sc::Rockiness::rolling || tile.rockiness() == sc::Rockiness::rocky) {
        const auto frame =
            static_cast<std::uint8_t>(tile_variant(visible.position, 2) * 2 +
                                      (tile.rockiness() == sc::Rockiness::rocky ? 1 : 0));
        draw_named_terrain_region(renderer, texture, sf::texture_atlas, "rocks", frame, ground,
                                  ground_tints);
    }
    if ((tile.landmark & 0x40U) != 0U)
        draw_named_terrain_region(renderer, texture, sf::texture_atlas, "dunes", 0, ground,
                                  ground_tints);
    if ((tile.improvements & (sc::tile_farm | sc::tile_soil_enricher)) != 0U)
        draw_named_terrain_region(renderer, texture, sf::texture_atlas, "farm",
                                  tile_variant(visible.position, 9), ground, ground_tints);
    if ((tile.improvements & sc::tile_forest) != 0U) {
        const auto frame = connectivity_frame(game.map(), visible.position, [](const sc::Tile& n) {
            return (n.improvements & sc::tile_forest) != 0U;
        });
        draw_named_terrain_region(renderer, texture, sf::texture_atlas, "forest", frame, ground,
                                  ground_tints);
    }
    if ((tile.landmark & 0x4U) != 0U) {
        const auto frame = connectivity_frame(game.map(), visible.position, [](const sc::Tile& n) {
            return (n.landmark & 0x4U) != 0U;
        });
        draw_named_terrain_region(renderer, texture, sf::texture_atlas, "jungle", frame, ground,
                                  ground_tints);
    }
    if (tile.terrain == sc::Terrain::land && (tile.improvements & sc::tile_fungus) != 0U) {
        const auto frame = connectivity_frame(game.map(), visible.position, [](const sc::Tile& n) {
            return (n.improvements & sc::tile_fungus) != 0U;
        });
        draw_named_terrain_region(renderer, texture, sf::texture_atlas, "fungus_land", frame,
                                  ground);
    }
    if ((tile.improvements & sc::tile_river) != 0U) {
        const auto frame = connectivity_frame(game.map(), visible.position, [](const sc::Tile& n) {
            return (n.improvements & sc::tile_river) != 0U || n.terrain == sc::Terrain::ocean;
        });
        draw_named_terrain_region(renderer, texture, sf::texture_atlas, "river", frame, ground);
    }
    const auto draw_network = [&](std::uint32_t flag, std::string_view name) {
        if ((tile.improvements & flag) == 0U)
            return;
        bool connected = false;
        for (std::size_t i = 0; i < sc::adjacent_offsets.size(); ++i) {
            const auto offset = sc::adjacent_offsets[i];
            const auto neighbor = game.map().normalize(
                {visible.position.x + offset.x, visible.position.y + offset.y});
            if (!neighbor || (game.map().at(*neighbor).improvements & flag) == 0U)
                continue;
            draw_named_terrain_region(renderer, texture, sf::texture_atlas, name,
                                      static_cast<std::uint8_t>(i + 1), ground);
            connected = true;
        }
        if (!connected)
            draw_named_terrain_region(renderer, texture, sf::texture_atlas, name, 0, ground);
    };
    draw_network(sc::tile_road, "road");
    draw_network(sc::tile_mag_tube, "mag_tube");
}

std::string terrain_description(const sc::Tile& tile) {
    std::string result = tile.terrain == sc::Terrain::ocean ? "Ocean" : "Land";
    result += " alt " + std::to_string(tile.altitude());
    if (tile.terrain == sc::Terrain::land) {
        constexpr std::array rainfall{"arid", "moist", "rainy", "unknown"};
        constexpr std::array rockiness{"flat", "rolling", "rocky", "unknown"};
        result += " ";
        result += rainfall[static_cast<std::uint8_t>(tile.rainfall())];
        result += " ";
        result += rockiness[static_cast<std::uint8_t>(tile.rockiness())];
    }
    return result;
}

void update_text_cache(SDL_Renderer* renderer, TTF_Font* font, TextCache& cache,
                       const std::string& text) {
    if (!font || cache.text == text)
        return;
    cache.reset();
    cache.text = text;
    const SDL_Color color{226, 231, 202, 255};
    auto* surface = TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
    if (!surface)
        return;
    cache.width = static_cast<float>(surface->w);
    cache.height = static_cast<float>(surface->h);
    cache.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
}

void paint_region(sf::IndexedImage& image, const sf::AtlasRegion& region, std::uint8_t index,
                  bool center_only = false) {
    for (std::uint8_t frame = 0; frame < region.frames; ++frame) {
        auto rectangle = sf::atlas_frame(region, frame);
        if (center_only) {
            rectangle.x += rectangle.width / 2 - 8;
            rectangle.y += rectangle.height / 2 - 8;
            rectangle.width = 16;
            rectangle.height = 16;
        }
        for (std::int32_t y = rectangle.y; y < rectangle.y + rectangle.height; ++y) {
            for (std::int32_t x = rectangle.x; x < rectangle.x + rectangle.width; ++x) {
                image.pixels[static_cast<std::size_t>(y) * image.width +
                             static_cast<std::size_t>(x)] = index;
            }
        }
    }
}

sf::IndexedImage synthetic_atlas(std::uint8_t background) {
    sf::IndexedImage image;
    image.width = 1024;
    image.height = 768;
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height, background);
    image.palette[1] = {132, 70, 42, 255};
    image.palette[2] = {20, 90, 160, 255};
    image.palette[3] = {190, 35, 70, 255};
    image.palette[4] = {245, 245, 220, 255};
    image.palette[253] = {152, 24, 228, 255};
    image.palette[255] = {100, 16, 156, 255};
    return image;
}

int run_acceptance_check(sc::GameState& game) {
    auto& unit = game.units().front();
    const auto origin = unit.position;
    std::optional<sc::MapPosition> destination;
    for (const auto neighbor : game.map().neighbors(origin)) {
        const auto evaluation = sc::evaluate_move(game, unit, origin, neighbor);
        if (evaluation.legal() && evaluation.cost > 0 &&
            evaluation.cost <= unit.movement_remaining) {
            destination = neighbor;
            break;
        }
    }
    if (!destination) {
        std::cerr << "acceptance check found no legal positive-cost move from the spawn tile\n";
        return 1;
    }

    const auto before_preview = game.stable_hash();
    const auto preview = sc::find_path(game, unit.id, *destination, unit.movement_remaining);
    if (preview.size() != 2 || preview.front() != origin || preview.back() != *destination ||
        game.stable_hash() != before_preview) {
        std::cerr << "acceptance check route preview was invalid or mutated game state\n";
        return 1;
    }

    const auto move_events = game.apply(sc::MoveUnit{unit.id, *destination});
    const auto* moved = move_events.empty() ? nullptr : std::get_if<sc::UnitMoved>(&move_events[0]);
    if (!moved || moved->from != origin || moved->to != *destination || moved->cost <= 0 ||
        unit.position != *destination || unit.movement_remaining >= unit.movement_max) {
        std::cerr << "acceptance check did not confirm the previewed move\n";
        return 1;
    }

    std::optional<sc::MapPosition> illegal;
    const auto adjacent = game.map().neighbors(unit.position);
    for (int y = 0; y < game.map().height() && !illegal; ++y) {
        for (int x = y & 1; x < game.map().width(); x += 2) {
            const sc::MapPosition candidate{x, y};
            if (candidate != unit.position &&
                std::find(adjacent.begin(), adjacent.end(), candidate) == adjacent.end()) {
                illegal = candidate;
                break;
            }
        }
    }
    if (!illegal) {
        std::cerr << "acceptance check found no non-adjacent destination\n";
        return 1;
    }
    const auto position_before_rejection = unit.position;
    const auto rejected = game.apply(sc::MoveUnit{unit.id, *illegal});
    if (rejected.empty() || !std::holds_alternative<sc::CommandRejected>(rejected[0]) ||
        unit.position != position_before_rejection) {
        std::cerr << "acceptance check failed to reject a non-adjacent destination\n";
        return 1;
    }

    unit.movement_remaining = 0;
    const auto turn_events = game.apply(sc::EndTurn{});
    if (turn_events.empty() || !std::holds_alternative<sc::TurnAdvanced>(turn_events[0]) ||
        game.turn() != 2 || unit.movement_remaining != unit.movement_max) {
        std::cerr << "acceptance check End Turn did not restore movement\n";
        return 1;
    }
    std::cout << "acceptance OK: previewed and moved native unit " << unit.id << " from "
              << origin.x << ',' << origin.y << " to " << destination->x << ',' << destination->y
              << "; rejected illegal move; turn " << game.turn() << " restored "
              << unit.movement_remaining << " MP\n";
    return 0;
}

bool pixel_near(SDL_Surface* surface, sc::ScreenPoint point, SDL_Color expected) {
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    if (!SDL_ReadSurfacePixel(surface, static_cast<int>(point.x), static_cast<int>(point.y), &red,
                              &green, &blue, &alpha))
        return false;
    constexpr int tolerance = 4;
    const bool matches = std::abs(static_cast<int>(red) - expected.r) <= tolerance &&
                         std::abs(static_cast<int>(green) - expected.g) <= tolerance &&
                         std::abs(static_cast<int>(blue) - expected.b) <= tolerance && alpha >= 250;
    if (!matches)
        std::cerr << "pixel " << static_cast<int>(point.x) << ',' << static_cast<int>(point.y)
                  << " was " << static_cast<int>(red) << ',' << static_cast<int>(green) << ','
                  << static_cast<int>(blue) << "; expected " << static_cast<int>(expected.r) << ','
                  << static_cast<int>(expected.g) << ',' << static_cast<int>(expected.b) << '\n';
    return matches;
}

int render_synthetic_screenshot(const std::filesystem::path& output) {
    auto terrain_image = synthetic_atlas(253);
    auto texture_image = synthetic_atlas(255);
    auto unit_image = synthetic_atlas(255);
    paint_region(texture_image, *sf::find_region(sf::texture_atlas, "arid"), 1);
    paint_region(texture_image, *sf::find_region(sf::texture_atlas, "water_surface"), 2);
    paint_region(texture_image, *sf::find_region(sf::texture_atlas, "fungus_land"), 3);
    paint_region(unit_image, *sf::find_region(sf::unit_atlas, "mind_worm"), 4, true);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "synthetic screenshot SDL initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("SMAC Native screenshot test", 640, 384, SDL_WINDOW_HIDDEN,
                                     &window, &renderer)) {
        std::cerr << "synthetic screenshot window failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    const auto terrain = create_atlas_texture(renderer, terrain_image, {252, 253, 254, 255});
    const auto texture = create_atlas_texture(renderer, texture_image, {252, 253, 254, 255});
    const auto units = create_atlas_texture(renderer, unit_image, {252, 253, 254, 255});

    sc::WorldMap map(8, 6, true);
    for (auto& tile : map.tiles()) {
        tile.terrain = sc::Terrain::land;
        tile.climate = 0x60;
        tile.contour = 60;
        tile.visibility = 1U << 1U;
    }
    map.at({4, 2}).terrain = sc::Terrain::ocean;
    map.at({4, 2}).climate = 0x40;
    map.at({4, 2}).contour = 35;
    map.at({6, 2}).terrain = sc::Terrain::ocean;
    map.at({6, 2}).climate = 0x40;
    map.at({6, 2}).contour = 55;
    map.at({2, 2}).climate = 0xA0;
    map.at({2, 2}).contour = 100;
    map.at({3, 3}).improvements = sc::tile_fungus;
    map.at({5, 3}).visibility = 0;
    sc::GameState game(std::move(map));
    game.units().push_back(
        sc::make_unit(1, 1, {1, 3}, sc::Chassis::native_life, sc::Domain::land, game.rules()));
    const sc::TerrainGeometry geometry(game.map());
    const sc::MapProjection projection{80, 40, 1.0};

    SDL_SetRenderDrawColor(renderer, 7, 12, 18, 255);
    SDL_RenderClear(renderer);
    const auto visible = sc::visible_tiles(game.map(), projection, {0, 0, 640, 384});
    for (const auto pass :
         {TerrainPass::ground, TerrainPass::water, TerrainPass::objects, TerrainPass::fog})
        for (const auto& tile : visible)
            draw_tile(renderer, game, geometry, projection, tile, terrain, texture, false, 1, pass);
    const auto unit_surface = sc::project_terrain_tile(game.map(), geometry, projection, {1, 3});
    const auto unit_center =
        unit_surface.points[static_cast<std::size_t>(sc::TerrainVertex::center)];
    const sc::ScreenPoint unit_top{unit_center.x - 50.0, unit_center.y - 42.0};
    draw_named_region(renderer, units, sf::unit_atlas, "mind_worm", 0, unit_top, 100, 76);
    draw_terrain_outline(renderer, unit_surface, {250, 211, 55, 255});
    draw_terrain_outline(renderer,
                         sc::project_terrain_tile(game.map(), geometry, projection, {3, 3}),
                         {80, 220, 235, 255});

    auto* surface = SDL_RenderReadPixels(renderer, nullptr);
    bool passed = surface != nullptr;
    if (surface) {
        const auto hill = sc::project_terrain_tile(game.map(), geometry, projection, {2, 2});
        const auto water = sc::project_terrain_tile(game.map(), geometry, projection, {4, 2});
        const auto shallow_water =
            sc::project_terrain_tile(game.map(), geometry, projection, {6, 2});
        const auto fungus = sc::project_terrain_tile(game.map(), geometry, projection, {3, 3});
        const auto fog = sc::project_terrain_tile(game.map(), geometry, projection, {5, 3});
        passed &= hill.points[0].y < projection.tile_center({2, 2}).y;
        passed &= pixel_near(surface, hill.points[0], {132, 70, 42, 255});
        passed &= pixel_near(surface, water.points[0], {20, 90, 160, 255});
        passed &= pixel_near(surface, shallow_water.points[0], {19, 86, 157, 255});
        passed &= pixel_near(surface, fungus.points[0], {190, 35, 70, 255});
        passed &= pixel_near(surface, fog.points[0], {5, 9, 14, 255});
        passed &= pixel_near(surface, {unit_top.x + 50, unit_top.y + 38}, {245, 245, 220, 255});
        passed &= SDL_SaveBMP(surface, output.string().c_str());
        SDL_DestroySurface(surface);
    }
    SDL_DestroyTexture(units.texture);
    SDL_DestroyTexture(texture.texture);
    SDL_DestroyTexture(terrain.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (!passed)
        std::cerr << "synthetic screenshot pixels did not match the expected layer composition\n";
    return passed ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "smac-native " << SDL_MAJOR_VERSION << '.' << SDL_MINOR_VERSION << '.'
                  << SDL_MICRO_VERSION << " (SDL)\n";
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--synthetic-screenshot")
        return render_synthetic_screenshot(argv[2]);
    const bool acceptance = argc == 4 && std::string_view(argv[3]) == "--acceptance-check";
    const bool screenshot = argc == 5 && std::string_view(argv[3]) == "--screenshot";
    const bool benchmark = argc == 5 && std::string_view(argv[3]) == "--benchmark-frames";
    std::uint32_t benchmark_frames = 0;
    if (benchmark) {
        const std::string_view count = argv[4];
        const auto parsed =
            std::from_chars(count.data(), count.data() + count.size(), benchmark_frames);
        if (parsed.ec != std::errc{} || parsed.ptr != count.data() + count.size() ||
            benchmark_frames < 2 || benchmark_frames > 10'000) {
            std::cerr << "benchmark frame count must be between 2 and 10000\n";
            return 2;
        }
    }
    if ((argc != 3 && !acceptance && !screenshot && !benchmark) ||
        std::string_view(argv[1]) != "--data-dir") {
        std::cerr << "usage: smac-native --data-dir DIR [--screenshot FILE | "
                     "--benchmark-frames COUNT | --acceptance-check]\n";
        return 2;
    }
    const std::filesystem::path data_dir = argv[2];
    const auto report = sf::validate_data_directory(data_dir);
    if (!report.valid()) {
        std::cerr << "invalid data directory; run smac-tool verify-data\n";
        return 1;
    }
    const auto map_path = sf::find_case_insensitive(data_dir, "maps/xplanet.MP");
    auto parsed_map = sf::load_terran_map(*map_path);
    if (auto* error = std::get_if<sf::Error>(&parsed_map)) {
        std::cerr << error->message << '\n';
        return 1;
    }
    const auto rules_path = sf::find_case_insensitive(data_dir, "alphax.txt");
    auto parsed_rules = sf::load_rules(*rules_path);
    if (auto* error = std::get_if<sf::Error>(&parsed_rules)) {
        std::cerr << error->message << '\n';
        return 1;
    }
    auto terrain_image = load_image(*sf::find_case_insensitive(data_dir, "ter1.pcx"));
    auto texture_image = load_image(*sf::find_case_insensitive(data_dir, "texture.pcx"));
    auto unit_image = load_image(*sf::find_case_insensitive(data_dir, "Units.pcx"));
    if (!terrain_image || !texture_image || !unit_image)
        return 1;

    sc::GameState game(std::get<sf::TerranMap>(parsed_map).to_world_map(),
                       std::get<sf::ParsedRules>(parsed_rules).database);
    std::optional<sc::MapPosition> spawn;
    std::int64_t nearest_distance = std::numeric_limits<std::int64_t>::max();
    for (int y = 1; y < game.map().height() - 1; ++y) {
        for (int x = y & 1; x < game.map().width(); x += 2) {
            if (game.map().at({x, y}).terrain != sc::Terrain::land)
                continue;
            const auto dx = static_cast<std::int64_t>(x - game.map().width() / 2);
            const auto dy = static_cast<std::int64_t>(y - game.map().height() / 2);
            const auto distance = dx * dx + dy * dy;
            if (distance < nearest_distance) {
                spawn = sc::MapPosition{x, y};
                nearest_distance = distance;
            }
        }
    }
    if (!spawn) {
        std::cerr << "map has no land tile for debug unit\n";
        return 1;
    }
    game.units().push_back(
        sc::make_unit(1, 1, *spawn, sc::Chassis::native_life, sc::Domain::land, game.rules()));
    if (acceptance)
        return run_acceptance_check(game);
    const sc::TerrainGeometry terrain_geometry(game.map());

    if (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (screenshot || benchmark)
        flags |= SDL_WINDOW_HIDDEN;
    if (!SDL_CreateWindowAndRenderer("SMAC Native", 1280, 800, flags, &window, &renderer)) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_SetRenderVSync(renderer, benchmark ? 0 : 1);
    const auto terrain_texture =
        create_atlas_texture(renderer, *terrain_image, {252, 253, 254, 255});
    const auto texture_texture =
        create_atlas_texture(renderer, *texture_image, {252, 253, 254, 255});
    const auto unit_texture = create_atlas_texture(renderer, *unit_image, {252, 253, 254, 255});
    if (!terrain_texture.texture || !texture_texture.texture || !unit_texture.texture) {
        std::cerr << "failed creating indexed atlas texture: " << SDL_GetError() << '\n';
        return 1;
    }
    const auto font_path = sf::find_case_insensitive(data_dir, "ALPHC___.TTF");
    TTF_Font* font = font_path ? TTF_OpenFont(font_path->string().c_str(), 18.0F) : nullptr;

    int output_width = 1280;
    int output_height = 800;
    SDL_GetRenderOutputSize(renderer, &output_width, &output_height);
    sc::MapProjection projection;
    projection.origin_x = output_width / 2.0 - (spawn->x + 1) * projection.tile_width / 2.0;
    projection.origin_y = output_height / 2.0 - spawn->y * projection.tile_height / 2.0;
    const auto projected_spawn =
        sc::project_terrain_tile(game.map(), terrain_geometry, projection, *spawn);
    projection.origin_y +=
        output_height / 2.0 -
        projected_spawn.points[static_cast<std::size_t>(sc::TerrainVertex::center)].y;
    std::optional<sc::MapPosition> hovered = spawn;
    std::optional<sc::UnitId> selected_unit = 1;
    std::vector<sc::MapPosition> preview;
    std::deque<sc::Command> commands;
    std::deque<sc::Event> event_queue;
    std::optional<TimedEvent> active_event;
    Uint64 next_command_time = 0;
    std::string status = "Click a destination to preview; Enter confirms; Space ends turn";
    TextCache hud_cache;
    bool running = true;
    bool dragging = false;
    bool reveal_all = true;
    bool screenshot_attempted = false;
    bool screenshot_written = false;
    std::vector<Uint64> frame_times;
    frame_times.reserve(benchmark_frames);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL)
                SDL_ConvertEventToRenderCoordinates(renderer, &event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                switch (event.key.key) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_RETURN:
                    if (selected_unit && preview.size() > 1) {
                        commands.clear();
                        for (std::size_t i = 1; i < preview.size(); ++i)
                            commands.push_back(sc::MoveUnit{*selected_unit, preview[i]});
                        preview.clear();
                        status = "Movement confirmed";
                    }
                    break;
                case SDLK_SPACE:
                    commands.push_back(sc::EndTurn{});
                    preview.clear();
                    break;
                case SDLK_LEFT:
                    projection.origin_x += 60.0;
                    break;
                case SDLK_RIGHT:
                    projection.origin_x -= 60.0;
                    break;
                case SDLK_UP:
                    projection.origin_y += 48.0;
                    break;
                case SDLK_DOWN:
                    projection.origin_y -= 48.0;
                    break;
                case SDLK_EQUALS:
                    projection.zoom = std::min(2.5, projection.zoom + 0.25);
                    break;
                case SDLK_MINUS:
                    projection.zoom = std::max(0.5, projection.zoom - 0.25);
                    break;
                case SDLK_V:
                    reveal_all = !reveal_all;
                    break;
                default:
                    break;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_RIGHT)
                dragging = true;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
                dragging = false;
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (dragging) {
                    projection.origin_x += event.motion.xrel;
                    projection.origin_y += event.motion.yrel;
                }
                hovered = sc::screen_to_world(game.map(), terrain_geometry, projection,
                                              {event.motion.x, event.motion.y});
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                const auto picked = sc::screen_to_world(game.map(), terrain_geometry, projection,
                                                        {event.button.x, event.button.y});
                if (!picked)
                    continue;
                hovered = picked;
                const auto unit = std::find_if(
                    game.units().begin(), game.units().end(), [&](const sc::Unit& value) {
                        return value.position == *picked && !value.embarked_on;
                    });
                if (unit != game.units().end()) {
                    selected_unit = unit->id;
                    preview.clear();
                    status = "Unit selected";
                    continue;
                }
                if (!selected_unit)
                    continue;
                const auto selected =
                    std::find_if(game.units().begin(), game.units().end(),
                                 [&](const sc::Unit& value) { return value.id == *selected_unit; });
                if (selected == game.units().end())
                    continue;
                preview = sc::find_path(game, selected->id, *picked, selected->movement_remaining);
                if (preview.empty()) {
                    status = "No legal route within remaining movement points";
                } else {
                    status = "Route previewed; press Enter to move";
                }
            }
            if (event.type == SDL_EVENT_MOUSE_WHEEL)
                projection.zoom = std::clamp(projection.zoom + event.wheel.y * 0.25, 0.5, 2.5);
        }

        const auto now = SDL_GetTicks();
        if (!commands.empty() && now >= next_command_time) {
            auto events = game.apply(commands.front());
            commands.pop_front();
            bool rejected = false;
            for (auto& emitted : events) {
                if (const auto* reason = std::get_if<sc::CommandRejected>(&emitted)) {
                    status = reason->reason;
                    rejected = true;
                }
                event_queue.push_back(std::move(emitted));
            }
            if (rejected)
                commands.clear();
            next_command_time = now + 120;
        }
        if (active_event && now >= active_event->expires)
            active_event.reset();
        if (!active_event && !event_queue.empty()) {
            active_event = TimedEvent{std::move(event_queue.front()), now + 180};
            event_queue.pop_front();
        }

        const auto render_started = SDL_GetTicksNS();
        SDL_GetRenderOutputSize(renderer, &output_width, &output_height);
        SDL_SetRenderDrawColor(renderer, 7, 12, 18, 255);
        SDL_RenderClear(renderer);
        const auto visible = sc::visible_tiles(
            game.map(), projection,
            {0, 0, static_cast<double>(output_width), static_cast<double>(output_height)});
        for (const auto pass :
             {TerrainPass::ground, TerrainPass::water, TerrainPass::objects, TerrainPass::fog})
            for (const auto& tile : visible)
                draw_tile(renderer, game, terrain_geometry, projection, tile, terrain_texture,
                          texture_texture, reveal_all, 1, pass);
        for (const auto& tile : visible) {
            const auto surface =
                sc::project_terrain_tile(game.map(), terrain_geometry, projection, tile.unwrapped);
            if (std::find(preview.begin(), preview.end(), tile.position) != preview.end())
                draw_terrain_outline(renderer, surface, {238, 239, 180, 255});
            if (hovered && tile.position == *hovered)
                draw_terrain_outline(renderer, surface, {80, 220, 235, 255});
        }
        for (const auto& unit : game.units()) {
            if (unit.embarked_on)
                continue;
            for (const auto& tile : visible) {
                if (tile.position != unit.position)
                    continue;
                const auto surface = sc::project_terrain_tile(game.map(), terrain_geometry,
                                                              projection, tile.unwrapped);
                const auto center =
                    surface.points[static_cast<std::size_t>(sc::TerrainVertex::center)];
                const auto frame = static_cast<std::uint8_t>((now / 180) % 7);
                draw_named_region(renderer, unit_texture, sf::unit_atlas, "mind_worm", frame,
                                  {center.x - projection.tile_width * projection.zoom / 2.0,
                                   center.y - 42.0 * projection.zoom},
                                  static_cast<float>(projection.tile_width * projection.zoom),
                                  static_cast<float>(76.0 * projection.zoom));
                if (selected_unit && unit.id == *selected_unit)
                    draw_terrain_outline(renderer, surface, {250, 211, 55, 255});
            }
        }
        if (active_event) {
            if (const auto* moved = std::get_if<sc::UnitMoved>(&active_event->event)) {
                for (const auto& tile : visible)
                    if (tile.position == moved->to)
                        draw_terrain_outline(renderer,
                                             sc::project_terrain_tile(game.map(), terrain_geometry,
                                                                      projection, tile.unwrapped),
                                             {255, 255, 255, 210});
            }
        }

        const auto& unit = game.units().front();
        const auto info_position = hovered.value_or(unit.position);
        const auto& info_tile = game.map().at(info_position);
        auto hud = std::string("Turn ") + std::to_string(game.turn()) + "  Tile " +
                   std::to_string(info_position.x) + "," + std::to_string(info_position.y) + "  " +
                   terrain_description(info_tile) + "  MP " +
                   std::to_string(unit.movement_remaining) + "/" +
                   std::to_string(unit.movement_max) + "  |  " + status;
        update_text_cache(renderer, font, hud_cache, hud);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 5, 12, 16, 225);
        const SDL_FRect panel{8, 8, static_cast<float>(output_width - 16), 38};
        SDL_RenderFillRect(renderer, &panel);
        if (hud_cache.texture) {
            const SDL_FRect destination{16, 16, hud_cache.width, hud_cache.height};
            SDL_RenderTexture(renderer, hud_cache.texture, nullptr, &destination);
        }
        if (screenshot && !screenshot_attempted) {
            screenshot_attempted = true;
            if (auto* surface = SDL_RenderReadPixels(renderer, nullptr)) {
                screenshot_written = SDL_SaveBMP(surface, argv[4]);
                if (!screenshot_written)
                    std::cerr << "failed saving screenshot: " << SDL_GetError() << '\n';
                SDL_DestroySurface(surface);
            } else {
                std::cerr << "failed reading screenshot pixels: " << SDL_GetError() << '\n';
            }
            running = false;
        }
        SDL_RenderPresent(renderer);
        if (benchmark) {
            frame_times.push_back(SDL_GetTicksNS() - render_started);
            if (frame_times.size() >= benchmark_frames)
                running = false;
        }
    }

    bool benchmark_passed = true;
    if (benchmark) {
        std::sort(frame_times.begin(), frame_times.end());
        const auto median = frame_times[frame_times.size() / 2];
        const auto p95_index = (frame_times.size() * 95 + 99) / 100 - 1;
        const auto p95 = frame_times[p95_index];
        const auto maximum = frame_times.back();
        constexpr Uint64 m1_frame_budget_ns = 33'333'334;
        benchmark_passed = p95 <= m1_frame_budget_ns;
        const auto milliseconds = [](Uint64 nanoseconds) {
            return static_cast<double>(nanoseconds) / 1'000'000.0;
        };
        std::cout << "frames=" << frame_times.size() << " median_ms=" << milliseconds(median)
                  << " p95_ms=" << milliseconds(p95) << " max_ms=" << milliseconds(maximum)
                  << " budget_ms=" << milliseconds(m1_frame_budget_ns) << '\n';
    }

    hud_cache.reset();
    if (font)
        TTF_CloseFont(font);
    SDL_DestroyTexture(unit_texture.texture);
    SDL_DestroyTexture(texture_texture.texture);
    SDL_DestroyTexture(terrain_texture.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (screenshot && !screenshot_written)
        return 1;
    return benchmark_passed ? 0 : 1;
}
