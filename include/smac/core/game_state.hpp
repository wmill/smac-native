#pragma once
#include "smac/core/world_map.hpp"

#include <string_view>
#include <variant>

namespace smac::core {
struct MoveUnit {
    UnitId unit{};
    MapPosition destination{};
    friend bool operator==(const MoveUnit&, const MoveUnit&) = default;
};
struct EndTurn {
    friend bool operator==(const EndTurn&, const EndTurn&) = default;
};
using Command = std::variant<MoveUnit, EndTurn>;
struct UnitMoved {
    UnitId unit{};
    MapPosition from{};
    MapPosition to{};
    std::int32_t cost{};
    friend bool operator==(const UnitMoved&, const UnitMoved&) = default;
};
struct TurnAdvanced {
    std::uint32_t turn{};
    friend bool operator==(const TurnAdvanced&, const TurnAdvanced&) = default;
};
struct CommandRejected {
    std::string reason;
    friend bool operator==(const CommandRejected&, const CommandRejected&) = default;
};
using Event = std::variant<UnitMoved, TurnAdvanced, CommandRejected>;

enum class MoveBlock : std::uint8_t {
    none,
    invalid_position,
    not_adjacent,
    wrong_domain,
    hostile_occupied,
    transport_required,
    transport_full,
    zone_of_control,
};

struct MoveEvaluation {
    std::int32_t cost{-1};
    MoveBlock blocked{MoveBlock::none};
    [[nodiscard]] constexpr bool legal() const noexcept {
        return blocked == MoveBlock::none;
    }
};

class GameState {
  public:
    explicit GameState(WorldMap map, RulesDatabase rules = {})
        : map_(std::move(map)), rules_(std::move(rules)) {}
    WorldMap& map() noexcept {
        return map_;
    }
    const WorldMap& map() const noexcept {
        return map_;
    }
    std::vector<Unit>& units() noexcept {
        return units_;
    }
    const std::vector<Unit>& units() const noexcept {
        return units_;
    }
    [[nodiscard]] std::uint32_t turn() const noexcept {
        return turn_;
    }
    [[nodiscard]] const RulesDatabase& rules() const noexcept {
        return rules_;
    }
    std::vector<Event> apply(const Command& command);
    [[nodiscard]] std::uint64_t stable_hash() const noexcept;

  private:
    WorldMap map_;
    RulesDatabase rules_;
    std::vector<Unit> units_;
    std::uint32_t turn_{1};
};
[[nodiscard]] std::int32_t movement_allowance(const Unit& unit,
                                              const RulesDatabase& rules) noexcept;
[[nodiscard]] Unit make_unit(UnitId id, FactionId faction, MapPosition position, Chassis chassis,
                             Domain domain, const RulesDatabase& rules);
[[nodiscard]] MoveEvaluation evaluate_move(const GameState& state, const Unit& unit,
                                           MapPosition from, MapPosition to);
[[nodiscard]] std::string_view move_block_reason(MoveBlock block) noexcept;
} // namespace smac::core
