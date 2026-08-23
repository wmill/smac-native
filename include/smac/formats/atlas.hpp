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
};

struct AtlasRegion {
    std::string_view name;
    AtlasRect source;
    std::uint8_t transparent_index{};
    std::uint8_t frames{1};
    std::int32_t frame_stride_x{};
    std::int32_t frame_stride_y{};
};

// The cyan guide pixels around cells are excluded. Coordinates describe the stock SMACX sheets.
inline constexpr std::array terrain_atlas{
    AtlasRegion{"resource_nutrient", {1, 254, 100, 62}, 253, 4, 101, 0},
    AtlasRegion{"resource_mineral", {1, 317, 100, 62}, 253, 4, 101, 0},
    AtlasRegion{"resource_energy", {1, 380, 100, 62}, 253, 4, 101, 0},
    AtlasRegion{"monolith", {304, 1, 100, 62}, 253},
    AtlasRegion{"mine", {506, 64, 100, 62}, 253},
    AtlasRegion{"solar_collector", {607, 127, 100, 62}, 253},
    AtlasRegion{"kelp_farm", {607, 190, 100, 62}, 253},
    AtlasRegion{"fungus", {607, 253, 100, 62}, 253},
    AtlasRegion{"bunker", {506, 316, 100, 62}, 253},
    AtlasRegion{"airbase", {607, 316, 100, 62}, 253},
    AtlasRegion{"sensor", {708, 316, 100, 62}, 253},
    AtlasRegion{"supply_pod", {418, 379, 100, 62}, 253, 6, 101, 0},
    AtlasRegion{"farm", {822, 443, 100, 62}, 253, 4, 0, 63},
    AtlasRegion{"soil_enricher", {923, 443, 100, 62}, 253, 4, 0, 63},
    AtlasRegion{"rocky", {478, 631, 100, 62}, 253, 6, 101, 0},
    AtlasRegion{"flat_arid", {1, 579, 100, 62}, 253},
    AtlasRegion{"flat_moist", {102, 579, 100, 62}, 253},
    AtlasRegion{"flat_rainy", {203, 579, 100, 62}, 253},
    AtlasRegion{"ocean_shelf", {1, 643, 100, 62}, 253},
    AtlasRegion{"ocean", {102, 643, 100, 62}, 253},
    AtlasRegion{"ocean_trench", {203, 643, 100, 62}, 253},
};

inline constexpr std::array unit_atlas{
    AtlasRegion{"unity_rover", {1, 1, 100, 76}, 255},
    AtlasRegion{"spore_launcher", {1, 79, 100, 76}, 255, 5, 102, 0},
    AtlasRegion{"isle_of_the_deep", {1, 157, 100, 76}, 255, 3, 102, 0},
    AtlasRegion{"mind_worm", {1, 235, 100, 76}, 255, 5, 102, 0},
    AtlasRegion{"sealurk", {1, 313, 100, 76}, 255, 5, 102, 0},
};

[[nodiscard]] constexpr const AtlasRegion* find_region(std::span<const AtlasRegion> regions,
                                                       std::string_view name) {
    for (const auto& region : regions)
        if (region.name == name)
            return &region;
    return nullptr;
}
} // namespace smac::formats
