#pragma once
#include "smac/core/world_map.hpp"
#include "smac/formats/result.hpp"
#include <array>
#include <filesystem>
#include <vector>
namespace smac::formats {
struct MapLandmark { std::int32_t x{}; std::int32_t y{}; std::array<std::byte,32> raw_name{}; std::string name; };
struct RawMapTile { std::array<std::byte,44> raw{}; std::uint8_t climate{},contour{},site_owner{},region{},visibility{},rock_lock_user{},unknown1{};std::int8_t territory{};std::uint32_t improvements{},landmark{};std::array<std::uint32_t,7> visible_improvements{}; };
struct TerranMap { std::int32_t width{},height{};bool flat{};std::uint32_t seed{};std::array<std::byte,2724> legacy_header{};std::vector<MapLandmark> landmarks;std::vector<RawMapTile> tiles;std::vector<std::byte> abstract_regions; smac::core::WorldMap to_world_map() const; };
Result<TerranMap> parse_terran_map(std::span<const std::byte>);Result<TerranMap> load_terran_map(const std::filesystem::path&);
}

