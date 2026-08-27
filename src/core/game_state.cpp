#include "smac/core/game_state.hpp"

#include <algorithm>
#include <cmath>

namespace smac::core {
namespace {
const Unit* transport_at(const GameState& state, const Unit& passenger, MapPosition position) {
    for (const auto& candidate : state.units()) {
        if (candidate.position != position || candidate.faction != passenger.faction ||
            candidate.domain != Domain::sea || candidate.transport_capacity == 0)
            continue;
        const auto passengers = static_cast<std::size_t>(
            std::count_if(state.units().begin(), state.units().end(), [&](const Unit& unit) {
                return unit.embarked_on && *unit.embarked_on == candidate.id;
            }));
        if (passengers < candidate.transport_capacity)
            return &candidate;
    }
    return nullptr;
}

bool hostile_unit_at(const GameState& state, const Unit& moving, MapPosition position) {
    return std::any_of(state.units().begin(), state.units().end(), [&](const Unit& unit) {
        return unit.id != moving.id && unit.position == position && unit.faction != moving.faction;
    });
}

bool enemy_zone_of_control(const GameState& state, const Unit& moving, MapPosition position) {
    return std::any_of(state.units().begin(), state.units().end(), [&](const Unit& unit) {
        if (unit.id == moving.id || unit.faction == moving.faction || unit.domain != Domain::land ||
            unit.embarked_on)
            return false;
        const auto neighbors = state.map().neighbors(unit.position);
        return std::find(neighbors.begin(), neighbors.end(), position) != neighbors.end();
    });
}

bool connected_diagonal(const WorldMap& map, MapPosition from, MapPosition to) {
    auto difference = std::abs(from.x - to.x);
    if (map.wraps())
        difference = std::min(difference, map.width() - difference);
    return difference == 1 && std::abs(from.y - to.y) == 1;
}
} // namespace

std::int32_t movement_allowance(const Unit& unit, const RulesDatabase& rules) noexcept {
    std::int32_t speed = 1;
    switch (unit.chassis) {
    case Chassis::speeder:
        speed = 2;
        break;
    case Chassis::hovertank:
        speed = 3;
        break;
    case Chassis::foil:
        speed = 4;
        break;
    case Chassis::cruiser:
        speed = 6;
        break;
    case Chassis::needlejet:
    case Chassis::copter:
    case Chassis::gravship:
        speed = 8;
        break;
    case Chassis::missile:
        speed = 12;
        break;
    case Chassis::infantry:
    case Chassis::native_life:
        break;
    }
    return speed * rules.road_movement_rate;
}

Unit make_unit(UnitId id, FactionId faction, MapPosition position, Chassis chassis, Domain domain,
               const RulesDatabase& rules) {
    Unit unit;
    unit.id = id;
    unit.faction = faction;
    unit.position = position;
    unit.domain = domain;
    unit.chassis = chassis;
    unit.native_life = chassis == Chassis::native_life;
    unit.movement_max = movement_allowance(unit, rules);
    unit.movement_remaining = unit.movement_max;
    return unit;
}

std::string_view move_block_reason(MoveBlock block) noexcept {
    switch (block) {
    case MoveBlock::none:
        return {};
    case MoveBlock::invalid_position:
        return "destination is outside the map or parity-invalid";
    case MoveBlock::not_adjacent:
        return "destination is not adjacent";
    case MoveBlock::wrong_domain:
        return "unit domain cannot enter destination terrain";
    case MoveBlock::hostile_occupied:
        return "combat is not available in this milestone";
    case MoveBlock::transport_required:
        return "land unit requires a friendly sea transport";
    case MoveBlock::transport_full:
        return "friendly sea transport is full";
    case MoveBlock::zone_of_control:
        return "enemy zone of control blocks this move";
    }
    return "unknown movement rejection";
}

MoveEvaluation evaluate_move(const GameState& state, const Unit& unit, MapPosition from,
                             MapPosition to) {
    if (!state.map().valid(from) || !state.map().valid(to))
        return {-1, MoveBlock::invalid_position};
    const auto neighbors = state.map().neighbors(from);
    if (std::find(neighbors.begin(), neighbors.end(), to) == neighbors.end())
        return {-1, MoveBlock::not_adjacent};
    if (hostile_unit_at(state, unit, to))
        return {-1, MoveBlock::hostile_occupied};

    const auto& source = state.map().at(from);
    const auto& destination = state.map().at(to);
    if (unit.domain == Domain::sea && destination.terrain != Terrain::ocean)
        return {-1, MoveBlock::wrong_domain};
    if (unit.domain == Domain::land && destination.terrain == Terrain::ocean) {
        bool friendly_transport = false;
        for (const auto& candidate : state.units())
            friendly_transport |= candidate.position == to && candidate.faction == unit.faction &&
                                  candidate.domain == Domain::sea &&
                                  candidate.transport_capacity != 0;
        if (!friendly_transport)
            return {-1, MoveBlock::transport_required};
        if (!transport_at(state, unit, to))
            return {-1, MoveBlock::transport_full};
    }
    if (unit.domain == Domain::land && !unit.native_life && source.terrain == Terrain::land &&
        enemy_zone_of_control(state, unit, from) && enemy_zone_of_control(state, unit, to))
        return {-1, MoveBlock::zone_of_control};

    const auto rate = state.rules().road_movement_rate;
    if (unit.domain == Domain::air)
        return {rate, MoveBlock::none};
    if (destination.terrain == Terrain::ocean) {
        if (unit.domain == Domain::sea && (destination.improvements & tile_fungus) != 0U &&
            destination.altitude() == 2 && !unit.native_life && !unit.xenoempathy)
            return {rate * 3, MoveBlock::none};
        return {rate, MoveBlock::none};
    }
    if (source.terrain == Terrain::ocean || unit.domain != Domain::land)
        return {rate, MoveBlock::none};
    if ((source.improvements & (tile_mag_tube | tile_base)) != 0U &&
        (destination.improvements & (tile_mag_tube | tile_base)) != 0U && unit.faction != 0)
        return {0, MoveBlock::none};
    if ((source.improvements & (tile_road | tile_base)) != 0U &&
        (destination.improvements & (tile_road | tile_base)) != 0U && unit.faction != 0)
        return {1, MoveBlock::none};
    if ((destination.improvements & tile_fungus) != 0U &&
        (unit.native_life || unit.xenoempathy || unit.faction == 0))
        return {1, MoveBlock::none};
    if ((source.improvements & tile_river) != 0U && (destination.improvements & tile_river) != 0U &&
        connected_diagonal(state.map(), from, to) && unit.faction != 0)
        return {1, MoveBlock::none};
    if (unit.chassis == Chassis::hovertank || unit.antigrav)
        return {rate, MoveBlock::none};

    auto cost = rate;
    if (destination.rockiness() == Rockiness::rocky)
        cost += rate;
    if ((destination.improvements & tile_forest) != 0U)
        cost += rate;
    if ((destination.improvements & tile_fungus) != 0U)
        cost += rate * 2;
    return {cost, MoveBlock::none};
}

std::vector<Event> GameState::apply(const Command& command) {
    return std::visit(
        [this](const auto& value) -> std::vector<Event> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, EndTurn>) {
                ++turn_;
                for (auto& unit : units_) {
                    unit.movement_max = movement_allowance(unit, rules_);
                    unit.movement_remaining = unit.movement_max;
                }
                return {TurnAdvanced{turn_}};
            } else {
                auto it = std::find_if(units_.begin(), units_.end(),
                                       [&](const Unit& unit) { return unit.id == value.unit; });
                if (it == units_.end())
                    return {CommandRejected{"unknown unit"}};
                const auto destination = map_.normalize(value.destination);
                if (!destination)
                    return {CommandRejected{
                        std::string(move_block_reason(MoveBlock::invalid_position))}};
                const auto evaluation = evaluate_move(*this, *it, it->position, *destination);
                if (!evaluation.legal())
                    return {CommandRejected{std::string(move_block_reason(evaluation.blocked))}};
                if (evaluation.cost > it->movement_remaining)
                    return {CommandRejected{"insufficient movement points"}};
                const auto from = it->position;
                const auto moving_transport =
                    it->domain == Domain::sea && it->transport_capacity > 0;
                const auto transport_id = it->id;
                it->position = *destination;
                it->movement_remaining -= evaluation.cost;
                if (it->domain == Domain::land && map_.at(*destination).terrain == Terrain::ocean)
                    it->embarked_on = transport_at(*this, *it, *destination)->id;
                else if (map_.at(*destination).terrain == Terrain::land)
                    it->embarked_on.reset();
                std::vector<Event> events{UnitMoved{it->id, from, *destination, evaluation.cost}};
                if (moving_transport) {
                    for (auto& passenger : units_) {
                        if (passenger.embarked_on && *passenger.embarked_on == transport_id) {
                            const auto passenger_from = passenger.position;
                            passenger.position = *destination;
                            events.push_back(
                                UnitMoved{passenger.id, passenger_from, *destination, 0});
                        }
                    }
                }
                return events;
            }
        },
        command);
}

