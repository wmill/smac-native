#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace smac::core {
struct MapPosition { std::int32_t x{}; std::int32_t y{}; friend auto operator<=>(const MapPosition&, const MapPosition&) = default; };
enum class Terrain : std::uint8_t { ocean, land };
struct Tile { Terrain terrain{Terrain::ocean}; std::uint8_t climate{}; std::uint8_t contour{}; std::uint8_t region{}; std::uint32_t improvements{}; };
using FactionId = std::uint8_t;
using UnitId = std::uint32_t;
struct Faction { FactionId id{}; std::string name; };
struct Unit { UnitId id{}; FactionId faction{}; MapPosition position{}; std::int32_t movement_remaining{3}; std::int32_t movement_max{3}; };
struct RulesDatabase { std::int32_t road_movement_rate{3}; std::vector<std::string> section_names; };
}

