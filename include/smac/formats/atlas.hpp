#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace smac::formats {
struct AtlasRect {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
    friend bool operator==(const AtlasRect&, const AtlasRect&) = default;
};

struct AtlasRegion {
    std::string_view name;
    AtlasRect source;
    std::uint8_t transparent_index{};
    std::uint8_t frames{1};
    std::int32_t frame_stride_x{};
    std::int32_t frame_stride_y{};
    std::uint8_t frame_columns{1};
};

// The cyan guide pixels around cells are excluded. Coordinates describe the stock SMACX sheets.
inline constexpr std::array terrain_atlas{
    AtlasRegion{"resource_nutrient_water", {1, 253, 100, 62}, 253, 2, 101, 0},
    AtlasRegion{"resource_nutrient_land", {203, 253, 100, 62}, 253, 2, 101, 0},
    AtlasRegion{"resource_mineral_water", {1, 316, 100, 62}, 253, 2, 101, 0},
    AtlasRegion{"resource_mineral_land", {203, 316, 100, 62}, 253, 2, 101, 0},
    AtlasRegion{"resource_energy_water", {1, 379, 100, 62}, 253, 2, 101, 0},
    AtlasRegion{"resource_energy_land", {203, 379, 100, 62}, 253, 2, 101, 0},
    AtlasRegion{"monolith", {304, 0, 100, 62}, 253},
    AtlasRegion{"mine_water", {506, 64, 100, 62}, 253},
    AtlasRegion{"mine_land", {607, 64, 100, 62}, 253},
    AtlasRegion{"solar_water", {506, 127, 100, 62}, 253},
    AtlasRegion{"solar_land", {607, 127, 100, 62}, 253},
    AtlasRegion{"kelp_farm", {607, 190, 100, 62}, 253},
    AtlasRegion{"geothermal", {822, 190, 100, 62}, 253},
    AtlasRegion{"condenser", {506, 253, 100, 62}, 253},
    AtlasRegion{"echelon_mirror", {607, 253, 100, 62}, 253},
    AtlasRegion{"thermal_borehole", {708, 253, 100, 62}, 253},
    AtlasRegion{"uranium", {822, 253, 100, 62}, 253},
    AtlasRegion{"bunker", {506, 316, 100, 62}, 253},
    AtlasRegion{"airbase", {607, 316, 100, 62}, 253},
    AtlasRegion{"sensor", {708, 316, 100, 62}, 253},
    AtlasRegion{"supply_pod_water", {418, 379, 100, 62}, 253, 3, 202, 0},
    AtlasRegion{"supply_pod_land", {519, 379, 100, 62}, 253, 3, 202, 0},
    AtlasRegion{"soil_enricher", {822, 453, 100, 62}, 253, 4, 0, 63},
    AtlasRegion{"farm_land", {923, 453, 100, 62}, 253, 4, 0, 63},
};

inline constexpr std::array unit_atlas{
    AtlasRegion{"mind_worm", {1, 79, 100, 76}, 255, 7, 102, 0},
};

inline constexpr std::array texture_atlas{
    AtlasRegion{"rocks", {1, 1, 56, 56}, 255, 4, 57, 0},
    AtlasRegion{"dunes", {229, 1, 56, 56}, 255},
    AtlasRegion{"water_deep", {280, 79, 56, 56}, 255},
    AtlasRegion{"water_surface", {280, 136, 56, 56}, 255},
    AtlasRegion{"arid", {1, 58, 56, 56}, 255},
    AtlasRegion{"moist", {1, 115, 56, 56}, 255, 16, 57, 57, 4},
    AtlasRegion{"rainy", {1, 343, 56, 56}, 255, 16, 57, 57, 4},
    AtlasRegion{"forest", {526, 6, 56, 56}, 255, 16, 57, 57, 4},
    AtlasRegion{"river", {280, 259, 56, 56}, 255, 16, 57, 57, 4},
    AtlasRegion{"jungle", {526, 259, 56, 56}, 255, 16, 57, 57, 4},
    AtlasRegion{"fungus_land", {280, 516, 56, 56}, 255, 16, 57, 57, 4},
    AtlasRegion{"fungus_water", {508, 516, 56, 56}, 255, 16, 57, 57, 4},
    AtlasRegion{"farm", {775, 219, 56, 56}, 255, 9, 57, 57, 3},
    AtlasRegion{"road", {775, 395, 56, 56}, 255, 9, 57, 57, 3},
    AtlasRegion{"mag_tube", {775, 566, 56, 56}, 255, 9, 57, 57, 3},
};

[[nodiscard]] constexpr AtlasRect atlas_frame(const AtlasRegion& region,
                                              std::uint8_t frame) noexcept {
    const auto bounded = static_cast<std::uint8_t>(frame % region.frames);
    if (region.frame_stride_x != 0 && region.frame_stride_y != 0) {
        return {region.source.x + (bounded % region.frame_columns) * region.frame_stride_x,
                region.source.y + (bounded / region.frame_columns) * region.frame_stride_y,
                region.source.width, region.source.height};
    }
    return {region.source.x + bounded * region.frame_stride_x,
            region.source.y + bounded * region.frame_stride_y, region.source.width,
            region.source.height};
}

[[nodiscard]] constexpr const AtlasRegion* find_region(std::span<const AtlasRegion> regions,
                                                       std::string_view name) {
    for (const auto& region : regions)
        if (region.name == name)
            return &region;
    return nullptr;
}
} // namespace smac::formats
