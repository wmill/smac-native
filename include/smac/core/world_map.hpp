#pragma once
#include "smac/core/types.hpp"
#include <optional>
#include <span>
#include <vector>

namespace smac::core {
class WorldMap {
public:
  WorldMap(std::int32_t width, std::int32_t height, bool wraps = true);
  [[nodiscard]] std::int32_t width() const noexcept { return width_; }
  [[nodiscard]] std::int32_t height() const noexcept { return height_; }
  [[nodiscard]] bool wraps() const noexcept { return wraps_; }
  [[nodiscard]] bool valid(MapPosition p) const noexcept;
  [[nodiscard]] std::optional<MapPosition> normalize(MapPosition p) const noexcept;
  [[nodiscard]] std::vector<MapPosition> neighbors(MapPosition p) const;
  [[nodiscard]] std::size_t index(MapPosition p) const;
  Tile& at(MapPosition p); const Tile& at(MapPosition p) const;
  [[nodiscard]] std::span<Tile> tiles() noexcept { return tiles_; }
private:
  std::int32_t width_{}; std::int32_t height_{}; bool wraps_{}; std::vector<Tile> tiles_;
};
}

