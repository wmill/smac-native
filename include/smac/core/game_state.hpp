#pragma once
#include "smac/core/world_map.hpp"
#include <variant>

namespace smac::core {
struct MoveUnit { UnitId unit{}; MapPosition destination{}; };
struct EndTurn {};
using Command = std::variant<MoveUnit, EndTurn>;
struct UnitMoved { UnitId unit{}; MapPosition from{}; MapPosition to{}; std::int32_t cost{}; };
struct TurnAdvanced { std::uint32_t turn{}; };
struct CommandRejected { std::string reason; };
using Event = std::variant<UnitMoved, TurnAdvanced, CommandRejected>;

class GameState {
public:
  explicit GameState(WorldMap map) : map_(std::move(map)) {}
  WorldMap& map() noexcept { return map_; } const WorldMap& map() const noexcept { return map_; }
  std::vector<Unit>& units() noexcept { return units_; } const std::vector<Unit>& units() const noexcept { return units_; }
  [[nodiscard]] std::uint32_t turn() const noexcept { return turn_; }
  std::vector<Event> apply(const Command& command);
  [[nodiscard]] std::uint64_t stable_hash() const noexcept;
private:
  WorldMap map_; std::vector<Unit> units_; std::uint32_t turn_{1};
};
int movement_cost(const WorldMap& map, MapPosition from, MapPosition to);
}