std::uint64_t GameState::stable_hash() const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    auto add = [&](std::uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            hash ^= (value & 255U);
            hash *= 1099511628211ULL;
            value >>= 8;
        }
    };
    auto add_string = [&](std::string_view value) {
        add(value.size());
        for (char raw : value) {
            const auto byte = static_cast<unsigned char>(raw);
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
    };
    add(2); // Hash schema version.
    add(turn_);
    add(static_cast<std::uint64_t>(map_.width()));
    add(static_cast<std::uint64_t>(map_.height()));
    add(static_cast<std::uint64_t>(static_cast<std::int64_t>(map_.sea_level())));
    add(map_.wraps());
    for (const auto& tile : map_.tiles()) {
        add(static_cast<std::uint8_t>(tile.terrain));
        add(tile.climate);
        add(tile.contour);
        add(tile.site);
        add(tile.occupant_owner);
        add(tile.region);
        add(tile.visibility);
        add(tile.rockiness_code);
        add(tile.locked_by);
        add(tile.worked_by);
        add(static_cast<std::uint8_t>(tile.territory));
        add(tile.improvements);
        add(tile.landmark);
        for (const auto visible : tile.visible_improvements)
            add(visible);
    }
    add(static_cast<std::uint32_t>(rules_.road_movement_rate));
    add(rules_.section_names.size());
    for (const auto& section : rules_.section_names)
        add_string(section);
    add(units_.size());
    for (const auto& unit : units_) {
        add(unit.id);
        add(unit.faction);
        add(static_cast<std::uint32_t>(unit.position.x));
        add(static_cast<std::uint32_t>(unit.position.y));
        add(static_cast<std::uint32_t>(unit.movement_remaining));
        add(static_cast<std::uint32_t>(unit.movement_max));
        add(static_cast<std::uint8_t>(unit.domain));
        add(static_cast<std::uint8_t>(unit.chassis));
        add(unit.native_life);
        add(unit.antigrav);
        add(unit.xenoempathy);
        add(unit.transport_capacity);
        add(unit.embarked_on.has_value());
        if (unit.embarked_on)
            add(*unit.embarked_on);
    }
    return hash;
}
} // namespace smac::core
