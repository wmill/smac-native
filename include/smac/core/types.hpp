#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace smac::core {
struct MapPosition {
    std::int32_t x{};
    std::int32_t y{};
    friend auto operator<=>(const MapPosition&, const MapPosition&) = default;
};
enum class Terrain : std::uint8_t { ocean, land };
enum class Rainfall : std::uint8_t { arid, moist, rainy, invalid };
enum class Rockiness : std::uint8_t { flat, rolling, rocky, invalid };

enum TileFlag : std::uint32_t {
    tile_base = 0x1,
    tile_unit = 0x2,
    tile_road = 0x4,
    tile_mag_tube = 0x8,
    tile_mine = 0x10,
    tile_fungus = 0x20,
    tile_solar = 0x40,
    tile_river = 0x80,
    tile_riverbed = 0x100,
    tile_resource = 0x400,
    tile_bunker = 0x800,
    tile_monolith = 0x2000,
    tile_farm = 0x8000,
    tile_energy_resource = 0x10000,
    tile_mineral_resource = 0x20000,
    tile_airbase = 0x40000,
    tile_soil_enricher = 0x80000,
    tile_forest = 0x200000,
    tile_condenser = 0x400000,
    tile_echelon_mirror = 0x800000,
    tile_thermal_borehole = 0x1000000,
    tile_supply_pod = 0x10000000,
    tile_nutrient_resource = 0x20000000,
    tile_sensor = 0x80000000,
};
struct Tile {
    Terrain terrain{Terrain::ocean};
    std::uint8_t climate{};
    std::uint8_t contour{};
    std::uint8_t site{};
    std::uint8_t occupant_owner{0xF};
    std::uint8_t region{};
    std::uint8_t visibility{};
    std::uint8_t rockiness_code{};
    std::uint8_t locked_by{};
    std::uint8_t worked_by{};
    std::int8_t territory{-1};
    std::uint32_t improvements{};
    std::uint32_t landmark{};
    std::array<std::uint32_t, 7> visible_improvements{};

    [[nodiscard]] constexpr std::uint8_t altitude() const noexcept {
        return climate >> 5U;
    }
    [[nodiscard]] constexpr Rainfall rainfall() const noexcept {
        return static_cast<Rainfall>((climate >> 3U) & 3U);
    }
    [[nodiscard]] constexpr std::uint8_t temperature() const noexcept {
        return climate & 7U;
    }
    [[nodiscard]] constexpr Rockiness rockiness() const noexcept {
        return static_cast<Rockiness>(rockiness_code);
    }
};
using FactionId = std::uint8_t;
using UnitId = std::uint32_t;
enum class Domain : std::uint8_t { land, sea, air };
enum class Chassis : std::uint8_t {
    infantry,
    speeder,
    hovertank,
    foil,
    cruiser,
    needlejet,
    copter,
    gravship,
    missile,
    native_life,
};
struct Faction {
    FactionId id{};
    std::string name;
};
struct Unit {
    UnitId id{};
    FactionId faction{};
    MapPosition position{};
    std::int32_t movement_remaining{3};
    std::int32_t movement_max{3};
    Domain domain{Domain::land};
    Chassis chassis{Chassis::infantry};
    bool native_life{};
    bool antigrav{};
    bool xenoempathy{};
    std::uint8_t transport_capacity{};
    std::optional<UnitId> embarked_on;
};
struct RulesDatabase {
    std::int32_t road_movement_rate{3};
    std::vector<std::string> section_names;
    friend bool operator==(const RulesDatabase&, const RulesDatabase&) = default;
};
} // namespace smac::core
